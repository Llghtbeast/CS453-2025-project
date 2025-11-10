#include "txn.h"

static inline tx_t tx_from_ptr(struct txn_t *t)
{
    if (!t) return invalid_tx;
    // Very unlikely that (tx_t)t == invalid_tx
    if (unlikely((tx_t)(uintptr_t)t == invalid_tx)) return invalid_tx;
    return (tx_t)(uintptr_t)t;
}

static inline struct txn_t *tx_to_ptr(tx_t tx)
{
    if (tx == invalid_tx) return NULL;
    return (struct txn_t *)(uintptr_t)tx;
}

tx_t txn_create(bool is_ro, struct region *r)
{
    struct txn_t *t = malloc(sizeof(struct txn_t));
    if (unlikely(!t)) return invalid_tx;

    t->shared = r;
    t->is_ro = is_ro;
    t->r_version_clock = r->version_clock;
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

bool txn_read(tx_t tx, void const *source, size_t size, void *target) {
    // TODO
    return false;
}

bool txn_write(tx_t tx, void const *source, size_t size, void *target) {
    // TODO
    return false;
}

bool txn_w_set_contains(tx_t tx, void *target){
    struct txn_t *t = tx_to_ptr(tx);
    return map_contains(t->w_set, target);
}

bool txn_lock_for_commit(tx_t tx)
{
    struct txn_t *t = tx_to_ptr(tx);
    struct segment_node *node = t->shared->allocs;

    bool abort = false;
    struct v_lock_t* acquired_locks[map_size(t->w_set)];
    uint32_t count = 0;
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

bool txn_set_wv(tx_t tx, uint32_t wv) {
    struct txn_t *t = tx_from_ptr(tx);
    t->w_version_clock = wv;
    return t->r_version_clock+1 == wv;
}

bool txn_validate_r_set(tx_t tx)
{
    struct txn_t *t = tx_to_ptr(tx);
    struct segment_node *node = t->shared->allocs;

    // 
    while (node) {
        if (txn_r_set_contains(tx, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
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

void txn_commit(tx_t tx)
{
    struct txn_t *t = tx_to_ptr(tx);
    struct segment_node *node = t->shared->allocs;

    while (node) {
        if (txn_w_set_contains(tx, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            // TODO this is not right
            // txn_write(tx, node);
        }        
        node = node->next;
    }
}

void txn_release_after_commit(tx_t tx)
{
    struct txn_t *t = tx_to_ptr(tx);
    struct segment_node *node = t->shared->allocs;

    while (node) {
        if (txn_w_set_contains(tx, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            v_lock_update_version(&(node->lock), t->w_version_clock);
            v_lock_release(&(node->lock));
        }
        node = node->next;
    }
}
