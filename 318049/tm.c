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

#include "helper.h"
#include "macros.h"
#include "v_lock.h"
#include "txn.h"
#include "shared.h"

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {
    // LOG_LOG("tm_create: creating new transactional machine.\n");
    
    struct region_t *region = region_create(size, align);
    // If shared memory region allocation failed, return invalid_shared
    if (!region) {
        LOG_WARNING("tm_create: transactional machine shared memory region creation failed.\n");
        return invalid_shared;
    } 
    // LOG_LOG("tm_create: transactional machine shared memory region %p of size %lu and alignement %lu was successfully created.\n", (shared_t) region, size, align);
    
    return (shared_t) region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    region_destroy((struct region_t *)shared);
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) {
    return region_start((struct region_t *) shared);
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
 **/
size_t tm_size(shared_t shared) {
    return region_size((struct region_t *) shared);
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
 **/
size_t tm_align(shared_t shared) {
    return region_align((struct region_t *) shared);
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro) {
    LOG_LOG("tm_begin: creating new transaction.\n");
    
    struct region_t *region = (struct region_t *) shared;
    struct txn_t *txn = txn_create(is_ro, global_clock_load(&region->version_clock));

    // If transaction creation failed, return invalid_tx
    if (!txn) {
        LOG_WARNING("tm_begin: transaction creation failed.\n");
        return invalid_tx;
    }
    LOG_LOG("tm_begin: transaction %lu was successfully created.\n", (tx_t) txn);

    return (tx_t) txn;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
    struct txn_t *txn = (struct txn_t *) tx;
    struct region_t *region = (struct region_t *) shared;

    LOG_LOG("tm_end: transaction %lu is ending.\n", tx);

    // Try committing transaction
    bool result = txn_end(txn, region);

    if (result == SUCCESS) {
        LOG_TEST("tm_end: transaction %lu successfully commited.\n", tx);
    } else {
        LOG_WARNING("tm_end: transaction %lu failed to commit.\n", tx);
    }

    // Free transaction and return
    txn_free(txn);
    return result;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    // LOG_LOG("tm_read: transaction %lu is reading %lu bytes from %p to %p\n", tx, size, source, target);

    bool read_result = txn_read((struct txn_t *) tx, (struct region_t *) shared, source, size, target);

    if (read_result == SUCCESS) {
        // LOG_LOG("tm_read: transaction %lu read was a success!\n", tx);
    } else {
        LOG_WARNING("tm_read: transaction %lu read failed and transaction must abort!\n", tx);
    }
    return read_result;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    // LOG_LOG("tm_write: transaction %lu is writing %lu bytes from %p to %p\n", tx, size, source, target);
    
    bool write_result = txn_write((struct txn_t *) tx, (struct region_t *) shared, source, size, target);

    if (write_result == SUCCESS) {
        // LOG_LOG("tm_write: transaction %lu write was a success!\n", tx);
    } else {
        LOG_WARNING("tm_write: transaction %lu write failed and transaction must abort!\n", tx);
    }
    return write_result;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
alloc_t tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** target) {
    LOG_LOG("tm_alloc: transaction %lu is allocating %lu bytes\n", tx, size);

    struct segment_node_t *node = region_alloc((struct region_t *) shared, size);
    if (!node) {
        LOG_WARNING("tm_alloc: transaction %lu allocation failed\n", tx);
        return nomem_alloc;
    } 
    LOG_WARNING("tm_alloc: transaction %lu allocation was successful!\n", tx);

    // create pointer to start of memory region
    void *data = (void *) ((uintptr_t) node + sizeof(struct segment_node_t));
    memset(data, 0, size);

    // Set target to newly allocated memory region
    *target =  data;
    return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) {
    // struct region_t *region = (struct region_t *) shared;
    // struct segment_node_t* node = (struct segment_node_t*) ((uintptr_t) target - sizeof(struct segment_node_t));
    
    // return region_free(region, node);
    return true;
}
