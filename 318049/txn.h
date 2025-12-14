#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "tm.h"
#include "map.h"
#include "macros.h"
#include "shared.h"

#define ABORT false
#define COMMIT true
#define INVALID -1

struct txn_t {
    bool is_ro;
    int rv;
    int wv;

    // Read and write sets. Contain target address (struct segment_node_t *), data and size of data to be written
    struct set_t *r_set;
    struct set_t *w_set;
};

/**
 * Allocate and initialize a new transaction object.
 *
 * Typical usage: the transaction manager calls this at transaction begin to
 * create state for a new transaction, then returns the resulting `struct txn_t *` to
 * the caller.
 *
 * @param is_ro Whether the new transaction is read-only (true) or read-write
 *              (false).
 * @param rv Read version of global clock.
 * @return A `struct txn_t *` encoding a newly allocated `struct txn_t` on success, or
 *         `invalid_tx` on allocation failure.
 */
struct txn_t *txn_create(bool is_ro, int rv);

/**
 * Free transaction resources and the transaction object itself.
 *
 * This function accepts a `struct txn_t *` value (that was previously returned by
 * `txn_create` or other internal helpers). If `tx == invalid_tx` the call is
 * a no-op.
 *
 * @param txn Transaction to free.
 */
void txn_free(struct txn_t *txn);

/**
 * @return Whether the transaction is read-only or not
 */
bool txn_is_ro(struct txn_t *txn);

/**
 * Read `size` bytes from `target` region address into `source` under the context of
 * transaction `tx`.
 *
 * This function should perform any transactional bookkeeping required
 * (e.g., adding the read address to the transaction read-set) and then copy
 * the requested bytes from `source` into `target`.
 *
 * @param tx     Transaction identifier (may be `invalid_tx` for non-transactional
 *               reads depending on your design).
 * @param source Address within the shared region to read from.
 * @param size   Number of bytes to copy.
 * @param target Destination buffer (caller-allocated) to receive the data.
 * @return true on success; false on failure (e.g., invalid args, out-of-bounds,
 *         or transaction abort decision).
 */
bool txn_read(struct txn_t *txn, struct region_t *region, void const *source, size_t size, void *target);

/**
 * Write `size` bytes from `source` into `target` (address in `region`) under
 * the context of transaction `tx`.
 *
 * This API performs any bookkeeping required for the transaction's write-set
 * and ensures that writes are staged/validated according to the transaction
 * manager's semantics.
 *
 * @param tx     Transaction identifier.
 * @param source Source buffer to copy from.
 * @param size   Number of bytes to write.
 * @param target Destination address within the region.
 * @return true on success; false on failure (e.g., out-of-bounds, conflicts,
 *         or when the transaction should be aborted).
 */
bool txn_write(struct txn_t *txn, struct region_t *region, void const *source, size_t size, void *target);

/** End the transaction and cleanup.
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
 **/
bool txn_end(struct txn_t *txn, struct region_t *);
