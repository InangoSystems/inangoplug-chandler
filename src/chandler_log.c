/*
################################################################################
#
#  Copyright 2019-2021 Inango Systems Ltd.
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
#define _GNU_SOURCE  /* => _POSIX_C_SOURCE >= 199309L */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* File output support */
#include <linux/limits.h>
#include <stdint.h>

#include "chandler_log.h"


#define LOG_MESSAGE_BUFFER_SIZE   (CHANDLER_LOG_MESSAGE_SIZE)

#define LOG_CONF_DEFAULTS  {                    \
        .file_name         = "",                \
        .log_to_console    = 1,                 \
        .log_to_file       = 0,                 \
        .file_size_limit   = MAX_LOG_FILE_SIZE, \
        .rotate_file_count = 1                  \
    }


typedef struct logger_t
{
    chandler_log_conf_t  conf;
    size_t          file_name_len;
    FILE           *file;
} logger_t;


static long       g_log_level  = CHANDLER_LOG_LEVEL_ERR_ID;

static FILE      *g_log_stream = NULL;

static logger_t   g_logger     = {
    .conf           = LOG_CONF_DEFAULTS,
    .file_name_len = 0,
    .file           = NULL
};


static FILE * get_log_stream(void)
{
    return g_log_stream? g_log_stream: stdout;
}
/*--------------------------------------------------------------------------*/
/* file output support */
/*--------------------------------------------------------------------------*/
static void log_file_cleanup(void)
{
    uint32_t i;
    char     name[MAX_LOG_FILE_PATH_SIZE + LOG_ROTATION_SUFFIX_LENGTH];

    if(!g_logger.conf.rotate_file_count || g_logger.conf.rotate_file_count >= MAX_LOG_ROTATE_FILE_COUNT)
        return;

    for(i = g_logger.conf.rotate_file_count + 1; i <= MAX_LOG_ROTATE_FILE_COUNT; ++i)
    {
        if (sprintf(name, "%s.%u", g_logger.conf.file_name, i) > 0)
        {
            name[sizeof(name) - 1] = '\0';
            unlink(name);
        }
    }
}
/*--------------------------------------------------------------------------*/
static int log_file_open(void)
{
    if (!g_logger.conf.log_to_file)
        return 0;

    if (NULL != g_logger.file)
        return 0;

    g_logger.file = fopen(g_logger.conf.file_name, "a");
    if (NULL == g_logger.file) {
        fprintf(stderr, "Failed to open file \"%s\" for appending: %d (%s)\n", g_logger.conf.file_name, errno, strerror(errno));
        return 1;
    }

    setvbuf(g_logger.file, NULL, _IOLBF, BUFSIZ);  /* _IOLBF - line buffering */
    return 0;
}
/*--------------------------------------------------------------------------*/
static void log_file_close(void)
{
    if (NULL == g_logger.file) {
        return;
    }

    fflush(g_logger.file);
    fclose(g_logger.file);
    g_logger.file = NULL;
}
/*--------------------------------------------------------------------------*/
static void log_file_rotate_if_needed(size_t added_size)
{
    static const char digits[] = "0123456789";

    uint32_t i;
    uint32_t j;
    uint32_t suffixPos = g_logger.file_name_len;
    char     name[2][MAX_LOG_FILE_PATH_SIZE + LOG_ROTATION_SUFFIX_LENGTH];
    long     filePos;

    if (NULL == g_logger.file) {
        return;
    }

    filePos = ftell(g_logger.file);
    if (-1 == filePos)
    {
        fprintf(stderr, "Failed to get log file position: %d (%s)", errno, strerror(errno));
        return;
    }

    if (!g_logger.conf.file_size_limit || filePos + (long)added_size <= g_logger.conf.file_size_limit)
        return;

    log_file_close();

    for (j = 0; j < 2; ++j)
    {
        strcpy(name[j], g_logger.conf.file_name);
        name[j][suffixPos] = '.';
        name[j][suffixPos + LOG_ROTATION_SUFFIX_LENGTH] = '\0';
    }

    ++suffixPos;
    name[0][suffixPos] = digits[g_logger.conf.rotate_file_count];

    for(j = 1, i = g_logger.conf.rotate_file_count - 1; i > 0; j = 1 - j, --i)
    {
        name[j][suffixPos] = digits[i];
        rename(name[j], name[1 - j]);
    }

    name[j][suffixPos - 1] = '\0';
    rename(name[j], name[1 - j]);

    log_file_open();
}
/*--------------------------------------------------------------------------*/
static void log_file_write(const char * message, size_t size)
{
    if (NULL == g_logger.file)
        log_file_open();

    if (NULL != g_logger.file)
    {
        log_file_rotate_if_needed(size + 1);
        fprintf(g_logger.file, "%s\n", message);
    }
}
/*--------------------------------------------------------------------------*/
/* Log writer entry point */
/*--------------------------------------------------------------------------*/
static void log_write(const char * message, size_t size)
{
    if (g_logger.conf.log_to_console)
        fprintf(get_log_stream(), "%s\n", message);

    if (g_logger.conf.log_to_file)
        log_file_write(message, size);
}
/*--------------------------------------------------------------------------*/
/* Interface functions */
/*--------------------------------------------------------------------------*/
void chandler_log_set_level(long level)
{
    g_log_level = level;
}
/*--------------------------------------------------------------------------*/
int  chandler_log_is_visible_level(long level)
{
    return level <= g_log_level;
}
/*--------------------------------------------------------------------------*/
void chandler_get_time(long * sec, int * msec)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *sec  = ts.tv_sec;
    *msec = (int)(ts.tv_nsec / 1000000);
}
/*--------------------------------------------------------------------------*/
void chandler_log(const char * format, ...)
{
    char     buffer[LOG_MESSAGE_BUFFER_SIZE];
    va_list  v_args;
    int      size;

    va_start(v_args, format);
    size = vsnprintf(buffer, CHANDLER_LOG_MESSAGE_SIZE, format, v_args);
    va_end(v_args);

    size = (size >= CHANDLER_LOG_MESSAGE_SIZE)? CHANDLER_LOG_MESSAGE_SIZE - 1: size;
    buffer[size] = '\0';

    if (size > 0) {
        log_write(buffer, size);
    }
}
/*--------------------------------------------------------------------------*/
int chandler_log_init(void)
{
    if (!g_logger.conf.log_to_file) {
        g_logger.file_name_len = 0;
    }
    else {
        g_logger.file_name_len = strlen(g_logger.conf.file_name);

        log_file_cleanup();
        if (log_file_open()) {
            fprintf(stderr, "Failed to open log file: %d (%s)\n", errno, strerror(errno));
            return 0;
        }
    }

    return 1;
}
/*--------------------------------------------------------------------------*/
void chandler_log_done(void)
{
    if (g_logger.conf.log_to_file) {
        log_file_close();
    }
}
/*--------------------------------------------------------------------------*/
chandler_log_conf_t * chandler_log_conf(void)
{
    return &g_logger.conf;
}
