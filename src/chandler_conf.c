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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "chandler_conf.h"
#include "chandler_log.h"


#define DEFAULT_OVS_RUNDIR  "/usr/local/var/run/openvswitch"


typedef enum conf_value_type_t {
    VT_NONE,
    VT_STRING,
    VT_INTEGER
} conf_value_type_t;

typedef struct conf_value_t {
    const char        *name;
    const char        *env_name;
    conf_value_type_t  value_type;
    void              *target;
    size_t             size;
} conf_value_t;


static chandler_conf_t chandler_conf = {
    .ovs_run_dir            = DEFAULT_OVS_RUNDIR,
    .ovs_name_switch        = "ovs-vswitchd",
    .ovs_name_db            = "ovsdb-server",
    .ovs_pidfile_switch     = "",
    .ovs_pidfile_db         = "",
    .ovs_unixctl_switch     = "",
    .ovs_unixctl_db         = "",
    .ovs_cmd_switch         = "ovs-vswitchd unix:" DEFAULT_OVS_RUNDIR "/db.sock"
                              " --log-file=" DEFAULT_OVS_RUNDIR "/vswitchd.log"
                              " --pidfile=" DEFAULT_OVS_RUNDIR "/ovs-vswitchd.pid"
                              " --detach",
    .ovs_cmd_db             = "ovsdb-server " DEFAULT_OVS_RUNDIR "/conf.db"
                              " --remote=punix:" DEFAULT_OVS_RUNDIR "/db.sock"
                              " --log-file=" DEFAULT_OVS_RUNDIR "/ovsdb.log"
                              " --pidfile=" DEFAULT_OVS_RUNDIR "/ovsdb-server.pid"
                              " --detach",
    .ovs_cmd_disconnect     = "",
    .ovs_cmd_reboot         = "",
    .ovs_unixsock_db        = "",
    //.bridge_name            = "",
    //.controller_addr        = "",
    .check_interval         = CHECK_INTERVAL_MSEC,
    .request_retries        = 1,
    .receive_timeout        = RECV_TIMEOUT_MSEC,
    .failures_before_reboot = 0,
    .restarts_before_reboot = 0
};

static conf_value_t conf_values[] = {
    {"ovs_run_dir",            "CHANDLER_OVS_RUNDIR",         VT_STRING,  chandler_conf.ovs_run_dir,             sizeof(chandler_conf.ovs_run_dir)},
    {"ovs_name_switch",        "CHANDLER_NAME_SW",            VT_STRING,  chandler_conf.ovs_name_switch,         sizeof(chandler_conf.ovs_name_switch)},
    {"ovs_name_db",            "CHANDLER_NAME_DB",            VT_STRING,  chandler_conf.ovs_name_db,             sizeof(chandler_conf.ovs_name_db)},
    {"ovs_pidfile_switch",     "CHANDLER_PIDFILE_SW",         VT_STRING,  chandler_conf.ovs_pidfile_switch,      sizeof(chandler_conf.ovs_pidfile_switch)},
    {"ovs_pidfile_db",         "CHANDLER_PIDFILE_DB",         VT_STRING,  chandler_conf.ovs_pidfile_db,          sizeof(chandler_conf.ovs_pidfile_db)},
    {"ovs_unixctl_switch",     "CHANDLER_UNIXCTL_SW",         VT_STRING,  chandler_conf.ovs_unixctl_switch,      sizeof(chandler_conf.ovs_unixctl_switch)},
    {"ovs_unixctl_db",         "CHANDLER_UNIXCTL_DB",         VT_STRING,  chandler_conf.ovs_unixctl_db,          sizeof(chandler_conf.ovs_unixctl_db)},
    {"ovs_cmd_switch",         "CHANDLER_CMD_RUN_SW",         VT_STRING,  chandler_conf.ovs_cmd_switch,          sizeof(chandler_conf.ovs_cmd_switch)},
    {"ovs_cmd_db",             "CHANDLER_CMD_RUN_DB",         VT_STRING,  chandler_conf.ovs_cmd_db,              sizeof(chandler_conf.ovs_cmd_db)},
    {"ovs_cmd_disconnect",     "CHANDLER_CMD_DISCON",         VT_STRING,  chandler_conf.ovs_cmd_disconnect,      sizeof(chandler_conf.ovs_cmd_disconnect)},
    {"ovs_cmd_reboot",         "CHANDLER_CMD_REBOOT",         VT_STRING,  chandler_conf.ovs_cmd_reboot,          sizeof(chandler_conf.ovs_cmd_reboot)},
    {"ovs_unixsock_db",        "CHANDLER_UNIXSOCK_DB",        VT_STRING,  chandler_conf.ovs_unixsock_db,         sizeof(chandler_conf.ovs_unixsock_db)},
    //{"bridge_name",            "CHANDLER_BRIDGE",             VT_STRING,  chandler_conf.bridge_name,             sizeof(chandler_conf.bridge_name)},
    //{"addrs",                  NULL,                     VT_STRING,  chandler_conf.addrs,                   sizeof(chandler_conf.addrs)},
    //{"addrs_count",            NULL,                     VT_INTEGER, &chandler_conf.addrs_count,            0},
    //{"controller_addr",        "CHANDLER_OVS_CONTROLLER",     VT_STRING,  chandler_conf.controller_addr,         sizeof(chandler_conf.controller_addr)},
    {"check_interval",         "CHANDLER_CHECK_INTERVAL",     VT_INTEGER, &chandler_conf.check_interval,         0},
    {"request_retries",        "CHANDLER_REQ_RETRIES",        VT_INTEGER, &chandler_conf.request_retries,        0},
    {"receive_timeout",        "CHANDLER_RECV_TIMEOUT",       VT_INTEGER, &chandler_conf.receive_timeout,        0},
    {"failures_before_reboot", "CHANDLER_FAILURES_TO_REBOOT", VT_INTEGER, &chandler_conf.failures_before_reboot, 0},
    {"restarts_before_reboot", "CHANDLER_RESTARTS_TO_REBOOT", VT_INTEGER, &chandler_conf.restarts_before_reboot, 0},
    {NULL,                     NULL,                     VT_NONE,    NULL,                             0}
};


void load_conf_env(void)
{
    long  long_value;
    char *end;

    for (conf_value_t *conf_value = conf_values; conf_value->name != NULL; ++conf_value)
    {
        if (conf_value->env_name == NULL)
            continue;

        const char *value = getenv(conf_value->env_name);

        if (!value || !value[0])
            continue;

        switch (conf_value->value_type)
        {
        case VT_STRING:
            if (strlen(value) >= conf_value->size)
            {
                LOG_ERROR("Failed to read string value for key \"%s\" from environment variable \"%s\"", conf_value->name, conf_value->env_name);
            }
            else
            {
                strncpy(conf_value->target, value, conf_value->size);
                ((char *)conf_value->target)[conf_value->size - 1] = '\0';
            }
            break;

        case VT_INTEGER:
            long_value = strtol(value, &end, 10);
            if (*end != '\0')
            {
                LOG_ERROR("Failed to read integer value for key \"%s\" from environment variable \"%s\"", conf_value->name, conf_value->env_name);
            }
            else
            {
                *(long *)conf_value->target = long_value;
            }
            break;

        default:
            LOG_ERROR("Unsupported configuration value type \"%conf_value\" for key \"%s\"", conf_value->value_type, conf_value->name);
        }
    }
}

static int get_key_value(char * line_buf, size_t line_size, char ** key_ptr, size_t * key_size, char ** value_ptr, size_t * value_size)
{
    char   *delimiter;
    char   *key;
    char   *value;
    char   *end;

    delimiter = strchr(line_buf, '=');
    if (!delimiter) {
        return -1;
    }

    for (key = line_buf; *key == ' ';) {
        ++key;
    }

    if (*key == '=') {
        return -1;
    }

    for (end = delimiter - 1; *end == ' ';) {
        --end;
    }

    ++end;
    *end = '\0';

    *key_size = end - key + 1;

    for (value = delimiter + 1; *value == ' ';) {
        ++value;
    }

    if (*value == '\n') {
        return -1;
    }

    for (end = line_buf + line_size - 1; *end == '\n' || *end == ' ';) {
        --end;
    }

    ++end;
    *end = '\0';

    *value_size = end - value + 1;

    *key_ptr   = key;
    *value_ptr = value;

    return 0;
}

static int update_conf_key_value(const char *key, const char *value, size_t value_size)
{
    long  long_value;
    char *end;

    for (conf_value_t *conf_value = conf_values; conf_value->name != NULL; ++conf_value) {
        if (0 == strcmp(conf_value->name, key)) {
            switch (conf_value->value_type) {
            case VT_STRING:
                if (value_size > conf_value->size) {
                    LOG_ERROR("Failed to read string value for key \"%s\" from configuration", key);
                    return -1;
                }
                strncpy(conf_value->target, value, value_size);
                break;

            case VT_INTEGER:
                long_value = strtol(value, &end, 10);
                if (*end != '\0') {
                    LOG_ERROR("Failed to read integer value for key \"%s\" from configuration", key);
                    return -1;
                }
                *(long *)conf_value->target = long_value;
                break;

            default:
                LOG_ERROR("Unsupported configuration value type \"%d\" for key \"%s\"", conf_value->value_type, conf_value->name);
                return -1;
            }

            return 0;
        }
    }

    /* Unknown key - for now not an error */
    return 0;
}

const chandler_conf_t * get_conf(void)
{
    return &chandler_conf;
}

int load_conf_file(const char * conf_file_name)
{
    int      error         = 0;
    char    *line_buf      = NULL;
    size_t   line_buf_size = 0;
    int      line_count    = 0;
    ssize_t  line_size;
    FILE    *fp;
    char    *key;
    size_t   key_size;
    char    *value;
    size_t   value_size;

    fp = fopen(conf_file_name, "r");
    if (!fp)
    {
        LOG_ERROR("Failed to open file \"%s\"", conf_file_name);
        return errno;
    }

    line_size = getline(&line_buf, &line_buf_size, fp);

    while (line_size >= 0)
    {
        ++line_count;

        error = get_key_value(line_buf, line_size, &key, &key_size, &value, &value_size);
        if (error != 0) {
            break;
        }

        error = update_conf_key_value(key, value, value_size);
        if (error != 0) {
            break;
        }

        line_size = getline(&line_buf, &line_buf_size, fp);
    }

    if (!error) {
        error = errno;
    }

    if (error == EINVAL || error == ENOMEM) {
        LOG_ERROR("Failed to read file \"%s\": errno = %d, line = %d", conf_file_name, error, line_count);
    }

    free(line_buf);
    fclose(fp);

    return error;
}
