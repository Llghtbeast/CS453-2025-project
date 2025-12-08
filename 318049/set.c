#include "set.h"

struct set_t *set_init()
{
    struct set_t *s = (struct set_t *) malloc(sizeof(struct set_t));
    if (unlikely(!s)) return NULL;
    s->count = 0;
    memset(s->addr_set, 0, sizeof(s->addr_set));
    return s;
}

bool set_add(struct set_t *set, void const *ptr)
{
    if (!set) return false;
    // Check if pointer already in set
    for (size_t i = 0; i < set->count; i++) {
        if (set->addr_set[i] == ptr) return true;
    }
    // Check if there is enough space to add
    if (set->count >= MAX_SET_SIZE) return false;

    // Insert pointer at end of list
    set->addr_set[set->count++] = ptr;
    return true;
}

bool set_contains(struct set_t *set, void const *ptr) {
    if (unlikely(!set)) return false;
    // Check if pointer already in set
    for (size_t i = 0; i < set->count; i++) {
        if (set->addr_set[i] == ptr) return true;
    }
    return false;
}

void set_free(struct set_t *set)
{
    if (unlikely(!set)) return;
    free(set);
}

size_t set_size(struct set_t *set)
{
    return set->count;
}
