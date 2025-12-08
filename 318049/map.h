#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"

#define INITIAL_CAPACITY 8
#define GROW_FACTOR 2

/**
 * @brief write/read set entry implementation. 
 * If contained in a read-set, only use the `target` attribute
 * If contained in a write-set, implements a key-value pair with `target` as key and `(size, data)` as value.
 * @param target pointer to target memory location where value should be written to.
 * @param size   size of buffer to be written
 * @param data   data to be written
 */
struct entry_t {
    void* target;
    size_t size;    // Not used in read-set
    void* data;     // Not used in read-set
};

/**
 * @brief read/write set implementation.
 * TODO: improve once solution works. (sort the elements progressively, use a tree, ...)
 */
struct set_t {
    struct entry_t** entries;
    size_t count;
    size_t capacity;
};

// ============== entry_t methods ============== 
struct entry_t *r_entry_create(void const *source);

struct entry_t *w_entry_create(void const *source, size_t size, void *target);

bool entry_update(struct entry_t *entry, void const *source, size_t size);

void entry_free(struct entry_t *entry);

// ============== set_t methods ============== 
/**
 * Initialize a set_t
 * @return Pointer to initialized set
 */
struct set_t *set_init();

/**
 * Add an element to the write set.
 * @param set the set to add to
 * @param source pointer to source write location
 * @param size size in bytes of value at source
 * @param target pointer to target write location
 * @return Whether the operation was a success
 */
bool w_set_add(struct set_t* set, void const *source, size_t size, void* target);

/**
 * Add an element to the read set.
 * @param set the set to add to
 * @param source pointer to source write location
 * @return Whether the operation was a success
 */
bool r_set_add(struct set_t* set, void const *source, size_t size, void* target);

/**
 * Check if a pointer is a key in this set
 * TODO: Use a Bloom filter for this check
 * @param set the set to check the element belongs to
 * @param ptr the pointer to check belongs to set
 * @return Whether the pointer belongs to the set
 */
bool set_contains(struct set_t *set, void *target);

/**
 * Read an element from the set.
 * @param set the set to get from
 * @param key pointer to target read location
 * @param size size in bytes of value at source
 * @param dest pointer to location to write to
 * @return Whether the operation was a success
 */
bool set_read(struct set_t *set, void const *key, size_t size, void* dest);

void set_free(struct set_t *set);

size_t set_size(struct set_t *set);

bool set_grow(struct set_t *set);
