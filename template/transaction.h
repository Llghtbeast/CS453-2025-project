#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "tm.h"

struct transaction {
    bool is_ro;
    version_clock_t r_version_clock;
    version_clock_t w_version_clock;
    struct set_t *r_set;
    struct map_t *w_set;
};

