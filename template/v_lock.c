#include "v_lock.h"

bool v_lock_init(struct v_lock_t* lock){
    return false;
}

void v_lock_cleanup(struct v_lock_t *lock)
{
}

bool v_lock_acquire(struct v_lock_t *lock)
{
    return false;
}

void v_lock_release(struct v_lock_t *lock)
{
}

void v_lock_wait(struct v_lock_t *lock)
{
}

void v_lock_wake_up(struct v_lock_t *lock)
{
}
