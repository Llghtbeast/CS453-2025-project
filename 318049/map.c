#include "map.h"

// ============== entry_t methods ============== 
struct entry_t *entry_create(void const *source) {}

struct entry_t *entry_create(void const *source, size_t size, void *target) {
    // Allocate memory for struct
    struct entry_t *entry = malloc(sizeof(struct entry_t));
    if (unlikely(!entry)) return NULL;

    // Try allocating memory for data 
    entry->data = malloc(size);
    if (unlikely(!(entry->data))) {
        entry_free(entry);
        return NULL;
    }

    // Create entr
    entry->target = target;
    entry->size = size;
    memcpy(entry->data, source, size);

    return entry;
}

bool entry_update(struct entry_t *entry, void const *source, size_t size) {
    if (unlikely(!entry)) return false;
    if (size != entry->size) return false;

    // Update write entry
    memcpy(entry->data, source, size);
    return true;
}

void entry_free(struct entry_t *entry) {
    free(entry->data);
    free(entry);
}

// ============== set_t methods ============== 
struct set_t *set_init() {
    // Allocate memory for the set_t structure
    struct set_t *set = (struct set_t *)malloc(sizeof(struct set_t));
    if (unlikely(!set)) {
        return NULL;
    }

    // Initialize attributes
    set->count = 0;
    set->capacity = INITIAL_CAPACITY;
    set->entries = calloc(INITIAL_CAPACITY, sizeof(struct entry_t *));
  
    return set;
}

bool w_set_add(struct set_t *set, void const *source, size_t size, void *target) {
    if (unlikely(!set)) return false;

    // Check if pointer already in set
    for (size_t i = 0; i < set->count; i++) {
        struct entry_t *entry = set->entries[i];
        if (entry->target == target) {
            return entry_update(entry, source, size);
        }
    }

    // Increase capacity if needed
    if (set->count >= set->capacity) {
        if (!set_grow(set)) return false;
    }

    // Create a new write entry
    struct entry_t *entry = entry_create(source, size, target);
    if (!entry) return false;  
    set->entries[set->count++] = entry;

    return true;
}

bool r_set_add(struct set_t *set, void const *source) {
    if (unlikely(!set)) return false;

    // Check if pointer already in set
    for (size_t i = 0; i < set->count; i++) {
        struct entry_t *entry = set->entries[i];
        if (entry->target == target) {
            return entry_update(entry, source, size);
        }
    }

    // Check if there is enough space to add
    if (set->count >= set->capacity) {
        if (!set_grow(set)) return false;
    }

    // Create a new write entry
    struct entry_t *entry = entry_create(source, size, target);
    if (!entry) return false;  
    set->entries[set->count++] = entry;

    return true;
}

bool set_contains(struct set_t *set, void *target) {
    if (unlikely(!set)) return false;
    // Check if pointer already in set
    for (size_t i = 0; i < set->count; i++) {
        if (set->entries[i]->target == target) return true;
    }
    return false;
}

bool set_read(struct set_t *set, void const *key, size_t size, void *dest) {
    for (size_t i = 0; i < set->count; i++)
    {
        struct entry_t *entry = set->entries[i];
        if (entry->target == key && entry->size == size) {
            memcpy(dest, entry->data, entry->size);
            return true;
        }
    }
    return false;
}

void set_free(struct set_t *set) {
    if (unlikely(!set)) {
        return;
    }

    // Iterate through all entries and free the dynamically allocated resources
    for (size_t i = 0; i < set->count; i++) {
        entry_free(set->entries[i]);
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
    // Increase capacity if needed
    size_t new_capacity = set->capacity * GROW_FACTOR;
    // Allocate new memroy bloc of increased size
    struct entry_t **new_entries = realloc(set->entries, new_capacity * sizeof(struct entry_t *));
    if (!new_entries) {
        return false;
    }
    set->entries = new_entries;
    set->capacity = new_capacity;

    return true;
}
