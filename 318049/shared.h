#pragma once

// Requested feature: posix_memalign
#define _POSIX_C_SOURCE   200809L

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "v_lock.h"
#include "tm.h"
#include "macros.h"

#define VLOCK_NUM 2<<20

/**
 * @brief Segment of shared memory.
 */
struct segment_node_t {
    struct segment_node_t* prev;
    struct segment_node_t* next;
};
typedef struct segment_node_t* segment_list;

// ============ Shared region ============ 
/**
 * @brief List of shared memory segments
 */
struct region_t {
    pthread_mutex_t alloc_lock;     // Lock to seize when allocating new memory block
    v_lock_t v_locks[VLOCK_NUM];    // Lock to acquire when writing to corresponding word in memory
    global_clock_t version_clock;  // Global version lock
    
    void* start;
    size_t size;
    size_t align;
    
    segment_list allocs;
    segment_list last;
};

struct region_t *region_create(size_t size, size_t align);

void region_destroy(struct region_t *);

void* region_start(struct region_t *);

size_t region_size(struct region_t *);

size_t region_align(struct region_t *);

int region_update_version_clock(struct region_t *);

struct segment_node_t *region_alloc(struct region_t *, size_t size);

bool region_free(struct region_t *, struct segment_node_t *node);

v_lock_t *region_get_memory_lock(struct region_t *, void *addr);