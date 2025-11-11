#include "shared.h"

static inline struct shared * shared_to_ptr(shared_t shared) {
    if (unlikely(shared == invalid_shared)) {
        return NULL;
    }
    else return (struct shared *) shared;
}

shared_t shared_create(size_t size, size_t align) {
    struct shared* shared = (struct shared*) malloc(sizeof(struct shared));
    if (unlikely(!shared)) {
        return invalid_shared;
    }
    // We allocate the shared memory buffer such that its words are correctly
    // aligned.
    if (posix_memalign(&(shared->start), align, size) != 0) {
        free(shared);
        return invalid_shared;
    }
    memset(shared->start, 0, size);
    shared->version_clock = 1;      // Init to 1 to avoid problems with rv/wv initialized to 0 in txn creation
    shared->allocs      = NULL;
    shared->size        = size;
    shared->align       = align;
    return shared;
}

void shared_destroy(shared_t shared) {
    struct shared* s =  shared_to_ptr(shared);
    while (s->allocs) { // Free allocated segments
        segment_list tail = s->allocs->next;
        v_lock_cleanup(&(s->allocs->lock));
        free(s->allocs);
        s->allocs = tail;
    }
    free(s->start);
    free(s);
}

void* shared_start(shared_t shared) {
    return ((struct shared *) shared)->start;
}

size_t shared_size(shared_t shared) {
    return ((struct shared *) shared)->size;
}

size_t shared_align(shared_t shared) {
    return ((struct shared *) shared)->align;
}

alloc_t shared_alloc(shared_t shared, size_t size, void **target) {
    struct shared *s = shared_to_ptr(shared);
    size_t align = s->align;
    align = align < sizeof(struct segment_node*) ? sizeof(void*) : align;

    struct segment_node* sn;
    if (unlikely(posix_memalign((void**)&sn, align, sizeof(struct segment_node) + size) != 0)) // Allocation failed
        return nomem_alloc;

    // Insert in the linked list
    sn->prev = NULL;
    sn->next = s->allocs;
    if (!v_lock_init(&(sn->lock))) {
        free(sn);
        return nomem_alloc;
    }
    if (sn->next) sn->next->prev = sn;
    s->allocs = sn;

    void* segment = (void*) ((uintptr_t) sn + sizeof(struct segment_node));
    memset(segment, 0, size);
    *target = segment;
    return success_alloc;
}

bool shared_free(shared_t shared, void *target) {
    struct segment_node* sn = (struct segment_node*) ((uintptr_t) target - sizeof(struct segment_node));

    // Remove from the linked list
    if (sn->prev) sn->prev->next = sn->next;
    else ((struct shared*) shared)->allocs = sn->next;
    if (sn->next) sn->next->prev = sn->prev;

    v_lock_cleanup(&(sn->lock));
    free(sn);
    return true;
}
