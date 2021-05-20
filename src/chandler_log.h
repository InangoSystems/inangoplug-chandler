/*
################################################################################
#
#  Copyright 2020-2021 Inango Systems Ltd.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
################################################################################
*/
#ifndef CHANDLER_LOG_H
#define CHANDLER_LOG_H

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <linux/limits.h>
#include <unistd.h>


#define CHANDLER_LOG_MESSAGE_SIZE   65536

/* Logger level identifiers */
#define CHANDLER_LOG_LEVEL_NIL_ID   0
#define CHANDLER_LOG_LEVEL_ERR_ID   1
#define CHANDLER_LOG_LEVEL_WRN_ID   2
#define CHANDLER_LOG_LEVEL_INF_ID   3
#define CHANDLER_LOG_LEVEL_DBG_ID   4


/* Logger level string tags */
#define CHANDLER_LOG_LEVEL_ERR_STR  "ERR"
#define CHANDLER_LOG_LEVEL_WRN_STR  "WRN"
#define CHANDLER_LOG_LEVEL_INF_STR  "INF"
#define CHANDLER_LOG_LEVEL_DBG_STR  "DBG"


#ifdef CHANDLER_LOG_FILE_LINES
    #define FILE_LINE_FORMAT  " @%s:%d"
    #define FILE_LINE_ARGS    , (strrchr("/" __FILE__, '/') + 1), __LINE__
#else
    #define FILE_LINE_FORMAT
    #define FILE_LINE_ARGS
#endif


#define CHANDLER_LOG_FORMAT(LEVEL__) \
    "%8ld.%03d|" LEVEL__##_STR "|%s" FILE_LINE_FORMAT

#define CHANDLER_LOG_(LEVEL__, ...)                           \
    do {                                                 \
        if (chandler_log_is_visible_level(LEVEL__##_ID)) {    \
            long sec__;                                  \
            int  msec__;                                 \
            char str__[CHANDLER_LOG_MESSAGE_SIZE] = {0};      \
            chandler_get_time(&sec__, &msec__);               \
            snprintf(str__, sizeof(str__), __VA_ARGS__); \
            chandler_log(CHANDLER_LOG_FORMAT(LEVEL__),             \
                sec__,                                   \
                msec__,                                  \
                str__                                    \
                FILE_LINE_ARGS                           \
            );                                           \
        }                                                \
    } while (0)


#define LOG_ERROR(...)  CHANDLER_LOG_(CHANDLER_LOG_LEVEL_ERR, __VA_ARGS__)
#define LOG_WARN(...)   CHANDLER_LOG_(CHANDLER_LOG_LEVEL_WRN, __VA_ARGS__)
#define LOG_INFO(...)   CHANDLER_LOG_(CHANDLER_LOG_LEVEL_INF, __VA_ARGS__)
#define LOG_DBG(...)    CHANDLER_LOG_(CHANDLER_LOG_LEVEL_DBG, __VA_ARGS__)

/*
 * Definitions for file output support
 */
#define LOG_ROTATION_SUFFIX_LENGTH  2
#define MAX_LOG_ROTATE_FILE_COUNT   9
#define MAX_LOG_FILE_PATH_SIZE      (PATH_MAX - LOG_ROTATION_SUFFIX_LENGTH)
#define MAX_LOG_FILE_SIZE           INT32_MAX
#define MIN_LOG_FILE_SIZE           4096

typedef struct chandler_log_conf_t
{
    char file_name[MAX_LOG_FILE_PATH_SIZE];
    int  log_to_console;
    int  log_to_file;
    long file_size_limit;
    long rotate_file_count;
} chandler_log_conf_t;

/**
 * Sets the global logging level.
 *
 * \param level
 */
void chandler_log_set_level(long level);

/**
 * Checks that level is allowed for printing
 *
 * \param level  The level to be checked
 *
 * \return       1 if the level is visible, 0 - otherwise
 */
int  chandler_log_is_visible_level(long level);

/**
 * Returns current time in form of seconds plus milliseconds since some
 * starting point.
 *
 * \param[out] sec   Seconds value
 * \param[out] msec  Milliseconds value
 */
void chandler_get_time(long * sec, int * msec);

/**
 * Writes formatted message to log.
 *
 * \param format  Message printf-like format.
 * \param ...     Optional format arguments.
 */
void chandler_log(const char * format, ...);

/**
 * Initializes a logger.
 *
 * Function should be called once in the main process before any usage of
 * chandler_log() function or CHANDLER_LOG_X() macros.
 *
 * \return          1 if logger has been successfully initialized, 0 - otherwise
 */
int  chandler_log_init(void);

/**
 * Finalizes the logger.
 */
void chandler_log_done(void);

/**
 * Returns pointer to log configuration
 *
 * \return
 */
chandler_log_conf_t * chandler_log_conf(void);

#endif  /* CHANDLER_LOG_H */
