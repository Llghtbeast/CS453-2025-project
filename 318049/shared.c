#include "shared.h"

struct region_t *region_create(size_t size, size_t align) {
    struct region_t* region = (struct region_t*) malloc(sizeof(struct region_t));
    if (unlikely(!region)) {
        return NULL;
    }
    // We allocate the region memory buffer such that its words are correctly aligned.
    if (posix_memalign(&(region->start), align, size) != 0) {
        free(region);
        return NULL;
    }
    memset(region->start, 0, size);
    region->version_clock;      // Init to 1 to avoid problems with rv/wv initialized to 0 in txn creation
    region->allocs      = NULL;
    region->size        = size;
    region->align       = align;
    global_clock_init(region->version_clock);
    return region;
}

void region_destroy(struct region_t *region) {
    while (region->allocs) { // Free allocated segments
        segment_list tail = region->allocs->next;
        v_lock_cleanup(&(region->allocs->lock));
        free(region->allocs);
        region->allocs = tail;
    }
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

version_clock_t region_update_version_clock(struct region_t *region) {
    struct region_t *s = region_to_ptr(region);
    return atomic_fetch_add(&(region->version_clock), 1);
}

alloc_t region_alloc(struct region_t *region, size_t size, void **target) {
    size_t align = region->align;
    align = align < sizeof(struct segment_node_t*) ? sizeof(void*) : align;

    struct segment_node_t* sn;
    if (unlikely(posix_memalign((void**)&sn, align, sizeof(struct segment_node_t) + size) != 0)) // Allocation failed
        return nomem_alloc;

    // Insert in the linked list
    sn->prev = NULL;
    sn->next = region->allocs;
    if (sn->next) sn->next->prev = sn;
    region->allocs = sn;
    v_lock_init(&sn->lock);

    void* segment = (void*) ((uintptr_t) sn + sizeof(struct segment_node_t));
    memset(segment, 0, size);
    *target = segment;
    return success_alloc;
}

bool region_free(struct region_t *region, void *target) {
    struct segment_node_t* sn = (struct segment_node_t*) ((uintptr_t) target - sizeof(struct segment_node_t));

    // Remove from the linked list
    if (sn->prev) sn->prev->next = sn->next;
    else ((struct region_t*) region)->allocs = sn->next;
    if (sn->next) sn->next->prev = sn->prev;

    v_lock_cleanup(&(sn->lock));
    free(sn);
    return true;
}
