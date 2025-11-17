#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "tm.h"
#include "set.h"
#include "map.h"
#include "macros.h"
#include "shared.h"

struct txn_t {
    bool is_ro;
    version_clock_t r_version_clock;
    version_clock_t w_version_clock;
    struct set_t *r_set;
    struct map_t *w_set;
};

/**
 * Allocate and initialize a new transaction object.
 *
 * Typical usage: the transaction manager calls this at transaction begin to
 * create state for a new transaction, then returns the resulting `tx_t` to
 * the caller.
 *
 * @param is_ro Whether the new transaction is read-only (true) or read-write
 *              (false).
 * @param shared Shared memory region of the transactional memory.
 * @return A `tx_t` encoding a newly allocated `struct txn_t` on success, or
 *         `invalid_tx` on allocation failure.
 */
tx_t txn_create(bool is_ro, shared_t shared);

/**
 * Free transaction resources and the transaction object itself.
 *
 * This function accepts a `tx_t` value (that was previously returned by
 * `txn_create` or other internal helpers). If `tx == invalid_tx` the call is
 * a no-op.
 *
 * @param tx Transaction identifier to free.
 */
void txn_free(tx_t tx);

/**
 * @return Whether the transaction is read-only or not
 */
bool txn_is_ro(tx_t tx);

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
 * @param source Address within the region to read from.
 * @param size   Number of bytes to copy.
 * @param target Destination buffer (caller-allocated) to receive the data.
 * @return true on success; false on failure (e.g., invalid args, out-of-bounds,
 *         or transaction abort decision).
 */
bool txn_read(tx_t tx, void const *source, size_t size, void *target);

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
bool txn_write(tx_t tx, void const *source, size_t size, void *target);

/** End the transaction and cleanup.
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
 **/
bool txn_end(tx_t tx);
