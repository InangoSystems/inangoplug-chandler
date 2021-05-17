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
#ifndef CHANDLER_CONF_H
#define CHANDLER_CONF_H

#include "chandler_system.h"

typedef struct chandler_conf_t {
    char ovs_run_dir[MAX_PATH_SIZE];
    char ovs_name_switch[MAX_APP_NAME_SIZE];
    char ovs_name_db[MAX_APP_NAME_SIZE];
    char ovs_pidfile_switch[MAX_PATH_SIZE];
    char ovs_pidfile_db[MAX_PATH_SIZE];
    char ovs_unixctl_switch[MAX_PATH_SIZE];
    char ovs_unixctl_db[MAX_PATH_SIZE];
    char ovs_cmd_switch[MAX_COMMAND_SIZE];
    char ovs_cmd_db[MAX_COMMAND_SIZE];
    char ovs_cmd_disconnect[MAX_COMMAND_SIZE];
    char ovs_cmd_reboot[MAX_COMMAND_SIZE];
    char ovs_unixsock_db[MAX_PATH_SIZE];
    //char bridge_name[MAX_BR_NAME_SIZE];
    //char addrs[MAX_ADDR_COUNT][MAX_ADDR_SIZE];
    //char addrs[MAX_ADDR_SIZE * MAX_ADDR_COUNT];
    //long addrs_count;
    //char controller_addr[MAX_ADDR_SIZE];
    long check_interval;                         // services check interval in msec
    long request_retries;                        // number of retries to query daemons via JRPC before blaming them as not alive
    long receive_timeout;                        // timeout in msec for response receive operations
    long failures_before_reboot;                 // number of failures before decision to reboot the system
    long restarts_before_reboot;                 // number of daemons relaunches (after their death) before decision to reboot the system
} chandler_conf_t;

/* Loads configuration from file in format "key = value\n" */
int  load_conf_file(const char * conf_file_name);

/* Loads configuration from environment variables */
void load_conf_env(void);

/* Returns pointer to configuration struct */
const chandler_conf_t * get_conf(void);

#endif  /* CHANDLER_CONF_H */
