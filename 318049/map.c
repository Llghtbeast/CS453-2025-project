#include "map.h"

// ============== write_entry_t methods ============== 
struct write_entry_t * write_entry_create(void const *source, size_t size, void *target)
{
    // Allocate memory for struct
    struct write_entry_t *we = malloc(sizeof(struct write_entry_t));
    if (unlikely(!we)) return NULL;

    // Try allocating memory for data 
    we->data = malloc(size);
    if (unlikely(!(we->data))) {
        write_entry_free(we);
        return NULL;
    }

    // Create write_entry
    we->target = target;
    we->size = size;
    memcpy(we->data, source, size);

    return we;
}

bool write_entry_update(struct write_entry_t *we, void const *source, size_t size)
{
    if (unlikely(!we)) return false;
    if (size != we->size) return false;

    // Update write entry
    memcpy(we->data, source, size);
}

void write_entry_free(struct write_entry_t *we)
{
    free(we->data);
    free(we);
}

// ============== write_set_t methods ============== 
struct write_set_t *write_set_init()
{
    // Allocate memory for the write_set_t structure
    struct write_set_t *ws = (struct write_set_t *)malloc(sizeof(struct write_set_t));
    if (unlikely(!ws)) {
        return NULL;
    }

    // Initialize attributes
    ws->count = 0;
    ws->capacity = INITIAL_CAPACITY;
    ws->entries = NULL;
  
    return ws;
}

bool write_set_add(struct write_set_t *ws, void const *source, size_t size, void *target)
{
    if (unlikely(!ws)) return false;

    // Check if pointer already in write_set
    for (size_t i = 0; i < ws->count; i++) {
        struct write_entry_t *entry = ws->entries[i];
        if (entry->target == target) {
            write_entry_update(entry, source, size);
            return true;
        }
    }

    // Increase capacity if needed
    if (ws->count >= ws->capacity) {
        ws->capacity *= GROW_FACTOR;
    }

    // Create a new write entry
    struct write_entry_t *we = write_entry_create(source, size, target);
    ws->entries[ws->count++] = we;

    return true;
}

bool write_set_contains(struct write_set_t *ws, void *target)
{
    if (unlikely(!ws)) return false;
    // Check if pointer already in write_set
    for (size_t i = 0; i < ws->count; i++) {
        if (ws->entries[i]->target == target) return true;
    }
    return false;
}

bool write_set_get(struct write_set_t *ws, void const *source, size_t size, void *target)
{
    for (size_t i = 0; i < ws->count; i++)
    {
        struct write_entry_t *we = ws->entries[i];
        if (we->target == source && we->size == size) {
            memcpy(target, we->data, we->size);
            return true;
        }
    }
    return false;
}

void write_set_free(struct write_set_t *ws)
{
    if (unlikely(!ws)) {
        return;
    }

    // Iterate through all entries and free the dynamically allocated resources
    for (size_t i = 0; i < ws->count; i++) {
        write_entry_free(ws->entries[i]);
    }

    // Free the dynamically allocated array of pointers
    free(ws->entries);

    // Free the write_set_t structure
    free(ws);
}

size_t write_set_size(struct write_set_t *ws)
{
    return ws->count;
}
