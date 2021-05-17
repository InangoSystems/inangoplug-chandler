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
#ifndef CHANDLER_SYSTEM_H
#define CHANDLER_SYSTEM_H

#include <fcntl.h>
#include <sys/wait.h>
#include <wchar.h>

#define MAX_APP_NAME_SIZE     64
#define MAX_PATH_SIZE         256
#define MAX_COMMAND_SIZE      1024
#define MAX_COMMAND_ARGS      16
#define MAX_REQUEST_SIZE      32768
#define MAX_RESPONSE_SIZE     32768
#define MAX_ADDR_SIZE         128
#define MAX_ADDR_COUNT        4
#define MAX_BR_NAME_SIZE      64
#define MAX_IF_NAME_SIZE      64
#define MAX_ENV_VALUE_SIZE    128

#define CHECK_INTERVAL_MSEC   60000
#define RECV_TIMEOUT_MSEC     15000


typedef enum query_status_t {
    QS_SUCCESS,
    QS_UNIX_SOCKET_NAME_ERROR,
    QS_SOCKET_ERROR,
    QS_SYSTEM_ERROR,
    QS_NO_CONNECTION,
    QS_RECEIVE_TIMEOUT,
    QS_PROTOCOL_ERROR,
    QS_RETURNED_ERROR,
    QS_CONNECTION_CLOSED
} query_status_t;


pid_t   find_process(const char * name);

pid_t   read_pid_from_file(const char * pid_file);

int     connect_unix_socket(int style, const char * path, int * fd);

int     spawn_process(const char * path, char * const * args);

int     spawn_process_from_command(const char * command_line);

int     timer_create_repeated(long interval_msec);

int     timer_destroy(int fd);

int     system_reboot(void);

#endif  /* CHANDLER_SYSTEM_H */
