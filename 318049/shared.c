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

    // Initialize the segment allocation lock
    if (unlikely(alloc_mutex_init(&region->alloc_lock))) {
        free(region->start);
        free(region);
        return NULL;
    }

    // Init the global version lock
    global_clock_init(region->version_clock);
    
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

    // Cleanup locks
    global_clock_cleanup(&region->version_clock);
    alloc_mutex_cleanup(&region->alloc_lock);
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
    alloc_mutex_acquire(&region->alloc_lock);
    node->prev = NULL;
    node->next = region->allocs;
    if (node->next) node->next->prev = node;
    region->allocs = node;
    alloc_mutex_release(&region->alloc_lock);

    return node;
}

bool region_free(struct region_t *region, struct segment_node_t *node) {
    // Remove from the linked list
    alloc_mutex_acquire(&region->alloc_lock);
    if (node->prev) node->prev->next = node->next;
    else region->allocs = node->next;
    if (node->next) node->next->prev = node->prev;
    alloc_mutex_release(&region->alloc_lock);

    free(node);
    return true;
}

v_lock_t *region_get_memory_lock(struct region_t *region, void *addr) {
    return &region->v_locks[(uintptr_t) addr % VLOCK_NUM];
}
