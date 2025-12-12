#pragma once

// Requested feature: posix_memalign
#define _POSIX_C_SOURCE   200809L

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "v_lock.h"
#include "tm.h"
#include "macros.h"

/**
 * @brief Segment of shared memory.
 */
struct segment_node_t {
    struct segment_node_t* prev;
    struct segment_node_t* next;

    size_t size;
    void* data;
    v_lock_t** locks;
};
typedef struct segment_node_t* segment_list;

struct segment_node_t *segment_init(size_t size, size_t align);

void segment_free(struct segment_node_t *node);

// ============ Shared region ============ 
/**
 * @brief List of shared memory segments
 */
struct region_t {
    version_clock_t version_clock;
    
    void* start;
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

struct segment_node_t * region_alloc(struct region_t *, size_t size);

bool region_free(struct region_t *, struct segment_node_t *node);