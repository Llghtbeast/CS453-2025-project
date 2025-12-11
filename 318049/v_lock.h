#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "tm.h"
#include "macros.h"

#define LOCKED -1

/**
 * @brief A lock that can only be taken exclusively. Contrarily to shared locks,
 * exclusive locks have wait/wake_up capabilities.
 */
typedef version_clock_t v_lock_t;

/** 
 * Initialize the given lock.
 * @param lock Lock to initialize
 * @return Whether the operation is a success
**/
bool v_lock_init(v_lock_t* lock);

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
 * Update version of the lock
 */
void v_lock_update(v_lock_t* lock, int new_val);

/**
 * Get version of the lock
 */
int v_lock_version(v_lock_t* lock);