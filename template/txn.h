#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tm.h"
#include "set.h"
#include "map.h"
#include "macros.h"
#include "tm.c"

struct txn_t {
    struct region *shared;
    bool is_ro;
    version_clock_t r_version_clock;
    version_clock_t w_version_clock;
    struct set_t *r_set;
    struct map_t *w_set;
};

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
static inline tx_t tx_from_ptr(struct txn_t *t);

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
static inline struct txn_t *tx_to_ptr(tx_t tx);

/**
 * Allocate and initialize a new transaction object.
 *
 * Typical usage: the transaction manager calls this at transaction begin to
 * create state for a new transaction, then returns the resulting `tx_t` to
 * the caller.
 *
 * @param is_ro Whether the new transaction is read-only (true) or read-write
 *              (false).
 * @param r Shared memory region of the transactional memory.
 * @return A `tx_t` encoding a newly allocated `struct txn_t` on success, or
 *         `invalid_tx` on allocation failure.
 */
tx_t txn_create(bool is_ro, struct region *r);

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

bool txn_w_set_contains(tx_t, void* target);

// ======= tm_end methods ======= //
bool txn_lock_for_commit(tx_t tx);

/**
 * @return Whether tx->rv + 1 == wv
 */
bool txn_set_wv(tx_t tx, uint32_t wv);

bool txn_validate_r_set(tx_t tx);

void txn_commit(tx_t tx);

void txn_release_after_commit(tx_t tx);
