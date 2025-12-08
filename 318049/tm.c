/**
 * @file   tm.c
 * @author [...]
 *
 * @section LICENSE
 *
 * [...]
 *
 * @section DESCRIPTION
 *
 * Implementation of your own transaction manager.
 * You can completely rewrite this file (and create more files) as you wish.
 * Only the interface (i.e. exported symbols and semantic) must be preserved.
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers

// Internal headers
#include <tm.h>

#include "macros.h"
#include "v_lock.h"
#include "shared.h"
#include "map.h"

/* ============ Useful structures ============ */
/**
 * @brief transaction structure
 */
struct txn_t {
    bool is_ro;
    version_clock_t r_version_clock;
    version_clock_t w_version_clock;
    struct set_t *r_set;
    struct set_t *w_set;
};


/* ============ Pointer casting methods ============ */
/**
 * @brief static method for transforming `tx_t` to `struct txn_t *`
 */
static inline struct txn_t *tx_to_ptr(tx_t tx);

/* ============ Transaction helper methods ============ */
/**
 * method to release locks held while trying to commit this transaction
 */
static void txn_release(tx_t tx, shared_t shared, bool committed);

/**
 * Method to destroy a transaction
 */
void txn_free(tx_t tx);


/* ============ Transaction Memory implementation ============ */
/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {
    return shared_create(size, align);
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    shared_destroy(shared);
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) {
    return shared_start(shared);
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
 **/
size_t tm_size(shared_t shared) {
    return shared_size(shared);
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
 **/
size_t tm_align(shared_t shared) {
    return shared_align(shared);
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro) {
    // allocate memory for transaction structure
    struct txn_t *t = malloc(sizeof(struct txn_t));
    if (unlikely(!t)) return invalid_tx;

    t->is_ro = is_ro;
    t->r_version_clock = atomic_load(&(shared_to_ptr(shared)->version_clock)); // Should be an atomic read
    t->w_version_clock = 0;
    t->r_set = set_init();
    t->w_set = set_init();

    return (tx_t)t;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
    struct txn_t *t = tx_to_ptr(tx);

    // If transaction is read-only, commit directly
    if (t->is_ro) {
        txn_free(tx);
        return true;
    }

    // If transaction is read write, perform additional steps
    struct shared *s = shared_to_ptr(shared);
    
    // STEP 1: Lock the write-set (Go through region LL and lock if they are in the write set)
    struct segment_node *node = s->allocs;
    bool abort = false;
    struct v_lock_t* acquired_locks[set_size(t->w_set)];
    size_t count = 0;
    // Try acquiring all locks
    while (node) {
        if (set_contains(t->w_set, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
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

    // If lock acquisition failed, need to release all acquired locks to avoid deadlock, and abort
    if (abort) {
        for (size_t i = 0; i < count; i++) {
            v_lock_release(acquired_locks[i]);
        }
        txn_free(tx);
        return false;
    }

    // STEP 2: Increment global version clock
    t->w_version_clock = shared_update_version_clock(shared);
    
    // Do not need to perform step 3 if write version is read version +1
    if (t->w_version_clock != t->r_version_clock+1) {
        // STEP 3: Validate the read set
        node = s->allocs;
        while (node) {
            if (set_contains(t->r_set, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
                // If lock is not acquirable or the lock version clock is higher than tx->rv, abort.
                if (!v_lock_test(&(node->lock)) || 
                    v_lock_version(&(node->lock)) > t->r_version_clock)
                {
                    txn_release(tx, shared, false);
                    txn_free(tx);
                    return false;
                }
            }     
            node = node->next;
        }
    }
    
    // STEP 4: Write values of write set
    for (size_t i = 0; i < t->w_set->count; i++)
    {
        struct entry_t *we = t->w_set->entries[i];
        memcpy(we->target, we->data, we->size);
    }
    
    // STEP 5: Update and release locks
    txn_release(tx, shared, true);

    // STEP 6: Destroy the commited transaction
    txn_free(tx);
    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t unused(shared), tx_t tx, void const* source, size_t size, void* target) {
    struct txn_t *t = tx_to_ptr(tx);

    // Check if transaction is read-only
    if (t->is_ro) {
        memcpy(target, source, size);
    }
    else {
        // set_read will write to target if the write set contains the target
        if (set_read(t->w_set, source, size, target)) {
            // if target belongs to set, return early, as there is no need to validate or add to read_set
            return true;
        } 
        else {
            // If write set does not contain the source, write anyway
            memcpy(target, source, size);
        }
    }
    
    // Post-validation: check if lock is free and has not changed. Else abort
    struct v_lock_t *lock = &(((struct segment_node *) ((uintptr_t) source - sizeof(struct segment_node)))->lock);
    if (!v_lock_test(lock) || v_lock_version(lock) > t->r_version_clock){
        return false;
    }

    // If transaction is read-write, add address to read set
    if (!t->is_ro) set_add(t->r_set, source);
    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t unused(shared), tx_t tx, void const* source, size_t size, void* target) {
    return set_add(tx_to_ptr(tx)->w_set, source, size, target);
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
alloc_t tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** target) {
    return shared_alloc(shared, size, target);
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t shared, tx_t unused(tx), void* target) {
    return shared_free(shared, target);
}

// pointer reallocation methods
static inline struct txn_t *tx_to_ptr(tx_t tx) {
    if (tx == invalid_tx) return NULL;
    return (struct txn_t *)(uintptr_t)tx;
}

// transaction helper methods
void txn_free(tx_t tx) {
    struct txn_t *t = tx_to_ptr(tx);
    if (unlikely(!t)) return;
    
    set_free(t->r_set);
    set_free(t->w_set);
    free(t);
}

static void txn_release(tx_t tx, shared_t shared, bool committed) {
    struct txn_t *t = tx_to_ptr(tx);
    struct segment_node *node = shared_to_ptr(shared)->allocs;

    while (node) {
        if (txn_w_set_contains(tx, (void*) ((uintptr_t) node + sizeof(struct segment_node)))) {
            // If transaction has committed, update lock versions
            if (committed) {
                v_lock_update_version(&(node->lock), t->w_version_clock);
            }
            v_lock_release(&(node->lock));
        }
        node = node->next;
    }
}