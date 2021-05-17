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
#define _POSIX_SOURCE

#include "chandler_conf.h"
#include "chandler_jrpc.h"
#include "chandler_log.h"
#include "chandler_ovs.h"
#include "chandler_stat.h"
#include "chandler_system.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>


typedef enum daemon_status_t {
    DS_ALIVE,
    DS_NO_RESPONSE,
    DS_NOT_ALIVE,
    DS_NO_PROCESS,
    DS_SYSTEM_ERROR
} daemon_status_t;


static const char * ovs_rundir(void)
{
    return get_conf()->ovs_run_dir;
}

static pid_t ovs_get_pid(const char * target, const char * pidfile)
{
    char  pid_file_name[MAX_PATH_SIZE];
    int   count;

    if (pidfile && pidfile[0] != '\0') {
        if (pidfile[0] == '/') {
            return read_pid_from_file(pidfile);
        }

        count = snprintf(pid_file_name, sizeof(pid_file_name), "%s/%s", ovs_rundir(), pidfile);
        if (count < 0 || (size_t)count >= sizeof(pid_file_name)) {
            return -1;
        }

        return read_pid_from_file(pid_file_name);
    }

    count = snprintf(pid_file_name, sizeof(pid_file_name), "%s/%s.pid", ovs_rundir(), target);
    if (count < 0 || (size_t)count >= sizeof(pid_file_name)) {
        return -1;
    }

    return read_pid_from_file(pid_file_name);
}

static char * ovs_make_unix_socket_name(char * buffer, size_t buffer_size, const char * target, pid_t pid)
{
    int count;

    if (target[0] != '/')
    {
        if (pid <= 0) {
            return NULL;
        }

        count = snprintf(buffer, buffer_size, "%s/%s.%d.ctl", ovs_rundir(), target, (int)pid);
        if (count < 0 || (size_t)count >= buffer_size) {
            return NULL;
        }
    }
    else
    {
        count = snprintf(buffer, buffer_size, "%s", target);
        if (count < 0 || (size_t)count >= buffer_size) {
            return NULL;
        }
    }

    return buffer;
}

query_status_t ovs_query_daemon(const char * target, pid_t pid)
{
    static char rpc_request[] = "{\"id\":0,\"method\":\"list-commands\",\"params\":[]}";
    static char rpc_response[MAX_RESPONSE_SIZE];

    struct timeval          tv = {.tv_sec = get_conf()->receive_timeout / 1000, .tv_usec = 1000*(get_conf()->receive_timeout % 1000)};
    char                    socket_name[MAX_PATH_SIZE];
    int                     fd;
    ssize_t                 count;
    ssize_t                 total = 0;
    int                     error;
    query_status_t          qstatus = QS_SUCCESS;
    ovsdb_message_parser_t  parser;

    if (NULL == ovs_make_unix_socket_name(socket_name, sizeof(socket_name), target, pid))
    {
        LOG_ERROR("failed to get unix socket name for \"%s\"", target);
        return QS_UNIX_SOCKET_NAME_ERROR;
    }

    LOG_DBG("got unix socket name %s for \"%s\"", socket_name, target);

    error = connect_unix_socket(SOCK_STREAM, socket_name, &fd);
    if (error)
    {
        LOG_ERROR("failed to connect to unix socket %s: %d", socket_name, error);
        switch (error)
        {
        case ETIMEDOUT:
        case ENETUNREACH:
        case ECONNREFUSED:
        case EADDRNOTAVAIL:
            return QS_NO_CONNECTION;
        default:
            return QS_SOCKET_ERROR;
        }
    }

    count = send(fd, rpc_request, sizeof(rpc_request) - 1, 0);
    if (count != sizeof(rpc_request) - 1)
    {
        LOG_ERROR("failed to send a request: %s", rpc_request);
        close(fd);
        return QS_SOCKET_ERROR;
    }

    LOG_DBG("sent a request: %s", rpc_request);

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
        LOG_ERROR("failed to set SO_RCVTIMEO: %d (%s)", errno, strerror(errno));
        close(fd);
        return QS_SOCKET_ERROR;
    }

    for (;;)
    {
        count = recv(fd, rpc_response + total, sizeof(rpc_response) - total - 1, 0);
        if (count < 0) {
            // for timeout -1 is returned with errno set to EAGAIN or EWOULDBLOCK
            switch (errno)
            {
            case EAGAIN:
                qstatus = QS_RECEIVE_TIMEOUT;
                break;
            default:
                qstatus = QS_SOCKET_ERROR;
            }

            LOG_DBG("recv failed: %d (%s)", errno, strerror(errno));
            break;
        }

        if (count == 0) {
            qstatus = QS_RECEIVE_TIMEOUT;
            LOG_DBG("connection closed");
            break;
        }

        LOG_DBG("received %zd bytes", count);
        total += count;

        rpc_response[total] = '\0';
        if (parse_jrpc(&parser, rpc_response)) {
            if (parser.id == 0 && parser.message_type == OVSDBMT_RESPONSE) {
                LOG_DBG("received valid JSON in response");
                LOG_DBG("  id    : %ld", parser.id);
                if (parser.result >= 0) {
                    LOG_DBG("  result: %s", rpc_response + parser.t[parser.result].start);
                }
                if (parser.error >= 0) {
                    LOG_DBG("  error : %s", rpc_response + parser.t[parser.error].start);
                }
                break;
            }
        }

        if ((ssize_t)sizeof(rpc_response) - 1 == total) {
            // no space left to receive data
            qstatus = QS_SYSTEM_ERROR;
            break;
        }
    }

    LOG_DBG("totally received %zd bytes", total);

    close(fd);

    if (qstatus != QS_SUCCESS) {
        LOG_DBG("failed to receive valid response: %s", rpc_response);
    }

    return qstatus;
}

daemon_status_t ovs_get_daemon_status(const char * target, const char * pidfile, pid_t * out_pid)
{
    pid_t          pid;
    query_status_t qs;

    LOG_INFO("checking process \"%s\"...", target);

    pid = ovs_get_pid(target, pidfile);

    if (pid <= 0) {
        LOG_WARN("failed to get pid from pidfile for process \"%s\"", target);
        pid = find_process(target);
    }

    *out_pid = pid;

    if (pid <= 0)
    {
        LOG_ERROR("failed to find pid by name for process \"%s\"", target);
        return DS_NO_PROCESS;
    }

    LOG_DBG("found process \"%s\" with pid: %d", target, pid);

    qs = ovs_query_daemon(target, pid);

    if (qs == QS_SUCCESS) {
        LOG_INFO("process \"%s\" is alive", target);
        return DS_ALIVE;
    }

    if (qs == QS_RECEIVE_TIMEOUT || qs == QS_NO_CONNECTION) {
        if (-1 == kill(pid, 0) && errno == ESRCH) {
            LOG_WARN("process \"%s\" is not responding", target);
            return DS_NO_RESPONSE;
        }

        LOG_ERROR("process \"%s\" is not alive", target);
        return DS_NOT_ALIVE;
    }

    return DS_SYSTEM_ERROR;
}

void ovs_check_daemon(const char * target, const char * pidfile, const char * cmd) {
    long           retries_count = get_conf()->request_retries;
    pid_t          pid;
    daemon_status_t status;

    if (retries_count <= 0)
        retries_count = 1;

    for (; retries_count; --retries_count)
    {
        status = ovs_get_daemon_status(target, pidfile, &pid);
        if (status == DS_ALIVE)
            return;

        if (status != DS_NO_RESPONSE)
            break;

        LOG_WARN("check attempt %ld of %ld has failed - retrying", get_conf()->request_retries - retries_count + 1, get_conf()->request_retries);
    }

    if (status == DS_NOT_ALIVE) {
        LOG_WARN("trying to kill the process \"%s\" with pid %d: %d (%s)", target, pid, errno, strerror(errno));
        if (-1 == kill(pid, SIGKILL)) {
            if (errno == EINVAL || errno == EPERM) {
                LOG_ERROR("failed to kill process \"%s\" with pid %d: %d (%s)", target, pid, errno, strerror(errno));
                chandler_stat()->failures_count += 1;
                return;
            }
        }
        else {
            LOG_WARN("killed the process \"%s\" with pid %d: %d (%s)", target, pid, errno, strerror(errno));
            chandler_stat()->kills_count += 1;
        }
    }

    // => DS_NO_PROCESS:
    if (0 != spawn_process_from_command(cmd)) {
        LOG_ERROR("failed to spawn a process for \"%s\"", target);
        chandler_stat()->failures_count += 1;
    }
    else {
        LOG_INFO("spawned a new process from command: %s", cmd);
        chandler_stat()->restarts_count += 1;
    }
}

void check_ovs(void)
{
    ovs_check_daemon(get_conf()->ovs_name_db, get_conf()->ovs_pidfile_db, get_conf()->ovs_cmd_db);
    ovs_check_daemon(get_conf()->ovs_name_switch, get_conf()->ovs_pidfile_switch, get_conf()->ovs_cmd_switch);
}
