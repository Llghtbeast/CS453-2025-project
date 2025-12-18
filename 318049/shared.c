#include "shared.h"

struct region_t *region_create(size_t size, size_t align) {
    struct region_t* region = (struct region_t*) malloc(sizeof(struct region_t));
    if (unlikely(!region)) {
        return NULL;
    }

    // We allocate the region memory buffer such that its words are correctly aligned.
    if (unlikely(posix_memalign(&(region->start), align, size))) {
        free(region);
        return NULL;
    }

    // Allocating to_free memory region
    region->to_free_count = 0;
    region->to_free_capacity = INITIAL_TO_FREE_CAPACITY;
    region->to_free = malloc(region->to_free_capacity * sizeof(region->to_free_capacity));
    if (unlikely(!region->to_free)) {
        free(region->start);
        free(region);
        return NULL;
    }

    // Initialize the locks
    if (unlikely(pthread_mutex_init(&region->alloc_lock, NULL) ||
                pthread_mutex_init(&region->append_to_free_lock, NULL)) ||
                pthread_rwlock_init(&region->free_lock, NULL)
    ) {
        free(region->to_free);
        free(region->start);
        free(region);
        return NULL;
    }

    // Init the global version lock
    global_clock_init(&region->version_clock);
    
    // Init the memory locks
    for (size_t i = 0; i < VLOCK_NUM; i++) {
        v_lock_init(&region->v_locks[i]);
    }
    
    memset(region->start, 0, size);
    region->allocs      = NULL;
    region->size        = size;
    region->align       = align;
    return region;
}

void region_destroy(struct region_t *region) {
    // Free allocated segments
    while (region->allocs) { 
        segment_list tail = region->allocs->next;
        free(region->allocs);
        region->allocs = tail;
    }
    free(region->to_free);

    // Cleanup locks
    pthread_mutex_destroy(&region->alloc_lock);
    pthread_mutex_destroy(&region->append_to_free_lock);
    pthread_rwlock_destroy(&region->free_lock);

    global_clock_cleanup(&region->version_clock);
    for (size_t i = 0; i < VLOCK_NUM; i++) {
        v_lock_cleanup(&region->v_locks[i]);
    }
    
    // Free initial memory region
    free(region->start);
    free(region);
}

void* region_start(struct region_t *region) {
    return region->start;
}

size_t region_size(struct region_t *region) {
    return region->size;
}

size_t region_align(struct region_t *region) {
    return region->align;
}

int region_update_version_clock(struct region_t *region) {
    return global_clock_increment_and_fetch(&region->version_clock);
}

struct segment_node_t *region_alloc(struct region_t *region, size_t size) {
    size_t align = region->align;
    align = align < sizeof(struct segment_node_t*) ? sizeof(void*) : align;

    struct segment_node_t* node;
    if (unlikely(posix_memalign((void**)&node, align, sizeof(struct segment_node_t) + size) != 0)) // Allocation failed
        return NULL;

    // Insert in the linked list
    pthread_mutex_lock(&region->alloc_lock);
    node->prev = NULL;
    node->next = region->allocs;
    if (unlikely(node->next)) node->next->prev = node;
    region->allocs = node;
    pthread_mutex_unlock(&region->alloc_lock);

    return node;
}

bool region_append_to_free(struct region_t *region, void** txn_to_free, size_t txn_to_free_count) {
    // Append to_free list to the end of region->to_free list
    pthread_mutex_lock(&region->append_to_free_lock);
    // Increase size of to_free dynamic array if needed
    if (region->to_free_count + txn_to_free_count > region->to_free_capacity) {
        region->to_free_capacity *= GROW_FACTOR;
        region->to_free = realloc(region->to_free, region->to_free_capacity * sizeof(void *));
        if (unlikely(!region->to_free)) {
            pthread_mutex_unlock(&region->append_to_free_lock);
            return false;
        }
    }

    // Append the new to_free pointers
    for (size_t i = 0; i < txn_to_free_count; i++) {
        region->to_free[region->to_free_count++] = txn_to_free[i];
    }
    pthread_mutex_unlock(&region->append_to_free_lock);
    return true;
}

bool region_free(struct region_t *region) {
    // Free all queued to-free memory regions
    pthread_rwlock_wrlock(&region->free_lock);
    for (size_t i = 0; i < region->to_free_count; i++) {
        struct segment_node_t* node = (struct segment_node_t*) ((uintptr_t) region->to_free[i] - sizeof(struct segment_node_t));

        // remove from linked list
        if (unlikely(node->prev)) node->prev->next = node->next;
        else region->allocs = node->next;
        if (unlikely(node->next)) node->next->prev = node->prev;
        free(node);
    }
    pthread_rwlock_unlock(&region->free_lock);
    return true;
}

v_lock_t *region_get_memory_lock_from_index(struct region_t *region, uintptr_t index) {
    return &region->v_locks[index];
}

v_lock_t *region_get_memory_lock_from_ptr(struct region_t *region, void const *addr) {
    return &region->v_locks[get_memory_lock_index(addr)];
}
