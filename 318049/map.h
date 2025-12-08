#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"

#define INITIAL_CAPACITY 8
#define GROW_FACTOR 2

/**
 * @brief write entry implementation. Implements a key-value pair with `target` as key and `(size, data)` as value.
 * @param target pointer to target memory location where value should be written to.
 * @param size   size of buffer to be written
 * @param data   data to be written
 */
struct write_entry_t {
    void* target;
    size_t size;
    void* data;
};

/**
 * @brief write-set implementation, TODO: improve once solution works. (sort the elements progressively, use a tree, ...)
 */
struct write_set_t {
    struct write_entry_t** entries;
    size_t count;
    size_t capacity;
};

// ============== write_entry_t methods ============== 
struct write_entry_t * write_entry_create(void const *source, size_t size, void *target);

bool write_entry_update(struct write_entry_t *we, void const *source, size_t size);

void write_entry_free(struct write_entry_t *we);

// ============== write_set_t methods ============== 
/**
 * Initialize a write_set_t
 * @return Pointer to initialized write_set
 */
struct write_set_t *write_set_init();

/**
 * Add an element to the write_set.
 * @param ws the write_set to add to
 * @param source pointer to source write location
 * @param size size in bytes of value at source
 * @param target pointer to target write location
 * @return Whether the operation was a success
 */
bool write_set_add(struct write_set_t* ws, void const *source, size_t size, void* target);

/**
 * Check if a pointer is a key in this write_set
 * TODO: Use a Bloom filter for this check
 * @param ws the write_set to check the element belongs to
 * @param ptr the pointer to check belongs to write_set
 * @return Whether the pointer belongs to the write_set
 */
bool write_set_contains(struct write_set_t *ws, void *target);

/**
 * Get an element from the write_set.
 * @param ws the write_set to get from
 * @param source pointer to source write location
 * @param size size in bytes of value at source
 * @param target pointer to target write location
 * @return Whether the operation was a success
 */
bool write_set_get(struct write_set_t* ws, void const *source, size_t size, void* target);

void write_set_free(struct write_set_t *ws);

size_t write_set_size(struct write_set_t *ws);
