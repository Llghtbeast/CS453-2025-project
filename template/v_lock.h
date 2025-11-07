#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "tm.h"

/**
 * @brief A lock that can only be taken exclusively. Contrarily to shared locks,
 * exclusive locks have wait/wake_up capabilities.
 */
struct v_lock_t {
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    version_clock_t version_clock;
    bool lock_bit;
};

/** Initialize the given lock.
 * @param lock Lock to initialize
 * @return Whether the operation is a success
**/
bool v_lock_init(struct v_lock_t* lock);

/** Clean up the given lock.
 * @param lock Lock to clean up
**/
void v_lock_cleanup(struct v_lock_t* lock);

/** Wait and acquire the given lock.
 * @param lock Lock to acquire
 * @return Whether the operation is a success
**/
bool v_lock_acquire(struct v_lock_t* lock);

/** Release the given lock.
 * @param lock Lock to release
**/
void v_lock_release(struct v_lock_t* lock);

/** Wait until woken up by a signal on the given lock.
 *  The lock is released until lock_wait completes at which point it is acquired
 *  again. Exclusive lock access is enforced.
 * @param lock Lock to release (until woken up) and wait on.
**/
void v_lock_wait(struct v_lock_t* lock);

/** Wake up all threads waiting on the given lock.
 * @param lock Lock on which other threads are waiting.
**/
void v_lock_wake_up(struct v_lock_t* lock);
