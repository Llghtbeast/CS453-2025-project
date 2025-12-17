#include "map.h"

// ============== hash_map_t methods ============== 
struct entry_index_t *entry_index_create(const void *target, uintptr_t id) {
    struct entry_index_t *entry_index = malloc(sizeof(struct  entry_index_t));
    if (unlikely(!entry_index)) return NULL;

    entry_index->target = target;
    entry_index->index = id;
    entry_index->next = NULL;

    return entry_index;
}

struct hash_map_t *hash_map_create()  {
    struct hash_map_t *hash_map = calloc(1, sizeof(struct hash_map_t));
    if (unlikely(!hash_map)) return NULL;

    return hash_map;
}

void hash_map_free(struct hash_map_t *hash_map) {
    if (unlikely(!hash_map)) return;

    for (size_t i = 0; i < VLOCK_NUM; i++) {
        struct entry_index_t *current = hash_map->map[i];
        
        // Traverse the linked list at this bucket
        while (current != NULL) {
            struct entry_index_t *temp = current;
            current = current->next;
            
            // Free the individual node
            free(temp);
        }
    }

    // Finally, free the hash map structure itself
    free(hash_map);
}

struct entry_index_t *hash_map_get(struct hash_map_t *hash_map, const void *target) {
    if (unlikely(!hash_map)) return NULL;

    uintptr_t index = get_memory_lock_index(target);
    struct entry_index_t *entry_index = hash_map->map[index];

    // return entry index that contains target pointer
    while (entry_index != NULL) {
        if (unlikely(entry_index->target == target)) return entry_index;
        entry_index = entry_index->next;
    }
    return NULL;
}

void hash_map_add(struct hash_map_t *hash_map, const void *target, size_t id) {
    if (unlikely(!hash_map)) return;

    uintptr_t index = get_memory_lock_index(target);
    struct entry_index_t *entry_index = hash_map->map[index];

    if (likely(!entry_index)) {
        hash_map->map[index] = entry_index_create(target, id);
        return;
    }

    // Go to last entry_index in linked list and append a new entry
    while (entry_index->next != NULL) entry_index = entry_index->next;

    entry_index->next = entry_index_create(target, id);
    return;
}

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

    // Init hash map
    set->entries_index = hash_map_create();
    if (unlikely(!set->entries_index)) {
        free(set->entries);
        free(set);
        return NULL;
    }
    
    // Initialize attributes
    set->count = 0;
    set->capacity = INITIAL_CAPACITY;
    set->is_write_set = is_write_set;
    set->collision = false;
  
    return set;
}

bool w_set_add(struct set_t *set, void const *source, size_t size, void *target) {
    if (unlikely(!set)) return false;

    // Check if pointer already in set
    struct entry_index_t *entry_index = hash_map_get(set->entries_index, target);
    if (unlikely(entry_index != NULL)) {
        write_entry_t *w_entry = (write_entry_t *) set->entries[entry_index->index];
        return w_entry_update(w_entry, source, size);
    }

    // Increase capacity if needed
    if (unlikely(set->count >= set->capacity)) {
        if (unlikely(!set_grow(set))) {
            LOG_WARNING("w_set_add: failed to grow size of set %p\n", set);
            return false;
        }
    }

    // Create a new write entry
    write_entry_t *entry = w_entry_create(source, size, target);
    if (unlikely(!entry)) {
        LOG_WARNING("w_set_add: failed to initialize write_entry_t in set %p\n", set);
        return false;
    }
    hash_map_add(set->entries_index, target, set->count);
    set->entries[set->count++] = &entry->base;

    return true;
}

bool r_set_add(struct set_t* set, void* target) {
    if (unlikely(!set)) return false;

    // Check if pointer already in set
    struct entry_index_t *entry_index = hash_map_get(set->entries_index, target);
    if (unlikely(entry_index != NULL)) return true;

    // Increase capacity if needed
    if (unlikely(set->count >= set->capacity)) {
        if (unlikely(!set_grow(set))) return false;
    }

    // Create a new write entry
    read_entry_t *entry = r_entry_create(target);
    if (unlikely(!entry)) return false;  
    hash_map_add(set->entries_index, target, set->count);
    set->entries[set->count++] = entry;

    return true;
}

bool set_contains(struct set_t *set, void *target) {
    if (unlikely(!set)) return false;
    // Check if pointer already in set
    return hash_map_get(set->entries_index, target) != NULL;
}

struct base_entry_t *set_get(struct set_t *set, void *key) {
    if (unlikely(!set)) return NULL;
    // Check if pointer already in set
    struct entry_index_t *entry_index = hash_map_get(set->entries_index, key);
    if (likely(!entry_index)) return NULL;
    return set->entries[entry_index->index];
}

bool set_read(struct set_t *set, void const *key, size_t size, void *dest) {
    if (unlikely(!set)) return false;
    if (unlikely(!set->is_write_set)) return false;     // method can only be used on write sets
    
    struct entry_index_t *entry_index = hash_map_get(set->entries_index, key);
    if (likely(!entry_index)) return false;

    write_entry_t *entry = (write_entry_t *)set->entries[entry_index->index];
    memcpy(dest, entry->data, size);
    return true;
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

    hash_map_free(set->entries_index);

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
