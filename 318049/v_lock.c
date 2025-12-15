#include "v_lock.h"

void v_lock_init(v_lock_t *lock) {
    atomic_init(&lock->version_clock, 0);
    atomic_init(&lock->owner, invalid_tx);  
}

void v_lock_cleanup(v_lock_t * unused(lock)) { return; }

bool v_lock_acquire(v_lock_t *lock, tx_t locker) {
    int old = atomic_load(&lock->version_clock);
    
    // Check if lock is free
    if (old & 0x1) return false;

    if (atomic_compare_exchange_strong(&lock->version_clock, &old, old | 0x1)) {
        atomic_store(&lock->owner, locker);
        return true;
    }
    return false;
}

void v_lock_release(v_lock_t *lock) {
    atomic_fetch_and(&lock->version_clock, ~1); // clears the lock bit
}

void v_lock_release_and_update(v_lock_t* lock, int val) {
    atomic_store(&lock->version_clock, val << 1);
}

int v_lock_version(v_lock_t *lock) {
    int version = atomic_load(&lock->version_clock);
    // locked, return -1 (ERROR)
    if (version & 0x1) return LOCKED;
    // unlocked, return version
    return version >> 1;
}

tx_t v_lock_owner(v_lock_t *lock) {
    return atomic_load(&lock->owner);
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
    return atomic_fetch_add(global_clock, 1)+1;
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
    return pthread_mutex_unlock(lock);
}
