#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdatomic.h>
#include <stdint.h>

#include "macros.h"

typedef atomic_int version_clock_t; // The type of the version clock

// map.h
#define INITIAL_CAPACITY 4
#define GROW_FACTOR 2
#define MAX_LOAD_FACTOR 0.75

// shared.h
#define VLOCK_NUM 4096
#define INITIAL_TO_FREE_CAPACITY 16
#define SEGMENT_FREE_BATCH_SIZE 128
#define SEGMENT_FREE_BATCH_CUM_SIZE 1048576     // 1MB

// txn.h
#define ABORT false
#define SUCCESS true
#define INVALID (-1)

// v_lock.h
#define LOCKED (-1)

// ============== helper methods ============== 
static inline size_t set_hash(void const *key, size_t capacity) {
    uintptr_t k = (uintptr_t)key;
    // Simple multiplicative hash
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k % capacity;
}

static inline uintptr_t get_memory_lock_index(void const *addr) {
    // 0x9E3779B97F4A7C15 is the Golden Ratio constant for 64-bit
    return set_hash(addr, VLOCK_NUM);
}

static inline void set_bit(uint64_t bit_field[], size_t bit) {
    size_t bit_index = bit >> 6;            // division by 64
    size_t bit_offset = bit & 0x3F;         // modulo 64

    bit_field[bit_index] |= (1ULL << bit_offset);
}

static inline bool get_bit(uint64_t bit_field[], size_t bit) {
    size_t bit_index = bit >> 6;            // division by 64
    size_t bit_offset = bit & 0x3F;         // modulo 64

    return bit_field[bit_index] & (1ULL << bit_offset);
}

// ============== Debug ==============
// Debug prints
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m" 

// Severity
#define LOG_LEVEL_RELEASE 0
#define LOG_LEVEL_TEST    1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_NOTE    3
#define LOG_LEVEL_LOG     4
#define LOG_LEVEL_DEBUG   5

#define LOG_LEVEL LOG_LEVEL_RELEASE

static inline void debug_vprint(
    int severity,
    FILE *stream, 
    const char *msg_color, 
    const char *header,
    const char *file,
    int line,
    const char *str, 
    ...) 
{
    if (unlikely(severity <= LOG_LEVEL)) { 
        va_list args;
        va_start(args, str);    
        fprintf(stream, "%s%s %s:%d: ", msg_color, header, file, line);
        vfprintf(stream, str, args);
        fprintf(stream, "%s", COLOR_RESET);
        fflush(stream);
        va_end(args);
    }
}

#define LOG_TEST(fmt, ...) debug_vprint(LOG_LEVEL_TEST, stdout, COLOR_WHITE, "[TEST] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARNING(fmt, ...) debug_vprint(LOG_LEVEL_WARNING, stdout, COLOR_RED, "[WARNING] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_NOTE(fmt, ...) debug_vprint(LOG_LEVEL_NOTE, stdout, COLOR_YELLOW, "[NOTE] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_LOG(fmt, ...) debug_vprint(LOG_LEVEL_LOG, stdout, COLOR_WHITE, "[LOG] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) debug_vprint(LOG_LEVEL_DEBUG, stdout, COLOR_BLUE, "[DEBUG] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)