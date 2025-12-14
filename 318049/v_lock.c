#include "v_lock.h"

void v_lock_init(v_lock_t *lock) {
    atomic_init(lock, 0);
}

void v_lock_cleanup(v_lock_t * unused(lock)) { return; }

bool v_lock_acquire(v_lock_t *lock) {
    int old = atomic_load(lock);

    // Check if lock is free
    if (old & 0x1) return false;

    return atomic_compare_exchange_strong(lock, &old, old | 0x1);
}

void v_lock_release(v_lock_t *lock) {
    atomic_fetch_sub(lock, 1);
}

void v_lock_update(v_lock_t *lock, int new_val) {
    atomic_store(lock, new_val << 1);
}

int v_lock_version(v_lock_t *lock) {
    int version = atomic_load(lock);
    // locked, return -1 (ERROR)
    if (version & 0x1) return LOCKED;
    // unlocked, return version
    return version >> 1;
}

// =========== Global clock functions =========== 
void global_clock_init(global_clock_t *global_clock) {
    atomic_init(global_clock, 0);
}

void global_clock_cleanup(global_clock_t * unused(global_clock)) { return; }

int global_clock_load(global_clock_t *global_clock) {
    return atomic_load(global_clock);
}

int global_clock_increment_and_fetch(global_clock_t *global_clock) {
    return atomic_fetch_add(global_clock, 1);
}

// =========== Segment allocation lock functions =========== 
int alloc_mutex_init(alloc_mutex_t *lock) {
    return pthread_mutex_init(lock, NULL);
}

int alloc_mutex_cleanup(alloc_mutex_t *lock) {
    return pthread_mutex_destroy(lock);
}

int alloc_mutex_acquire(alloc_mutex_t *lock) {
    return pthread_mutex_lock(lock);
}

int alloc_mutex_release(alloc_mutex_t *lock) {
    return pthread_mutex_lock(lock);
}
