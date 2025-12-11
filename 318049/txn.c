#include "txn.h"

// ------- txn_end helper -------

static bool txn_lock(struct txn_t *txn, shared_t shared);

/**
 * @return Whether tx->rv + 1 == wv
 */
static bool txn_set_wv(struct txn_t *txn, version_clock_t wv);

static bool txn_validate_r_set(struct txn_t *txn, shared_t shared);

static void txn_w_commit(struct txn_t *txn, shared_t shared);

static void txn_release(struct txn_t *txn, shared_t shared, bool committed);

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
        // write_set_get will write to target if the write set contains the target
        if (!write_set_get(txn->w_set, target, size, source)) {
            // If write set does not contain the source, write
            memcpy(target, source, size);
        }
    }
    
    // Post-validation: check if lock is free and has not changed. Else abort
    struct v_lock_t *lock = &(((struct segment_node *) ((uintptr_t) source - sizeof(struct segment_node)))->lock);
    int version = v_lock_version(lock);
    if (version == LOCKED || version > txn->rv) return false;

    // If transaction is read-write, add address to read set
    if (!txn->is_ro) { set_add(txn->r_set, source); }
    return true;
}

bool txn_write(struct txn_t *txn, void const *source, size_t size, void *target) {
    return w_set_add(txn->w_set, source, size, target);
}

bool txn_end(struct txn_t *txn, shared_t shared) {
    // If transaction is read write, perform additional steps
    if (!txn->is_ro) {
        // Lock the write-set (Go through region LL and lock if they are in the write set)
        if (!txn_lock(txn, shared)) {
            txn_free(txn);
            return false;
        }

        // Increment global version clock
        version_clock_t wv = shared_update_version_clock(shared);
        
        if (!txn_set_wv(txn, wv)) {
            // Validate the read set
            if (!txn_validate_r_set(txn, shared)){
                txn_release(txn, shared, false);
                txn_free(txn);
                return false;
            } 
        }
        
        // Commit
        txn_w_commit(txn, shared);
        
        // Release locks
        txn_release(txn, shared, true);
    }

    // Destroy the commited transaction
    txn_free(txn);
    return true;
}

// ============================================= static functions implementation =============================================
static bool txn_lock(struct txn_t *txn, shared_t shared) {
    struct shared *s = shared_to_ptr(shared);
    struct segment_node *node = s->allocs;
    
    bool abort = false;
    struct v_lock_t* acquired_locks[write_set_size(txn->w_set)];
    size_t count = 0;
    // Try acquiring all locks
    while (node) {
        if (txn_w_set_contains(txn, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            if (!v_lock_acquire(&(node->lock))) {
                // Failed to acquire lock -> abort transaction
                abort = true;
                break;
            }
            else {
                acquired_locks[count++] = &(node->lock);
            }
        }
        node = node->next;
    }

    // If lock acquisition failed, need to release all acquired locks to avoid deadlock
    if (abort) {
        for (size_t i = 0; i < count; i++) {
            v_lock_release(acquired_locks[i]);
        }
    }
    return !abort;
}

static bool txn_set_wv(struct txn_t *txn, version_clock_t wv) {
    txn->wv = wv;
    return txn->rv+1 == wv;
}

static bool txn_validate_r_set(struct txn_t *txn, shared_t shared) {
    struct segment_node *node = shared_to_ptr(shared)->allocs;

    while (node) {
        if (set_contains(txn->r_set, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            // If lock is not acquirable or the lock version clock is higher than tx->rv, abort.
            if (v_lock_version(&(node->lock) != LOCKED) || 
                v_lock_version(&(node->lock)) > txn->rv)
            {
                return false;
            }
        }        
        node = node->next;
    }
    return true;
}

static void txn_w_commit(struct txn_t *txn, shared_t shared) {
    struct segment_node *node = shared_to_ptr(shared)->allocs;

    while (node) {
        if (set_contains(txn->w_set, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            for (size_t i = 0; i < txn->w_set->count; i++)
            {
                write_entry_t *we = txn->w_set->entries[i];
                memcpy(we->base.target, we->data, we->size);
            }
        }        
        node = node->next;
    }
}

static void txn_release(struct txn_t *txn, shared_t shared, bool committed) {
    struct segment_node *node = shared_to_ptr(shared)->allocs;

    while (node) {
        if (set_contains(txn->w_set, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            // If transaction has committed, update lock versions
            if (committed) {
                v_lock_update(&(node->lock), txn->wv);
            }
            v_lock_release(&(node->lock));
        }
        node = node->next;
    }
}