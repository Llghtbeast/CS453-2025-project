#pragma once

// Requested feature: posix_memalign
// #define _POSIX_C_SOURCE   200809L

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "helper.h"
#include "macros.h"

/**
 * @brief Base entry for read/write sets.
 * Read entries only use the target field.
 * @param target pointer to target memory location
 */
struct base_entry_t {
    void *target;
};

/**
 * @brief Write entry extends base_entry_t with data to write.
 * @param base   base entry containing target pointer
 * @param size   size of buffer to be written
 * @param data   data to be written
 */
typedef struct write_entry_t {
    struct base_entry_t base;
    void *data;
} write_entry_t;

typedef struct base_entry_t read_entry_t;

/**
 * @brief read/write set implementation.
 * TODO: improve once solution works. (sort the elements progressively, use a tree, hash table)
 * Current implementation uses: Hash table
 */
struct set_t {
    bool is_write_set;
    size_t data_size;

    struct base_entry_t** entries;
    uint64_t *occupied_field;
    size_t count;
    size_t capacity;
};

// ============== entry_t methods ============== 
/**
 * Create a read entry
 * @param target pointer to target memory location
 * @return Pointer to created read entry, NULL on failure
 */
read_entry_t *r_entry_create(void *target);

/**
 * Create a write entry
 * @param source pointer to source data
 * @param size size in bytes of data
 * @param target pointer to target memory location
 * @return Pointer to created write entry, NULL on failure
 */
write_entry_t *w_entry_create(void const *source, size_t size, void *target);

/**
 * Update a write entry's data
 * @param entry write entry to update
 * @param source pointer to new source data
 * @param size size in bytes of new data
 * @return Whether the operation was a success
 */
bool w_entry_update(write_entry_t *entry, void const *source, size_t size);

/**
 * Free a read entry
 * @param entry entry to free
 */
void r_entry_free(read_entry_t *entry);

/**
 * Free a write entry (frees both data and entry)
 * @param entry entry to free
 */
void w_entry_free(struct write_entry_t *entry);

// ============== set_t methods ============== 
/**
 * Initialize a set_t
 * @param is_write_set whether this is a write set (true) or read set (false)
 * @return Pointer to initialized set
 */
struct set_t *set_init(bool is_write_set, size_t data_size);

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
 * @param target pointer to target read location
 * @return Whether the operation was a success
 */
bool r_set_add(struct set_t* set, void* target);

/**
 * Get pointer to entry with key key, else NULL
 */
struct base_entry_t *set_get(struct set_t *set, void *key);

/**
 * Free the set and all its entries
 * @param set the set to free
 */
void set_free(struct set_t *set);


/**
 * Get the number of entries in the set
 * @param set the set to query
 * @return Number of entries
 */
size_t set_size(struct set_t *set);

/**
 * Grow the set capacity
 * @param set the set to grow
 * @return Whether the operation was a success
 */
bool set_grow(struct set_t *set);

void set_get_lock_field(struct set_t *set, uint64_t *lock_field);
