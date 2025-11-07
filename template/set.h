#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

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
bool set_add(struct set_t *set, void *ptr);