#include "txn.h"

inline tx_t tx_from_ptr(struct txn_t *t)
{
    if (!t) return invalid_tx;
    // Very unlikely that (tx_t)t == invalid_tx
    if (unlikely((tx_t)(uintptr_t)t == invalid_tx)) return invalid_tx;
    return (tx_t)(uintptr_t)t;
}

inline struct txn_t *tx_to_ptr(tx_t tx)
{
    if (tx == invalid_tx) return NULL;
    return (struct txn_t *)(uintptr_t)tx;
}

tx_t txn_create(bool is_ro, version_clock_t rv)
{
    struct txn_t *t = malloc(sizeof(struct txn_t));
    if (unlikely(!t)) return invalid_tx;

    t->is_ro = is_ro;
    t->r_version_clock = rv;
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
