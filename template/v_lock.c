#include "v_lock.h"

bool v_lock_init(struct v_lock_t* lock) {
    return pthread_mutex_init(&(lock->mutex), NULL) == 0
        && pthread_cond_init(&(lock->cv), NULL) == 0;
}

void v_lock_cleanup(struct v_lock_t *lock) {
    pthread_mutex_destroy(&(lock->mutex));
    pthread_cond_destroy(&(lock->cv));
}

bool v_lock_acquire(struct v_lock_t *lock) {
    lock->lock_bit = true;
    return pthread_mutex_lock(&(lock->mutex)) == 0;
}

void v_lock_release(struct v_lock_t *lock, version_clock_t wv) {
    lock->lock_bit = false;
    lock->version_clock = wv;
    pthread_mutex_unlock(&(lock->mutex));
}

void v_lock_wait(struct v_lock_t *lock) {
    pthread_cond_wait(&(lock->cv), &(lock->mutex));
}

void v_lock_wake_up(struct v_lock_t *lock) {
    pthread_cond_broadcast(&(lock->cv));
}