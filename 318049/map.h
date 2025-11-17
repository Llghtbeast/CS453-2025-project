#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"

#define MAX_MAP_SIZE 100

/**
 * @brief Minimalist map implementation, TODO: improve once solution works.
 */
struct map_t {
    void const* sources[MAX_MAP_SIZE];
    size_t sizes[MAX_MAP_SIZE];
    void const* targets[MAX_MAP_SIZE];
    size_t count;
};

/**
 * Initialize a map
 * @return Pointer to initialized map
 */
struct map_t *map_init();

/**
 * Add an element to the map.
 * @param map the map to add to
 * @param source pointer to source write location
 * @param size size in bytes of value at source
 * @param target pointer to target write location
 * @return Whether the operation was a success
 */
bool map_add(struct map_t* map, void const *source, size_t size, void* target);

/**
 * Check if a pointer is a key in this map
 * TODO: Use a Bloom filter for this check
 * @param map the map to check the element belongs to
 * @param ptr the pointer to check belongs to map
 * @return Whether the pointer belongs to the map
 */
bool map_contains(struct map_t *map, void const *ptr);

/**
 * Get an element to the map.
 * @param map the map to get from
 * @param source pointer to source write location
 * @param size size in bytes of value at source
 * @param target pointer to target write location
 * @return Whether the operation was a success
 */
bool map_get(struct map_t* map, void const *source, size_t size, void* target);

void map_free(struct map_t *map);

size_t map_size(struct map_t *map);