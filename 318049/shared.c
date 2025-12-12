#include "shared.h"

struct segment_node_t *segment_init(size_t size, size_t align) {
    struct segment_node_t *node = malloc(sizeof(struct segment_node_t));
    if (unlikely(!node)) return NULL;
    if (unlikely(posix_memalign(&node->data, align, size))) {
        free(node);
        return NULL;
    }

    node->size = size;
    memset(node->data, 0, size);
    
    // Allocate space for locks
    node->locks = calloc(size / align, sizeof(v_lock_t));
    if (unlikely(!node->locks)) {
        free(node->data);
        free(node);
        return NULL;
    }
    // Init locks
    for (size_t i = 0; i < size/align; i++) {
        v_lock_init(&node->locks[i]);
    }
    
    return node;
}

void segment_free(struct segment_node_t *node) {
    free(node->locks);
    free(node->data);
    free(node);
}

// ============ Shared region methods ============
struct region_t *region_create(size_t size, size_t align) {
    struct region_t* region = (struct region_t*) malloc(sizeof(struct region_t));
    if (unlikely(!region)) {
        return NULL;
    }
    // Allocate initial non-free-able segment
    struct segment_node_t *node = segment_init(size, align);
    if (unlikely(!node)) {
        free(region);
        return NULL;
    }

    global_clock_init(region->version_clock);
    region->start       = node->data;
    region->align       = align;
    region->allocs      = node;
    region->last        = node;
    return region;
}

void region_destroy(struct region_t *region) {
    while (region->allocs) { // Free allocated segments
        segment_list tail = region->allocs->next;
        segment_free(region->allocs);
        region->allocs = tail;
    }
    free(region->start);
    free(region);
}

void* region_start(struct region_t *region) {
    return region->start;
}

size_t region_size(struct region_t *region) {
    return region->allocs->size;
}

size_t region_align(struct region_t *region) {
    return region->align;
}

int region_update_version_clock(struct region_t *region) {
    return global_clock_increment_and_fetch(&region->version_clock);
}

struct segment_node_t *region_alloc(struct region_t *region, size_t size) {
    // size_t align = region->align;
    // align = align < sizeof(struct segment_node_t*) ? sizeof(void*) : align;

    struct segment_node_t* node = segment_init(size, region->align);
    if (unlikely(!node)) {
        return NULL;
    }

    // Insert in the linked list
    node->next = NULL;
    node->prev = region->last;
    region->last->next = node;
    region->last = node;

    return node;
}

bool region_free(struct region_t *region, struct segment_node_t *node) {
    if (unlikely(!region || !node)) return false;
    
    // Remove from the linked list
    if (region->last == node) region->last = node->prev;
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;

    segment_free(node);
    return true;
}
