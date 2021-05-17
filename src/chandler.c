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
#define _DEFAULT_SOURCE

#include "chandler_conf.h"
#include "chandler_log.h"
#include "chandler_ovs.h"
#include "chandler_ovs_db.h"
#include "chandler_stat.h"
#include "chandler_system.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

#define OUTPUT_BUFFER_SIZE  4096

static volatile int is_interrupted = 0;

static void sig_int_handler(int value)
{
    (void)value;
    fprintf(stderr, "\n-- received SIGINT\n");
    is_interrupted = 1;
}

void print_usage(void)
{
    printf("Usage:\n");
    printf("    chandler -h\n");
    printf("    chandler [-c FILE] [-l LEVEL] [-f NAME [-r COUNT] [-m SIZE]] [-s] \n");
    printf("Where:\n");
    printf("    -c FILE - load configuration from FILE (FILE can contain a full path, max length is %d)\n", MAX_PATH_SIZE - 1);
    printf("    -h - print this page\n");
    printf("    -l LEVEL - set log level:\n");
    printf("        1 - error (default)\n");
    printf("        2 - warning\n");
    printf("        3 - informational\n");
    printf("        4 - debug\n");
    printf("    -f NAME - log file name (may be including full path, max length is %d)\n", MAX_LOG_FILE_PATH_SIZE - 1);
    printf("    -s - silent mode - no console output\n");
    printf("    -r COUNT - rotation file count (1 <= count <= 9, default is 1)\n");
    printf("    -m SIZE - log file size limit in bytes (max is %d (used by default), min is %d)\n", MAX_LOG_FILE_SIZE, MIN_LOG_FILE_SIZE);
}

int configure(int argc, char * argv[])
{
    int             rc;
    int             opt;
    char            conf_path[MAX_PATH_SIZE] = "";
    long            log_level;
    chandler_log_conf_t *log_conf = chandler_log_conf();
    char           *end;

    do
    {
        opt = getopt(argc, argv, "hc:l:sf:r:m:");
        switch (opt)
        {
        case -1:
            break;
        case 'h':
            print_usage();
            return 0;
        case 'c':
            if (strlen(optarg) >= sizeof(conf_path))
            {
                fprintf(stderr, "configuration file path is too long: \"%s\"\n", optarg);
                print_usage();
                return 2;
            }
            strcpy(conf_path, optarg);
            break;
        case 'l':
            log_level = strtol(optarg, &end, 0);
            if (*end != '\0' || log_level > UINT16_MAX)
            {
                fprintf(stderr, "invalid log level: %s\n", optarg);
                print_usage();
                return 2;
            }
            chandler_log_set_level(log_level);
            break;
        case 's':
            log_conf->log_to_console = 0;
            break;
        case 'f':
            if (strlen(optarg) >= sizeof(log_conf->file_name))
            {
                fprintf(stderr, "log file path is too long: \"%s\"\n", optarg);
                print_usage();
                return 2;
            }
            strcpy(log_conf->file_name, optarg);
            log_conf->log_to_file = 1;
            break;
        case 'r':
            log_conf->rotate_file_count = strtol(optarg, &end, 0);
            if (*end != '\0' || log_conf->rotate_file_count < 1 || log_conf->rotate_file_count > MAX_LOG_ROTATE_FILE_COUNT)
            {
                fprintf(stderr, "invalid rotate file count value: %s\n", optarg);
                print_usage();
                return 2;
            }
            break;
        case 'm':
            log_conf->file_size_limit = strtol(optarg, &end, 0);
            if (*end != '\0' || log_conf->file_size_limit > MAX_LOG_FILE_SIZE || log_conf->file_size_limit < MIN_LOG_FILE_SIZE)
            {
                fprintf(stderr, "log file size limit is invalid: %s\n", optarg);
                print_usage();
                return 2;
            }
            break;
        default:
            print_usage();
            return 2;
        }
    } while (opt != -1);

    if (conf_path[0] != '\0')
    {
        rc = load_conf_file(conf_path);
        if (rc != 0)
        {
            LOG_ERROR("failed to load configuration from file \"%s\"", conf_path);
            return 1;
        }
    }

    /* Overriding configuration from environment if any */
    load_conf_env();
    return 0;
}

static int on_disconnect(void)
{
    FILE   *output;
    char    output_buffer[OUTPUT_BUFFER_SIZE] = "";
    size_t  count = 0;

    LOG_WARN("received disconnect notification");

    if (get_conf()->ovs_cmd_disconnect[0] == '\0') {
        return 0;
    }

    output = popen(get_conf()->ovs_cmd_disconnect, "r");
    if (output == NULL) {
        LOG_ERROR("failed to invoke disconnect command \"%s\": %d (%s)", get_conf()->ovs_cmd_disconnect, errno, strerror(errno));
        return -1;
    }

    LOG_WARN("invoked disconnect command \"%s\"", get_conf()->ovs_cmd_disconnect);

    do {
        count = fread(output_buffer, 1, sizeof(output_buffer) - 1, output);
        output_buffer[count] = '\0';
        LOG_DBG("-- %s", output_buffer);
    } while (count == sizeof(output_buffer) - 1);

    return pclose(output);
}

static int reboot(void)
{
    FILE   *output;
    char    output_buffer[OUTPUT_BUFFER_SIZE] = "";
    size_t  count = 0;

    if (get_conf()->ovs_cmd_reboot[0] == '\0') {
        return system_reboot();
    }

    LOG_WARN("invoking reboot command \"%s\"", get_conf()->ovs_cmd_reboot);

    output = popen(get_conf()->ovs_cmd_reboot, "r");
    if (output == NULL) {
        LOG_ERROR("failed to invoke reboot command \"%s\": %d (%s)", get_conf()->ovs_cmd_reboot, errno, strerror(errno));
        return -1;
    }

    do {
        count = fread(output_buffer, 1, sizeof(output_buffer) - 1, output);
        output_buffer[count] = '\0';
        LOG_DBG("-- %s", output_buffer);
    } while (count == sizeof(output_buffer) - 1);

    return pclose(output);
}

int main(int argc, char * argv[])
{
    int             rc;
    struct pollfd   fds[2] = {{.fd = -1, .events = POLLIN}, {.fd = -1, .events = POLLIN}};
    int             fd_count = 2;
    ovsdb_monitor_t db_monitor;
    uint64_t        exp;

    setlinebuf(stdout);

    signal(SIGINT,  sig_int_handler);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP,  SIG_IGN);

    rc = configure(argc, argv);
    if (rc) {
        return rc;
    }

    if (!chandler_log_init()) {
        fprintf(stderr, "failed to initialize logger - aborting\n");
        return 1;
    }

    LOG_DBG("started");

    fds[0].fd = timer_create_repeated(get_conf()->check_interval);
    if (fds[0].fd == -1) {
        LOG_ERROR("failed to create timer: %d (%s)", errno, strerror(errno));
        return 1;
    }

    LOG_INFO("created timer with %ld msec interval", get_conf()->check_interval);

    while (!is_interrupted) {
        if (fds[1].fd == -1) {
            switch (monitor_create(get_conf()->ovs_unixsock_db, &db_monitor, on_disconnect))
            {
            case QS_SUCCESS:
                LOG_INFO("created ovsdb monitor");
                fds[1].fd = db_monitor.fd;
                fd_count = 2;
                break;
            default:
                LOG_ERROR("failed to create ovsdb monitor");
                fd_count = 1;
            }
        }

        rc = poll(fds, fd_count, -1);
        if (rc == -1)
        {
            LOG_ERROR("poll failed: %d", rc);
            continue;
        }

        if (rc == 0)
        {
            LOG_ERROR("poll timeout");
            continue;
        }

        if (((unsigned short)fds[0].revents & (unsigned short)POLLIN))
        {
            LOG_DBG("-- timer");
            fds[0].revents = 0;
            if (sizeof(exp) != read(fds[0].fd, &exp, sizeof(exp))) {
                LOG_ERROR("failed to reset timer descriptor");
            }
            check_ovs();
        }

        if (((unsigned short)fds[1].revents & (unsigned short)POLLIN))
        {
            LOG_DBG("-- ovsdb monitor event");
            fds[1].revents = 0;
            if (db_monitor.on_read != NULL) {
                if (QS_SUCCESS != db_monitor.on_read(&db_monitor)) {
                    sleep(1);
                    LOG_WARN("destroying ovsdb monitor");
                    monitor_destroy(&db_monitor);
                    fds[1].fd = -1;
                    fd_count = 1;
                }
            }
        }

        if (   (get_conf()->restarts_before_reboot && (chandler_stat()->restarts_count > get_conf()->restarts_before_reboot))
            || (get_conf()->failures_before_reboot && (chandler_stat()->failures_count > get_conf()->failures_before_reboot))
        )
        {
            LOG_INFO("restarts count: %ld (max: %ld)", chandler_stat()->restarts_count, get_conf()->restarts_before_reboot);
            LOG_INFO("failures count: %ld (max: %ld)", chandler_stat()->failures_count, get_conf()->failures_before_reboot);

            LOG_WARN("rebooting the system...");
            if (reboot()) {
                LOG_ERROR("failed to reboot the system: %d (%s)", errno, strerror(errno));
            }
        }
    }

    monitor_destroy(&db_monitor);
    timer_destroy(fds[0].fd);

    chandler_log_done();

    return 0;
}
