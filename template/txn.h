#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "tm.h"
#include "set.h"
#include "map.h"

struct txn_t {
    bool is_ro;
    version_clock_t r_version_clock;
    version_clock_t w_version_clock;
    struct set_t *r_set;
    struct map_t *w_set;
};

/**
 * Cast transaction struct to type tx_t
 * @param t transaction struct to cast
 */
static inline tx_t tx_from_ptr(struct txn_t *t);

static inline struct txn_t *tx_to_ptr(tx_t tx);

tx_t txn_create(bool is_ro, version_clock_t rv);

void txn_free(tx_t tx);

bool txn_read(tx_t tx, struct region *region, void const* source, size_t size, void* target);

bool txn_write(tx_t tx, void* source, void* target);