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
#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <unistd.h>

#include "chandler_log.h"
#include "chandler_system.h"


int timer_create_repeated(long interval_msec)
{
    int               flags = 0;
    int               fd;
    struct itimerspec tsp;

    fd = timerfd_create(CLOCK_MONOTONIC, flags);
    if (-1 == fd)
        return -1;

    tsp.it_interval.tv_sec  = interval_msec / 1000;
    tsp.it_interval.tv_nsec = (interval_msec % 1000) * 1000000;
    tsp.it_value.tv_sec     = tsp.it_interval.tv_sec;
    tsp.it_value.tv_nsec    = tsp.it_interval.tv_nsec;

    timerfd_settime(fd, flags, &tsp, NULL);
    return fd;
}
//----------------------------------------------------------------------------
int timer_destroy(int fd)
{
    return close(fd);
}
//----------------------------------------------------------------------------
static pid_t read_pid_from_open_file(FILE * file, const char * pid_file)
{
    int   error;
    char  line[128];
    char *end;

    if (!fgets(line, sizeof(line), file)) {
        if (ferror(file)) {
            error = errno;
            LOG_ERROR("failed to read from file \"%s\": %s", pid_file, strerror(error));
        }
        else {
            error = ESRCH;
            LOG_ERROR("failed to read from file \"%s\": unexpected end of file", pid_file);
        }
        return -error;
    }

    pid_t pid = strtoul(line, &end, 10);
    if (*end != '\0' && *end != '\n' && *end != ' ') {
        LOG_ERROR("failed to decode pid from string value \"%s\"", line);
        return -ESRCH;
    }

    return pid;
}

pid_t read_pid_from_file(const char * pid_file)
{
    FILE  *file;
    pid_t  pid;

    file = fopen(pid_file, "r");
    if (!file) {
        LOG_ERROR("failed to open file \"%s\": %d (%s)", pid_file, errno, strerror(errno));
        return -errno;
    }

    pid = read_pid_from_open_file(file, pid_file);
    fclose(file);

    return pid;
}

pid_t find_process(const char * name)
{
    DIR           *dir;
    struct dirent *ent;
    char          *end_ptr;
    char           buffer[512];

    dir = opendir("/proc");
    if (!dir) {
        LOG_ERROR("failed to open /proc directory: %d", errno);
        return -errno;
    }

    for (ent = readdir(dir); ent != NULL; ent = readdir(dir)) {
        long pid = strtol(ent->d_name, &end_ptr, 10);
        if (*end_ptr != '\0')
            continue;

        snprintf(buffer, sizeof(buffer), "/proc/%ld/cmdline", pid);
        FILE *fp = fopen(buffer, "r");
        if (!fp) {
            LOG_ERROR("failed to open file \"%s\": %d", buffer, errno);
        }
        else {
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                char *first = strtok(buffer, " ");
                if (0 == strcmp(first, name)) {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t)pid;
                }
            }
            fclose(fp);
        }
    }

    closedir(dir);
    return 0;
}

static int make_sockaddr_un(const char * name, struct sockaddr_un * un, socklen_t * un_len)
{
    if (strlen(name) >= sizeof(un->sun_path))
        return E2BIG;

    un->sun_family = AF_UNIX;
    strncpy(un->sun_path, name, sizeof(un->sun_path));
    *un_len = (offsetof(struct sockaddr_un, sun_path) + strlen(un->sun_path) + 1);
    return 0;
}

int connect_unix_socket(int style, const char * path, int * fd)
{
    struct sockaddr_un un;
    socklen_t          un_len;
    int                error;

    if (path == NULL) {
        return EADDRNOTAVAIL;
    }

    *fd = socket(PF_UNIX, style, 0);
    if (*fd < 0) {
        LOG_ERROR("failed to create unix socket: %d (%s)", errno, strerror(errno));
        return errno;
    }

    error = make_sockaddr_un(path, &un, &un_len);
    if (error) {
        LOG_ERROR("failed to initialize sockaddr_un for path \"%s\": %d (%s)", path, error, strerror(error));
        close(*fd);
        return error;
    }

    if (0 != connect(*fd, (struct sockaddr *)&un, un_len))
    {
        LOG_ERROR("failed to connect to \"%s\": %d (%s)", path, errno, strerror(errno));
        close(*fd);
        return errno;
    }

    return 0;
}

int spawn_process(const char * path, char * const * args)
{
    pid_t fork_pid = fork();
    int   rc;

    if (fork_pid == 0) {
        /* a child process */
        //signal(SIGCHLD, SIG_IGN);
        //signal(SIGHUP,  SIG_IGN);

        /* Close all open file descriptors */
        for (int x = (int)sysconf(_SC_OPEN_MAX); x >= 0; --x) {
            close(x);
        }

        rc = execv(path, args);
        if (rc == -1) {
            fprintf(stderr, "forked child failed to exec: errno = %d", errno);
            _exit(EXIT_FAILURE);
        }

        _exit(EXIT_SUCCESS);
    }

    if (fork_pid == -1) {
        LOG_ERROR("failed to fork: errno = %d", errno);
        return -1;
    }

    LOG_DBG("forked a child process with pid = %d", fork_pid);
    return 0;
}

int spawn_process_from_command(const char * command_line) {
    char  cmd[MAX_COMMAND_SIZE];
    char *args[MAX_COMMAND_ARGS + 1];
    char *save_ptr = NULL;
    char *arg;
    int   count = 0;

    strncpy(cmd, command_line, sizeof(cmd));
    arg = strtok_r(cmd, " ", &save_ptr);
    for (; arg != NULL && count < MAX_COMMAND_ARGS; arg = strtok_r(NULL, " ", &save_ptr)) {
        LOG_DBG("-- arg[%d] = %s", count, arg);
        args[count] = arg;
        ++count;
    }

    if (arg && count == MAX_COMMAND_ARGS) {
        LOG_ERROR("too many arguments in command (> %d): %s", MAX_COMMAND_ARGS, command_line);
        return -1;
    }

    args[count] = NULL;

    return spawn_process(args[0], args);
}

int system_reboot(void)
{
    sync();

    if(setuid(0) != 0)
    {
        LOG_ERROR("failed to setuid: errno = %d", errno);
    }

    return reboot(RB_AUTOBOOT);
}
