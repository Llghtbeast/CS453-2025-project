#include "txn.h"

// ------- txn_end helper -------

static bool txn_lock(struct set_t *ws);

/**
 * @return Whether tx->rv + 1 == wv
 */
static bool txn_set_wv(struct txn_t *txn, version_clock_t wv);

static bool txn_validate_r_set(struct set_t *rs, int rv);

static void txn_w_commit(struct set_t *ws);

static void txn_unlock(struct set_t *ws, write_entry_t *last, int wv, bool committed);

// ============================================= global functions =============================================

struct txn_t *txn_create(bool is_ro, int rv) {
    struct txn_t *txn = malloc(sizeof(struct txn_t));
    if (unlikely(!txn)) return NULL;

    txn->is_ro = is_ro;
    txn->rv = rv;
    txn->wv = -1;

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

bool txn_read(struct txn_t *txn, void const *source, size_t size, void *target) {
    // Check if transaction is read-only
    if (txn->is_ro) {
        memcpy(target, source, size);
    }
    else {
        
        if (set_contains(txn->w_set, target))
        // write_set_get will write to target if the write set contains the target
        if (!write_set_get(txn->w_set, target, size, source)) {
            // If write set does not contain the source, write
            memcpy(target, source, size);
        }
    }
    
    // Post-validation: check if lock is free and has not changed. Else abort
    struct v_lock_t *lock = &(((struct segment_node_t *) ((uintptr_t) source - sizeof(struct segment_node_t)))->lock);
    int version = v_lock_version(lock);
    if (version == LOCKED || version > txn->rv) return ABORT;

    // If transaction is read-write, add address to read set
    if (!txn->is_ro) { set_add(txn->r_set, source); }
    return true;
}

bool txn_write(struct txn_t *txn, void const *source, size_t size, void *target) {
    return w_set_add(txn->w_set, source, size, target);
}

bool txn_end(struct txn_t *txn, struct region_t *region) {
    // If transaction is read only or no writes occured (effectively read-only), directly commit
    if (txn->is_ro || txn->w_set->count == 0) return COMMIT;

    // If transaction is read write, perform additional steps
    // Lock the write-set (Go through region LL and lock if they are in the write set)
    if (!txn_lock(txn->w_set)) {
        return ABORT;
    }

    // Increment global version clock
    version_clock_t wv = region_update_version_clock(region);
    
    if (!txn_set_wv(txn, wv)) {
        // Validate the read set
        if (!txn_validate_r_set(txn->r_set, txn->rv)){
            txn_unlock(txn->w_set, NULL, ABORTED_TXN, false);
            return ABORT;
        } 
    }
    
    // Commit
    txn_w_commit(txn->w_set);
    
    // Release locks and update their write version
    txn_unlock(txn->w_set, NULL, wv, true);
    return COMMIT;
}

// ============================================= static functions implementation =============================================
static bool txn_lock(struct set_t *ws) {
    for (size_t i = 0; i < ws->count; i++) {
        // Extract corresponding memory location
        write_entry_t *entry = ws->entries[i];
        struct segment_node_t *node = ((uintptr_t) entry->base.target + sizeof(struct segment_node_t));

        if (!v_lock_acquire(&node->lock)) {
            // Failed to acquire lock -> unlock acquired locks & abort transaction
            txn_unlock(ws, entry, ABORTED_TXN, false);
            return false;
        }
    }
    return true;
}

static bool txn_set_wv(struct txn_t *txn, version_clock_t wv) {
    txn->wv = wv;
    return txn->rv+1 == wv;
}

static bool txn_validate_r_set(struct set_t *rs, int rv) {
    // Iterate through read set
    for (size_t i = 0; i < rs->count; i++) {
        read_entry_t *entry = rs->entries[i];
        struct segment_node_t *node = ((uintptr_t) entry->target + sizeof(struct segment_node_t));

        int lv = v_lock_version(&node->lock);
        // If lock is not acquirable or the lock version clock is higher than tx->rv, abort.
        if (lv & 0x1 || (lv >> 1) > rv) {
            return false;
        }
    }
    return true;
}

static void txn_w_commit(struct set_t *ws) {
    // Iterate through write set
    for (size_t i = 0; i < ws->count; i++) {
        write_entry_t *we = ws->entries[i];
        memcpy(we->base.target, we->data, we->size);
    }
}

static void txn_unlock(struct set_t *ws, write_entry_t *last, int wv, bool committed) {
    for (size_t i = 0; i < ws->count; i++) {
        // Extract corresponding memory location
        write_entry_t *entry = ws->entries[i];
        if (entry == last) return;      // only unlock locked memory regions
        
        struct segment_node_t *node = ((uintptr_t) entry->base.target + sizeof(struct segment_node_t));
        // If transaction has committed, update lock versions
        if (committed) {
            v_lock_update(&(node->lock), wv);
        }
        v_lock_release(&(node->lock));
    }
}