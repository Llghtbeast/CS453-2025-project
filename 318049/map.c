#include "map.h"

// ============== entry_t methods ============== 
read_entry_t *r_entry_create(void *target) {
    // Allocate memory for struct
    read_entry_t *entry = malloc(sizeof(read_entry_t));
    if (unlikely(!entry)) return NULL;

    entry->target = target;
    return entry;
}

write_entry_t *w_entry_create(void const *source, size_t size, void *target) {
    // Allocate memory for struct
    write_entry_t *entry = malloc(sizeof(write_entry_t));
    if (unlikely(!entry)) return NULL;

    // Try allocating memory for data 
    entry->data = malloc(size);
    if (unlikely(!(entry->data))) {
        free(entry);
        return NULL;
    }

    entry->base.target = target;
    entry->size = size;
    memcpy(entry->data, source, size);

    return entry;
}

bool w_entry_update(write_entry_t *entry, void const *source, size_t size) {
    if (unlikely(!entry)) return false;
    if (unlikely(size != entry->size)) return false;

    // Update write entry
    memcpy(entry->data, source, size);
    return true;
}

void r_entry_free(read_entry_t *entry) {
    if (unlikely(!entry)) return;
    free(entry);
}

void w_entry_free(write_entry_t *entry) {
    if (unlikely(!entry)) return;
    free(entry->data);
    free(entry);
}

// ============== set_t methods ============== 
struct set_t *set_init(bool is_write_set) {
    // Allocate memory for the set_t structure
    struct set_t *set = (struct set_t *)malloc(sizeof(struct set_t));
    if (unlikely(!set)) {
        return NULL;
    }

    set->entries = calloc(INITIAL_CAPACITY, sizeof(struct base_entry_t *));
    if (unlikely(!set->entries)) {
        free(set);
        return NULL;
    }

    // Initialize attributes
    set->count = 0;
    set->capacity = INITIAL_CAPACITY;
    set->is_write_set = is_write_set;
  
    return set;
}

bool w_set_add(struct set_t *set, void const *source, size_t size, void *target) {
    if (unlikely(!set)) return false;

    // struct timespec t1, t2, t3, t4;
    // if (unlikely(set->count > 1000))
    //     clock_gettime(CLOCK_MONOTONIC, &t1);

    // Check if pointer already in set
    if (likely(get_bit(set->lock_field, get_memory_lock_index(target)))) {
        for (size_t i = 0; i < set->count; i++) {
            struct base_entry_t *entry = set->entries[i];
            if (unlikely(entry->target == target)) {
                write_entry_t *w_entry = (write_entry_t *) entry;
                return w_entry_update(w_entry, source, size);
            }
        }
    }

    // if (unlikely(set->count > 1000))
    //     clock_gettime(CLOCK_MONOTONIC, &t2);

    // Increase capacity if needed
    if (unlikely(set->count >= set->capacity)) {
        if (unlikely(!set_grow(set))) {
            LOG_WARNING("w_set_add: failed to grow size of set %p\n", set);
            return false;
        }
    }

    // if (unlikely(set->count > 1000))
    //     clock_gettime(CLOCK_MONOTONIC, &t3);

    // Create a new write entry
    write_entry_t *entry = w_entry_create(source, size, target);
    if (unlikely(!entry)) {
        LOG_WARNING("w_set_add: failed to initialize write_entry_t in set %p\n", set);
        return false;
    }
    set->entries[set->count++] = &entry->base;

    // set memory lock field
    set_bit(set->lock_field, get_memory_lock_index(target));

    // if (unlikely(set->count > 1000)) {
    //     clock_gettime(CLOCK_MONOTONIC, &t4);
    
    //     // Make sure to update the print statement to reflect the correct function name
    //     printf("w_set_add: set size: %lu, target hash: %ld, Elapsed time: contains check: %ldns, grow: %ldns, insert: %ld \n", 
    //         set->count, get_memory_lock_index(target), (t2.tv_nsec - t1.tv_nsec), (t3.tv_nsec - t2.tv_nsec), (t4.tv_nsec - t3.tv_nsec));
    // }

    return true;
}

bool r_set_add(struct set_t* set, void* target) {
    if (unlikely(!set)) return false;

    // Check if pointer already in set
    if (likely(get_bit(set->lock_field, get_memory_lock_index(target)))) {
        for (size_t i = 0; i < set->count; i++) {
            struct base_entry_t *entry = set->entries[i];
            if (unlikely(entry->target == target)) {
                return true;
            }
        }
    }

    // Increase capacity if needed
    if (unlikely(set->count >= set->capacity)) {
        if (unlikely(!set_grow(set))) return false;
    }

    // Create a new write entry
    read_entry_t *entry = r_entry_create(target);
    if (unlikely(!entry)) return false;  
    set->entries[set->count++] = entry;

    // set memory lock field
    set_bit(set->lock_field, get_memory_lock_index(target));

    return true;
}

bool set_contains(struct set_t *set, void *target) {
    if (unlikely(!set)) return false;
    // Check if pointer already in set
    for (size_t i = 0; i < set->count; i++) {
        if (unlikely(set->entries[i]->target == target)) return true;
    }
    return false;
}

struct base_entry_t *set_get(struct set_t *set, void *key) {
    if (unlikely(!set)) return NULL;
    // Check if pointer already in set
    for (size_t i = 0; i < set->count; i++) {
        if (unlikely(set->entries[i]->target == key)) return set->entries[i];
    }
    return NULL;
}

bool set_read(struct set_t *set, void const *key, size_t size, void *dest) {
    if (unlikely(!set)) return false;
    if (unlikely(!set->is_write_set)) return false;     // method can only be used on write sets
    
    for (size_t i = 0; i < set->count; i++)
    {
        write_entry_t *entry = (write_entry_t *)set->entries[i];
        if (unlikely(entry->base.target == key)) {
            memcpy(dest, entry->data, size);
            return true;
        }
    }
    return false;
}

void set_free(struct set_t *set) {
    if (unlikely(!set)) return;

    // Iterate through all entries and free the dynamically allocated resources
    for (size_t i = 0; i < set->count; i++) {
        if (set->is_write_set) {
            w_entry_free((write_entry_t *)set->entries[i]);
        }
        else {
            r_entry_free((read_entry_t *)set->entries[i]);
        }
    }

    // Free the dynamically allocated array of pointers
    free(set->entries);

    // Free the set_t structure
    free(set);
}

size_t set_size(struct set_t *set) {
    return set->count;
}

bool set_grow(struct set_t *set) {
    size_t new_capacity = set->capacity * GROW_FACTOR;
    // Allocate new memroy bloc of increased size
    struct base_entry_t **new_entries = (struct base_entry_t **) realloc(set->entries, new_capacity * sizeof(struct base_entry_t *));
    if (unlikely(!new_entries)) {
        return false;
    }
    set->entries = new_entries;
    set->capacity = new_capacity;

    return true;
}