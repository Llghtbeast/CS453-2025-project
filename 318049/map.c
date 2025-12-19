#include "map.h"

// ============== helper methods ==============
/**
 * Find the index of the entry containing this target. 
 * If not contained in the set, return set->capacity
 */
static size_t set_find(struct set_t *set, void const *target);

/**
 * Find the next ideal available bucket for the target
 */ 
static size_t set_find_next_free(struct set_t *set, void const *target);

static void w_set_add_help(struct set_t *set, write_entry_t *entry);

static void r_set_add_help(struct set_t *set, read_entry_t *entry);

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
    memcpy(entry->data, source, size);

    return entry;
}

bool w_entry_update(write_entry_t *entry, void const *source, size_t size) {
    if (unlikely(!entry)) return false;

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
struct set_t *set_init(bool is_write_set, size_t data_size) {
    // Allocate memory for the set_t structure
    struct set_t *set = (struct set_t *)malloc(sizeof(struct set_t));
    if (unlikely(!set)) {
        LOG_TEST("set_init: initial set allocation failed!\n");
        return NULL;
    }

    set->entries = calloc(INITIAL_CAPACITY, sizeof(struct base_entry_t *));
    if (unlikely(!set->entries)) {
        LOG_TEST("set_init: set->entries allocation failed!\n");
        free(set);
        return NULL;
    }

    size_t num_words = (INITIAL_CAPACITY + 63) / 64;
    set->occupied_field = calloc(num_words, sizeof(uint64_t));
    if (unlikely(!set->occupied_field)) {
        LOG_TEST("set_init: set->occupied_field allocation failed!\n");
        free(set->entries);
        free(set);
        return NULL;
    }

    // Initialize attributes
    set->is_write_set = is_write_set;
    set->data_size = data_size;
    set->count = 0;
    set->capacity = INITIAL_CAPACITY;
  
    return set;
}

bool w_set_add(struct set_t *set, void const *source, size_t size, void *target) {
    if (unlikely(!set)) return false;
    if (unlikely(size != set->data_size)) return false;
    
    // Increase capacity if needed
    LOG_DEBUG("w_set_add: adding element to set %p of size %lu and capacity %lu\n", size, set->count, set->capacity);
    if (unlikely(set->count >= set->capacity * MAX_LOAD_FACTOR)) {
        if (unlikely(!set_grow(set))) {
            LOG_WARNING("w_set_add: failed to grow size of set %p\n", set);
            return false;
        }
        LOG_DEBUG("w_set_add: increased capacity of set %p to %lu\n", set, set->capacity);
    }

    // See if target is already in set and update
    size_t index = set_find(set, target);
    LOG_DEBUG("w_set_add: target %p in set %p (set->capacity=%lu) has: hash=%lu, index=%lu\n", target, set, set->capacity, set_hash(target, set->capacity), index);
    if (unlikely(index != set->capacity)) {
        write_entry_t *w_entry = (write_entry_t *) set->entries[index];
        return w_entry_update(w_entry, source, size);
    }

    // Create a new write entry
    write_entry_t *entry = w_entry_create(source, size, target);
    if (unlikely(!entry)) {
        LOG_WARNING("w_set_add: failed to initialize write_entry_t in set %p\n", set);
        return false;
    }
    w_set_add_help(set, entry);
    set->count++;

    return true;
}

bool r_set_add(struct set_t* set, void* target) {
    if (unlikely(!set)) return false;

    // Increase capacity if needed
    if (unlikely(set->count >= set->capacity * MAX_LOAD_FACTOR)) {
        if (unlikely(!set_grow(set))) {
            LOG_WARNING("w_set_add: failed to grow size of set %p\n", set);
            return false;
        }
    }

    // See if target is already in set and update
    size_t index = set_find(set, target);
    if (unlikely(index != set->capacity)) return true;

    // Create a new write entry
    read_entry_t *entry = r_entry_create(target);
    if (unlikely(!entry)) return false;  
    r_set_add_help(set, entry);
    set->count++;

    return true;
}

bool set_contains(struct set_t *set, void *target) {
    if (unlikely(!set)) return false;

    size_t index = set_find(set, target);
    return index != set->capacity;
}

struct base_entry_t *set_get(struct set_t *set, void *key) {
    if (unlikely(!set)) return NULL;
    
    size_t index = set_find(set, key);
    if (unlikely(index != set->capacity)) return set->entries[index];
    return NULL;
}

bool set_read(struct set_t *set, void const *key, size_t size, void *dest) {
    if (unlikely(!set)) return false;
    if (unlikely(!set->is_write_set)) return false;     // method can only be used on write sets
    
    size_t index = set_find(set, key);
    if (likely(index != set->capacity)) {
        write_entry_t *entry = (write_entry_t *)set->entries[index];
        memcpy(dest, entry->data, size);
        return true;
    }
    return false;
}

void set_free(struct set_t *set) {
    if (unlikely(!set)) return;

    // Iterate through all entries and free the dynamically allocated resources
    for (size_t i = 0; i < set->capacity; i++) {
        if (get_bit(set->occupied_field, i)) {
            if (set->is_write_set) {
                w_entry_free((write_entry_t *)set->entries[i]);
            }
            else {
                r_entry_free((read_entry_t *)set->entries[i]);
            }
        }
    }

    // Free the dynamically allocated array of pointers
    free(set->entries);
    free(set->occupied_field);

    // Free the set_t structure
    free(set);
}

size_t set_size(struct set_t *set) {
    return set->count;
}

bool set_grow(struct set_t *set) {
    if (unlikely(!set)) return false;
    size_t old_capacity = set->capacity;
    struct base_entry_t **old_entries = set->entries;
    uint64_t *old_occupied = set->occupied_field;

    // Allocate new memroy bloc of increased size
    set->capacity *= GROW_FACTOR;
    set->entries = calloc(set->capacity, sizeof(struct base_entry_t *));

    size_t num_words = (set->capacity + 63) / 64;
    set->occupied_field = calloc(num_words, sizeof(uint64_t));
    if (unlikely(!set->entries || !set->occupied_field)) {
        free(old_entries);
        free(old_occupied);
        return false;
    }

    for (size_t i = 0; i < old_capacity; i++) {
        if (get_bit(old_occupied, i)) {
            // re-hash
            if (set->is_write_set) {
                w_set_add_help(set, (write_entry_t *) old_entries[i]);
            } else {
                r_set_add_help(set, (read_entry_t *) old_entries[i]);
            }
        }
    }

    free(old_entries);
    free(old_occupied);
    return true;
}

void set_get_lock_field(struct set_t *set, uint64_t *lock_field) {
    if (unlikely(!set || !lock_field)) return;
    if (unlikely(!set->is_write_set)) return;   // Can only use on write sets

    memset(lock_field, 0, (VLOCK_NUM / 64) * sizeof(uint64_t));

    for (size_t i = 0; i < set->capacity; i++) {
        if (get_bit(set->occupied_field, i)) {
            void *target = set->entries[i]->target;
            int lock_index = get_memory_lock_index(target);
        
            set_bit(lock_field, lock_index);
        }
    }
}

// ============= helper methods implementation =============
size_t set_find(struct set_t *set, void const *target) {
    size_t index = set_hash(target, set->capacity);

    // Linear probing to find the target in the table, if an empty bucket is encountered, then the value is not in the set
    while (get_bit(set->occupied_field, index)) {
        if (likely(set->entries[index]->target == target)) return index;
        index = (index + 1) % set->capacity;
    }
    return set->capacity;
}

size_t set_find_next_free(struct set_t *set, void const *target) {
    size_t index = set_hash(target, set->capacity);

    // Linear probing to find the first empty slot in the table
    while (get_bit(set->occupied_field, index)) {
        index = (index + 1) % set->capacity;
    }
    return index;
}

void w_set_add_help(struct set_t *set, write_entry_t *entry) {
    size_t index = set_find_next_free(set, entry->base.target);
    set->entries[index] = (struct base_entry_t *)entry;
    set_bit(set->occupied_field, index);
}

void r_set_add_help(struct set_t *set, read_entry_t *entry) {
    size_t index = set_find_next_free(set, entry->target);
    set->entries[index] = (struct base_entry_t *)entry;
    set_bit(set->occupied_field, index);
}
