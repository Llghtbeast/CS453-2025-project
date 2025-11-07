#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_MAP_SIZE 100

/**
 * @brief Minimalist map implementation, TODO: improve once solution works.
 */
struct map_t {
    void *keys[MAX_MAP_SIZE];
    int32_t values[MAX_MAP_SIZE];
    uint32_t count;
};

/**
 * Initialize a map
 * @return Pointer to initialized map
 */
struct map_t *map_init();

/**
 * Add an element to the map.
 * @param map the map to add to
 * @param ptr memory address to add to map
 * @param value value to add to map
 * @return Whether the operation was a success
 */
bool map_add(struct map_t* map, void *ptr, int32_t value);

/**
 * Check if a pointer is a key in this map
 * TODO: Use a Bloom filter for this check
 * @param map the map to check the element belongs to
 * @param ptr the pointer to check belongs to map
 * @return Whether the pointer belongs to the map
 */
bool map_contains(struct map_t *map, void *ptr);