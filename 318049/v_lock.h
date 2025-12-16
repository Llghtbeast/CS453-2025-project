#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "helper.h"
#include "tm.h"
#include "macros.h"

/**
 * @brief A versioned spinlock.
 */
typedef version_clock_t v_lock_t;

/** 
 * Initialize the given lock.
 * @param lock Lock to initialize
 * @return Whether the operation is a success
**/
void v_lock_init(v_lock_t* lock);

/** 
 * Clean up the given lock.
 * @param lock Lock to clean up
**/
void v_lock_cleanup(v_lock_t* lock);

/**
 * Try acquiring lock
 * @param lock Lock to acquire
 * @return true if lock was acquired, false otherwise
 */
bool v_lock_acquire(v_lock_t* lock);

/**
 * Release lock
 * @param lock Lock to release
 */
void v_lock_release(v_lock_t* lock);

/**
 * Update version of the lock and release it
 */
void v_lock_release_and_update(v_lock_t* lock, int val);

/**
 * Get version of the lock
 */
int v_lock_version(v_lock_t* lock);

// ============= Global clock implementation ============= 
/**
 * @brief Global clock implementation
 */
typedef version_clock_t global_clock_t;

void global_clock_init(global_clock_t *global_clock);

void global_clock_cleanup(global_clock_t *global_clock);

int global_clock_load(global_clock_t *global_clock);

int global_clock_increment_and_fetch(global_clock_t *global_clock);

// ============= Segment allocation lock implementation ============= 
typedef pthread_mutex_t alloc_mutex_t;

int alloc_mutex_init(alloc_mutex_t *lock);

int alloc_mutex_cleanup(alloc_mutex_t *lock);

int alloc_mutex_acquire(alloc_mutex_t *lock);

int alloc_mutex_release(alloc_mutex_t *lock);
