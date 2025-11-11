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
 * @brief List of dynamically allocated segments.
 */
struct segment_node {
    struct segment_node* prev;
    struct segment_node* next;
    
    struct v_lock_t lock;
};
typedef struct segment_node* segment_list;

struct shared {
    version_clock_t version_clock;
    
    void* start;
    size_t size;
    size_t align;
    
    segment_list allocs;
};

shared_t shared_create(size_t size, size_t align);

void shared_destroy(shared_t shared);

void* shared_start(shared_t shared);

size_t shared_size(shared_t shared);

size_t shared_align(shared_t shared);

alloc_t shared_alloc(shared_t shared, size_t size, void** target);

bool shared_free(shared_t shared, void* target);