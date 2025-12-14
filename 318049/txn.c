#include "txn.h"

// ------- txn_end helper -------

static bool txn_lock(struct region_t *region, struct set_t *ws);

/**
 * @return Whether tx->rv + 1 == wv
 */
static bool txn_set_wv(struct txn_t *txn, int wv);

static bool txn_validate_r_set(struct region_t *region, struct set_t *rs, int rv);

static void txn_w_commit(struct set_t *ws);

static void txn_unlock(struct region_t *region, struct set_t *ws, write_entry_t *last, int wv, bool committed);

// ============================================= global functions =============================================

struct txn_t *txn_create(bool is_ro, int rv) {
    struct txn_t *txn = malloc(sizeof(struct txn_t));
    if (unlikely(!txn)) return NULL;

    txn->is_ro = is_ro;
    txn->rv = rv;
    txn->wv = INVALID;       // invalid write version

    txn->r_set = set_init(false);
    if (!txn->r_set) {
        free(txn);
        return NULL;
    }
    txn->w_set = set_init(true);
    if (!txn->w_set) {
        set_free(txn->w_set);
        free(txn);
        return NULL;
    }

    return txn;
}

void txn_free(struct txn_t *txn) {
    if (unlikely(!txn)) return;
    
    set_free(txn->r_set);
    set_free(txn->w_set);
    free(txn);
}

bool txn_is_ro(struct txn_t * txn) {
    return txn->is_ro;
}

bool txn_read(struct txn_t *txn, struct region_t *region, void const *source, size_t size, void *target) {
    size_t word_size = region->align;
   
    for (size_t i = 0; i < size; i += word_size) {
        void *source_addr = (char *)source +i;
        void *target_addr = (char *)target +i;

        if (txn->is_ro) {
            // Check if address has been written to during this trasaction
            write_entry_t *entry = (write_entry_t *) set_get(txn->w_set, source_addr);
            if (entry) {
                memcpy(target_addr, entry->data, word_size);
                continue;
            }
        }

        // Determine lock associated to shared memory region
        v_lock_t *lock = region_get_memory_lock(region, source_addr);

        // Verify lock is free (without acquiring it)
        int lv_pre = v_lock_version(lock);
        if (lv_pre == LOCKED || lv_pre > txn->rv) return ABORT; // Abort (maybe should cleanup transaction directly?)

        memcpy(target_addr, source_addr, word_size);

        // Lock post-validation
        int lv_post = v_lock_version(lock);
        if (lv_post == LOCKED || lv_post != lv_pre) return ABORT; // Abort (maybe should cleanup transaction directly?)

        if (txn->is_ro) {
            // Add to read set
            if (unlikely(!r_set_add(txn->r_set, source_addr))) {
                return ABORT;
            }
        }
    }
    return COMMIT;
}

bool txn_write(struct txn_t *txn, struct region_t *region, void const *source, size_t size, void *target) {
    size_t word_size = region->align;
   
    for (size_t i = 0; i < size; i += word_size) {
        void *source_addr = (char *)source +i;
        void *target_addr = (char *)target +i;

        // Add to write set
        if (unlikely(!w_set_add(txn->w_set, source_addr, word_size, target_addr))) {
            return ABORT;
        }
    }
    return COMMIT;
}

bool txn_end(struct txn_t *txn, struct region_t *region) {
    // If transaction is read only or no writes occured (effectively read-only), directly commit
    if (txn->is_ro || txn->w_set->count == 0) return COMMIT;

    // If transaction is read write, perform additional steps
    // Lock the write-set (Go through region LL and lock if they are in the write set)
    if (!txn_lock(region, txn->w_set)) {
        return ABORT;
    }

    // Increment global version clock
    int wv = region_update_version_clock(region);
    
    if (!txn_set_wv(txn, wv)) {
        // Validate the read set
        if (!txn_validate_r_set(region, txn->r_set, txn->rv)){
            txn_unlock(region, txn->w_set, NULL, INVALID, false);
            return ABORT;
        } 
    }
    
    // Commit
    txn_w_commit(txn->w_set);
    
    // Release locks and update their write version
    txn_unlock(region, txn->w_set, NULL, wv, true);
    return COMMIT;
}

// ============================================= static functions implementation =============================================
static bool txn_lock(struct region_t *region, struct set_t *ws) {
    for (size_t i = 0; i < ws->count; i++) {
        // Extract corresponding memory location
        write_entry_t *entry = (write_entry_t *)ws->entries[i];

        v_lock_t *lock = region_get_memory_lock(region, entry->base.target);
        if (!v_lock_acquire(lock)) {
            // Failed to acquire lock -> unlock acquired locks & abort transaction
            txn_unlock(region, ws, entry, INVALID, false);
            return ABORT;
        }
    }
    return true;
}

static bool txn_set_wv(struct txn_t *txn, int wv) {
    txn->wv = wv;
    return txn->rv+1 == wv;
}

static bool txn_validate_r_set(struct region_t *region, struct set_t *rs, int rv) {
    // Iterate through read set
    for (size_t i = 0; i < rs->count; i++) {
        read_entry_t *entry = rs->entries[i];
        v_lock_t *lock = region_get_memory_lock(region, entry->target);

        int lv = v_lock_version(lock);
        // If lock is not acquirable or the lock version clock is higher than tx->rv, abort.
        if (lv == LOCKED || lv > rv) {
            return false;
        }
    }
    return true;
}

static void txn_w_commit(struct set_t *ws) {
    // Iterate through write set and write values
    for (size_t i = 0; i < ws->count; i++) {
        write_entry_t *we = (write_entry_t *) ws->entries[i];
        memcpy(we->base.target, we->data, we->size);
    }
}

static void txn_unlock(struct region_t *region, struct set_t *ws, write_entry_t *last, int wv, bool committed) {    
    for (size_t i = 0; i < ws->count; i++) {
        // Extract corresponding memory location
        write_entry_t *entry = (write_entry_t *) ws->entries[i];
        if (entry == last) return;      // only unlock locked memory regions
        
        v_lock_t *lock = region_get_memory_lock(region, entry->base.target);
        // If transaction has committed, update lock versions
        if (committed) {
            v_lock_update(lock, wv);
        }
        v_lock_release(lock);
    }
}