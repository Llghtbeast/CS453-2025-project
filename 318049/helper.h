#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdatomic.h>

#include "macros.h"

typedef atomic_int version_clock_t; // The type of the version clock

// map.h
#define INITIAL_CAPACITY 8
#define GROW_FACTOR 2

// shared.h
#define VLOCK_NUM 65536

// txn.h
#define ABORT false
#define SUCCESS true
#define INVALID (-1)

// v_lock.h
#define LOCKED (-1)

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
        va_end(args);
    }
}

#define LOG_TEST(fmt, ...) debug_vprint(LOG_LEVEL_TEST, stdout, COLOR_WHITE, "[TEST] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARNING(fmt, ...) debug_vprint(LOG_LEVEL_WARNING, stdout, COLOR_RED, "[WARNING] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_NOTE(fmt, ...) debug_vprint(LOG_LEVEL_NOTE, stdout, COLOR_YELLOW, "[NOTE] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_LOG(fmt, ...) debug_vprint(LOG_LEVEL_LOG, stdout, COLOR_WHITE, "[LOG] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) debug_vprint(LOG_LEVEL_DEBUG, stdout, COLOR_BLUE, "[DEBUG] ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)