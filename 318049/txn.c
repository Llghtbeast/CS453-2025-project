#include "txn.h"
#include "shared.h"

// ------- txn_end helper -------

static bool txn_lock(struct txn_t *txn, struct region_t *region, uint64_t *lock_field);

/**
 * @return Whether tx->rv + 1 == wv
 */
static bool txn_set_wv(struct txn_t *txn, int wv);

static bool txn_validate_r_set(struct region_t *region, struct set_t *rs, int rv);

static void txn_w_commit(struct set_t *ws);

static void txn_unlock(struct txn_t *txn, struct region_t *region, uint64_t *lock_field, size_t last, bool committed);

// ============================================= global functions =============================================

struct txn_t *txn_create(struct region_t *region, bool is_ro) {
    pthread_rwlock_rdlock(&region->free_lock);      // Stops another transaction from freeing any shared memory regions

    struct txn_t *txn = malloc(sizeof(struct txn_t));
    if (unlikely(!txn)) {
        LOG_TEST("txn_create: memory allocation for transaction failed!\n");
        return NULL;
    }

    txn->is_ro = is_ro;
    txn->rv = global_clock_load(&region->version_clock);
    txn->wv = INVALID;       // invalid write version

    txn->r_set = set_init(false, region->align);
    if (unlikely(!txn->r_set)) {
        LOG_TEST("txn_create: read set_init failed!\n");
        free(txn);
        return NULL;
    }
    txn->w_set = set_init(true, region->align);
    if (unlikely(!txn->w_set)) {
        LOG_TEST("txn_create: write set_init failed!\n");
        set_free(txn->w_set);
        free(txn);
        return NULL;
    }

    txn->to_free = NULL;
    txn->to_free_count = 0;

    // LOG_NOTE("txn_create: transaction %lu created.\n", (tx_t) txn);
    return txn;
}

void txn_destroy(struct txn_t *txn, struct region_t *region) {
    // Release lock to allow a transaction to free a batch of shared memory segments
    pthread_rwlock_unlock(&region->free_lock);      
    
    if (unlikely(!txn)) return;
    
    set_free(txn->r_set);
    set_free(txn->w_set);
    free(txn);
}

bool txn_schedule_to_free(struct txn_t *txn, void *target) {
    txn->to_free = realloc(txn->to_free, (txn->to_free_count + 1) * sizeof(void *));
    if (unlikely(!txn->to_free)) return ABORT;

    txn->to_free[txn->to_free_count++] = target;
    return SUCCESS;
}

bool txn_is_ro(struct txn_t *txn) {
    return txn->is_ro;
}

bool txn_read(struct txn_t *txn, struct region_t *region, void const *source, size_t size, void *target) {
    size_t word_size = region->align;
   
    for (size_t i = 0; i < size; i += word_size) {
        void *source_addr = (char *)source +i;
        void *target_addr = (char *)target +i;

        if (unlikely(!txn->is_ro)) {
            // Check if address has been written to during this trasaction
            write_entry_t *entry = (write_entry_t *) set_get(txn->w_set, source_addr);
            if (unlikely(entry)) {
                LOG_NOTE("txn_read: transaction %lu read from write set for source: %p!\n", (tx_t) txn, source_addr);
                memcpy(target_addr, entry->data, word_size);
                continue;
            }
        }

        // Determine lock associated to shared memory region
        v_lock_t *lock = region_get_memory_lock_from_ptr(region, source_addr);

        // Verify lock is free (without acquiring it)
        int lv_pre = v_lock_version(lock);
        if ((lv_pre == LOCKED) || (lv_pre > txn->rv)) {
            LOG_WARNING("txn_read: transaction %lu failed lock PRE-validation for source: %p -> lock %p!\n", (tx_t) txn, source_addr, lock);
            txn_destroy(txn, region);
            return ABORT; 
        }

        memcpy(target_addr, source_addr, word_size);

        // Lock post-validation
        int lv_post = v_lock_version(lock);
        if ((lv_post == LOCKED) || (lv_post != lv_pre)) {
            LOG_WARNING("txn_read: transaction %lu failed lock POST-validation for source: %p -> lock %p\n", (tx_t) txn, source_addr, lock);
            txn_destroy(txn, region);
            return ABORT; 
        }

        if (unlikely(!txn->is_ro)) {
            // Add to read set
            if (unlikely(!r_set_add(txn->r_set, source_addr))) {
                LOG_WARNING("txn_read: transaction %lu failed add source: %p to read-set!\n", (tx_t) txn, source_addr);
                txn_destroy(txn, region);
                return ABORT;
            }
        }
    }
    return SUCCESS;
}

bool txn_write(struct txn_t *txn, struct region_t *region, void const *source, size_t size, void *target) {
    size_t word_size = region->align;
   
    for (size_t i = 0; i < size; i += word_size) {
        void *source_addr = (char *)source +i;
        void *target_addr = (char *)target +i;

        // Add to write set
        if (unlikely(!w_set_add(txn->w_set, source_addr, word_size, target_addr))) {
            LOG_WARNING("txn_write: transaction %lu failed to add entry {source: %p, target: %p, size: %p} to write set!\n", (tx_t) txn, source_addr, target_addr, word_size);
            txn_destroy(txn, region);
            return ABORT;
        }
    }
    return SUCCESS;
}

bool txn_end(struct txn_t *txn, struct region_t *region) {
    // append scheduled memory frees to region
    if (unlikely(txn->to_free_count > 0))
        region_append_to_free(region, txn->to_free, txn->to_free_count);

    // If transaction is read only or no writes occured (effectively read-only), directly commit
    if (likely(txn->is_ro || txn->w_set->count == 0)) return SUCCESS;

    // If transaction is read write, perform additional steps
    uint64_t lock_field[VLOCK_NUM / 64];
    set_get_lock_field(txn->w_set, lock_field);

    // Lock the write-set
    if (unlikely(!txn_lock(txn, region, lock_field))) {
        LOG_WARNING("txn_end: transaction %lu failed to lock write-set!\n", (tx_t) txn);
        return ABORT;
    }

    // Increment global version clock
    int wv = region_update_version_clock(region);
    
    if (likely(!txn_set_wv(txn, wv))) {
        // Validate the read set
        if (unlikely(!txn_validate_r_set(region, txn->r_set, txn->rv))){
            LOG_WARNING("txn_end: transaction %lu failed to validate read-set!\n", (tx_t) txn);
            txn_unlock(txn, region, lock_field, VLOCK_NUM, false);
            return ABORT;
        } 
    }
    
    // Commit
    txn_w_commit(txn->w_set);
    
    // Release locks and update their write version
    txn_unlock(txn, region, lock_field, VLOCK_NUM, true);
    return SUCCESS;
}

// ============================================= static functions implementation =============================================
static bool txn_lock(struct txn_t *txn, struct region_t *region, uint64_t *lock_field) {
    for (size_t i = 0; i < VLOCK_NUM; i++) {
        if (get_bit(lock_field, i)) {
            v_lock_t *lock = region_get_memory_lock_from_index(region, i);
            if (!v_lock_acquire(lock)) {
                // Failed to acquire lock -> unlock acquired locks & abort transaction
                txn_unlock(txn, region, lock_field, i, false);
                return ABORT;
            }
        }
    }
    return SUCCESS;
}

static bool txn_set_wv(struct txn_t *txn, int wv) {
    txn->wv = wv;
    return txn->rv+1 == wv;
}

static bool txn_validate_r_set(struct region_t *region, struct set_t *rs, int rv) {
    // Iterate through read set
    for (size_t i = 0; i < rs->capacity; i++) {
        if (get_bit(rs->occupied_field, i)) {
            read_entry_t *entry = rs->entries[i];
            v_lock_t *lock = region_get_memory_lock_from_ptr(region, entry->target);
    
            int lv = v_lock_version(lock);
            // If lock is not acquirable or the lock version clock is higher than tx->rv, abort.
            if (lv == LOCKED || lv > rv) {
                return ABORT;
            }
        }
    }
    return SUCCESS;
}

static void txn_w_commit(struct set_t *ws) {
    // Iterate through write set and write values
    for (size_t i = 0; i < ws->capacity; i++) {
        if (get_bit(ws->occupied_field, i)) {
            write_entry_t *we = (write_entry_t *) ws->entries[i];
            memcpy(we->base.target, we->data, ws->data_size);
        }
    }
}

static void txn_unlock(struct txn_t *txn, struct region_t *region, uint64_t *lock_field, size_t last, bool committed) {    
    for (size_t i = 0; i < last; i++) {
        if (get_bit(lock_field, i)) {
            v_lock_t *lock = region_get_memory_lock_from_index(region, i);
        
            // If transaction has committed, update lock versions
            if (unlikely(committed)) {
                v_lock_release_and_update(lock, txn->wv);
            } else {
                v_lock_release(lock);
            }
        }
    }
}