#include "txn.h"

/**
 * Convert a pointer to a `struct txn_t` into a transaction identifier.
 *
 * The project defines `tx_t` as an unsigned integer type that can hold a
 * pointer value (`uintptr_t`). Use this helper to encode a live
 * `struct txn_t *` into a `tx_t`.
 *
 * @param t Pointer to a `struct txn_t` (may be NULL).
 * @return A `tx_t` encoding `t`, or `invalid_tx` if `t` is NULL or cannot be
 *         represented.
 */
static inline tx_t tx_from_ptr(struct txn_t *t)
{
    if (!t) return invalid_tx;
    // Very unlikely that (tx_t)t == invalid_tx
    if (unlikely((tx_t)(uintptr_t)t == invalid_tx)) return invalid_tx;
    return (tx_t)(uintptr_t)t;
}

/**
 * Convert a `tx_t` transaction identifier previously produced with
 * `tx_from_ptr` back into a `struct txn_t *`.
 *
 * @param tx Transaction identifier previously returned by
 *           `tx_from_ptr` (or `invalid_tx`).
 * @return A `struct txn_t *` corresponding to `tx`, or NULL when `tx ==`
 *         `invalid_tx`.
 *
 * @note Callers should not dereference the returned pointer if it is NULL.
 */
static inline struct txn_t *tx_to_ptr(tx_t tx)
{
    if (tx == invalid_tx) return NULL;
    return (struct txn_t *)(uintptr_t)tx;
}

tx_t txn_create(bool is_ro, shared_t shared)
{
    struct txn_t *t = malloc(sizeof(struct txn_t));
    if (unlikely(!t)) return invalid_tx;

    t->is_ro = is_ro;
    t->r_version_clock = atomic_load(&(shared_to_ptr(shared)->version_clock)); // Should be an atomic read
    t->w_version_clock = 0;
    t->r_set = set_init();
    t->w_set = map_init();

    return tx_from_ptr(t);
}

void txn_free(tx_t tx)
{
    struct txn_t *t = tx_to_ptr(tx);
    if (unlikely(!t)) return;
    
    set_free(t->r_set);
    map_free(t->w_set);
    free(t);
}

bool txn_is_ro(tx_t tx) {
    return tx_to_ptr(tx)->is_ro;
}

bool txn_read(tx_t tx, void const *source, size_t size, void *target) {
    struct txn_t *t = tx_to_ptr(tx);

    // Check if transaction is read-only
    if (t->is_ro) {
        memcpy(target, source, size);
    }
    else {
        // Check if address has already been written to
        if (map_contains(t->w_set, source)) {
            // map_get will write to target
            map_get(t->w_set, source, size, target);
        } else {
            memcpy(target, source, size);
        }
    }
    
    // Post-validation: check if lock is free and has not changed. Else abort
    struct v_lock_t *lock = &(((struct segment_node *) ((uintptr_t) source - sizeof(struct segment_node)))->lock);
    if (!v_lock_test(lock) || v_lock_version(lock) > t->r_version_clock){
        return false;
    }

    // If transaction is read-write, add address to read set
    if (!t->is_ro) { set_add(t->r_set, source); }
    return true;
}

bool txn_write(tx_t tx, void const *source, size_t size, void *target) {
    return map_add(tx_to_ptr(tx)->w_set, source, size, target);
}

bool txn_end(tx_t tx, shared_t shared) {
    struct txn_t *t = tx_to_ptr(tx);
    if (t->is_ro) {
        txn_free(tx);
        return true;
    }
    else {
        // Lock the write-set (Go through region LL and lock if they are in the write set)
        if (!txn_lock_for_commit(tx, shared)) {
            txn_free(tx);
            return false;
        }

        // Increment global version clock with CAS
        // -> store new global version clock to variable: wv
        version_clock_t wv = shared_update_version_clock(shared, t->r_version_clock);
        
        if (!txn_set_wv(tx, wv)) {
            // Validate the read set
            if (!txn_validate_r_set(tx, shared)){
                txn_release_after_commit(tx, shared);
                txn_free(tx);
                return false;
            } 
        }
        
        // Commit
        txn_w_commit(tx, shared);
        
        // Release locks
        txn_release_after_commit(tx, shared);
    }
}

// ======= txn_end methods ======= //
static bool txn_lock_for_commit(tx_t tx, shared_t shared)
{
    struct txn_t *t = tx_to_ptr(tx);
    struct shared *s = shared_to_ptr(shared);
    struct segment_node *node = s->allocs;
    
    bool abort = false;
    struct v_lock_t* acquired_locks[map_size(t->w_set)];
    size_t count = 0;
    // Try acquiring all locks
    while (node) {
        if (txn_w_set_contains(tx, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
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

/**
 * @return Whether tx->rv + 1 == wv
 */
static bool txn_set_wv(tx_t tx, version_clock_t wv) {
    struct txn_t *t = tx_to_ptr(tx);
    t->w_version_clock = wv;
    return t->r_version_clock+1 == wv;
}

static bool txn_validate_r_set(tx_t tx, shared_t shared)
{
    struct txn_t *t = tx_to_ptr(tx);
    struct segment_node *node = shared_to_ptr(shared)->allocs;

    while (node) {
        if (set_contains(t->r_set, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            // If lock is not acquirable or the lock version clock is higher than tx->rv, abort.
            if (!v_lock_test(&(node->lock)) || 
                v_lock_version(&(node->lock)) > t->r_version_clock)
            {
                return false;
            }
        }        
        node = node->next;
    }
    return true;
}

static void txn_w_commit(tx_t tx, shared_t shared)
{
    struct txn_t *t = tx_to_ptr(tx);
    struct segment_node *node = shared_to_ptr(shared)->allocs;

    while (node) {
        if (txn_w_set_contains(tx, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            for (size_t i = 0; i < t->w_set->count; i++)
            {
                memcpy(t->w_set->targets[i], t->w_set->sources[i], t->w_set->sizes[i]);
            }
        }        
        node = node->next;
    }
}

static void txn_release_after_commit(tx_t tx, shared_t shared)
{
    struct txn_t *t = tx_to_ptr(tx);
    struct segment_node *node = shared_to_ptr(shared)->allocs;

    while (node) {
        if (txn_w_set_contains(tx, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            v_lock_update_version(&(node->lock), t->w_version_clock);
            v_lock_release(&(node->lock));
        }
        node = node->next;
    }
}

static bool txn_w_set_contains(tx_t tx, void *target){
    struct txn_t *t = tx_to_ptr(tx);
    return map_contains(t->w_set, target);
}