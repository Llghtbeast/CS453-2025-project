#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"

#define MAX_SET_SIZE 100

/**
 * @brief Minimalist set implementation, TODO: improve once solution works.
 */
struct set_t {
    void *addr_set[MAX_SET_SIZE];
    uint32_t count;
};

/**
 * Initialize a set
 * @return Pointer to initialized set
 */
struct set_t *set_init();

/**
 * Add an element to the set.
 * @param ptr memory address to add to set
 * @return Whether the operation was a success
 */
bool set_add(struct set_t *set, void const *ptr);

bool set_contains(struct set_t *set, void const *ptr);

void set_free(struct set_t *set);

uint32_t set_size(struct set_t *set);