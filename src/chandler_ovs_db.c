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
#include "chandler_ovs_db.h"

#include "chandler_conf.h"
#include "chandler_jrpc.h"
#include "chandler_log.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


static char rpc_request_monitor[] = "{\"id\":0,\"method\":\"monitor\",\"params\":[\"Open_vSwitch\",null,{\"Controller\":[{\"columns\":[\"is_connected\"]}]}]}";
/* response sample:
 * {
 *     "id":0,
 *     "result":{"Controller":{
 *         "5702e5df-220d-4eb0-ace6-ea2ae02770cd":{"new":{"is_connected":false"}},
 *         "8849b2f6-1049-4fa6-84ea-dabffadbc370":{"new":{"is_connected":false"}},
 *         "b85f9c78-438a-4b6d-9492-3dd907f3897c":{"new":{"is_connected":false"}},
 *         "3713513d-b7dc-4afc-b7e6-6aa99b7fedad":{"new":{"is_connected":false"}}
 *     }},
 *     "error":null
 * }
 */

void handle_controller_changes(ovsdb_monitor_t * monitor, jsmntok_t * t, int count)
{
    const char *json = monitor->buffer;
    jsmntok_t  *t_row;
    int         count_row;
    int         upper_bound = json_next_index(t, count, 0);

    /* sample of JSON part, which should be referenced by token t
     * {
     *    "afc1a2e8-e999-49df-ab4e-943b3a2cdaf0":{"new":{"is_connected":false}},
     *    "b85f9c78-438a-4b6d-9492-3dd907f3897c":{"old":{"is_connected":true}}
     * }
     */

    for (int i = 1; i < upper_bound; i = json_next_index(t, count, i)) {
        /* i = index of key equal to uuid */
        if (t[i].type != JSMN_STRING) {
            return;
        }

        ++i;  /* skip row uuid */

        /* i = index of value {"new":{"is_connected":false}} */
        if (t[i].type != JSMN_OBJECT || t[i].size == 0) {
            return;
        }

        /* i + 1 = index of first key which should be "new" or "old" */
        /* i + 2 = index of value related to key - should be object of table field names as keys and string values */
        if (   is_json_token_equal_to_str(json, &t[i + 1], "new")
            && t[i + 1].size == 1
            && t[i + 2].type == JSMN_OBJECT
        )
        {
            t_row     = t + i + 2;                                   /* token related to object of table's fields */
            count_row = json_next_index(t_row, count - (i + 2), 0);  /* number of tokens comprising the t_row */

            for (int j = 1; j < count_row; j = json_next_index(t_row, (count - (i + 2)) - (j + 1), j + 1)) {
                /* (t_row + j) - token of the key */
                /* (t_row + j + 1) - token of the value */
                if (is_json_token_equal_to_str(json, t_row + j, "is_connected")) {

                    if (is_json_token_equal_to_primitive(json, t_row + j + 1, "false")) {
                        LOG_DBG("found tables::controller::is_connected == false");
                        if (monitor->on_disconnect != NULL) {
                            monitor->on_disconnect();
                        }

                        return;  /* first located is enough */
                    }

                    break;  /* lets look for the next row */
                }
            }
        }
    }
}

static void handle_changes(struct ovsdb_monitor_t * monitor, jsmntok_t * t, int count)
{
    if (t->type == JSMN_OBJECT && t->size > 0) {
        /* look for "Controller" key */
        int upper_bound = json_next_index(t, count, 0);

        for (int i = 1; i < upper_bound; i = json_next_index(t, upper_bound, i + 1))
        {
            if (is_json_token_equal_to_str(monitor->buffer, t + i, "Controller")) {
                ++i;
                handle_controller_changes(monitor, t + i, upper_bound - i);
                break;
            }
        }
    }
}

static void handle_notifications(struct ovsdb_monitor_t * monitor)
{
    ovsdb_message_parser_t  parser;
    int                     i;
    jsmntok_t              *t;

    LOG_DBG("monitor.buffer.size: %zd", monitor->size);

    while (monitor->size > 0 && parse_jrpc(&parser, monitor->buffer))
    {
        if (parser.id == ID_NULL && parser.message_type == OVSDBMT_METHOD_UPDATE) {
            /* handle notification */
            if (parser.params >= 0) {
                t = parser.t + parser.params;

                if (t->type == JSMN_ARRAY && t->size > 1) {
                    /* index of the second element in the array */
                    i = json_next_index(parser.t, parser.count, parser.params + 1);

                    handle_changes(monitor, parser.t + i, parser.count - i);
                }
            }
        }

        monitor->size -= (parser.end - monitor->buffer);
        memmove(monitor->buffer, parser.end, monitor->size + 1);

        LOG_DBG("monitor.buffer.size: %zd", monitor->size);
    }
}

static query_status_t  on_read(struct ovsdb_monitor_t * monitor)
{
    ssize_t count = recv(monitor->fd, monitor->buffer + monitor->size, sizeof(monitor->buffer) - monitor->size - 1, 0);

    if (count < 0) {
        /* for timeout -1 is returned with errno set to EAGAIN or EWOULDBLOCK */
        LOG_DBG("recv failed: %d (%s)", errno, strerror(errno));

        if (errno == EAGAIN) {
            return QS_RECEIVE_TIMEOUT;
        }

        return QS_SOCKET_ERROR;
    }

    if (count == 0) {
        LOG_DBG("connection closed");
        return QS_CONNECTION_CLOSED;
    }

    LOG_DBG("received %zd bytes", count);
    monitor->size += count;

    monitor->buffer[monitor->size] = '\0';

    handle_notifications(monitor);

    if ((ssize_t)sizeof(monitor->buffer) - 1 == monitor->size) {
        /* no space left to receive data */
        return QS_SYSTEM_ERROR;
    }

    return QS_SUCCESS;
}

query_status_t monitor_create(const char * sock_path, ovsdb_monitor_t * monitor, ovsdb_disconnect_handler_t on_disconnect)
{
    struct timeval         tv = {.tv_sec = get_conf()->receive_timeout / 1000, .tv_usec = 1000*(get_conf()->receive_timeout % 1000)};
    int                    fd;
    ssize_t                count;
    ssize_t                total = 0;
    int                    error;
    query_status_t         status = QS_SUCCESS;
    ovsdb_message_parser_t parser;

    monitor->fd = -1;
    monitor->size = 0;
    monitor->on_read = on_read;
    monitor->on_disconnect = on_disconnect;

    /* connect */
    error = connect_unix_socket(SOCK_STREAM, sock_path, &fd);
    if (error)
    {
        LOG_ERROR("failed to connect to unix socket %s: %d", sock_path, error);
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

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
        LOG_ERROR("failed to set SO_RCVTIMEO: %d (%s)", errno, strerror(errno));
        close(fd);
        return QS_SOCKET_ERROR;
    }

    /* request/response */
    count = send(fd, rpc_request_monitor, sizeof(rpc_request_monitor) - 1, 0);
    if (count != sizeof(rpc_request_monitor) - 1)
    {
        LOG_ERROR("failed to send a request: %s", rpc_request_monitor);
        close(fd);
        return QS_SOCKET_ERROR;
    }

    LOG_DBG("sent a request: %s", rpc_request_monitor);

    for (;;)
    {
        count = recv(fd, monitor->buffer + total, sizeof(monitor->buffer) - total - 1, 0);
        if (count < 0) {
            /* for timeout -1 is returned with errno set to EAGAIN or EWOULDBLOCK */
            switch (errno)
            {
            case EAGAIN:
                status = QS_RECEIVE_TIMEOUT;
                break;
            default:
                status = QS_SOCKET_ERROR;
            }

            LOG_DBG("recv failed: %d (%s)", errno, strerror(errno));
            break;
        }

        if (count == 0) {
            status = QS_RECEIVE_TIMEOUT;
            LOG_DBG("connection closed");
            break;
        }

        LOG_DBG("received %zd bytes", count);
        total += count;

        monitor->buffer[total] = '\0';
        if (parse_jrpc(&parser, monitor->buffer)) {
            if (parser.id == 0 && parser.message_type == OVSDBMT_RESPONSE)
            {
                LOG_DBG("received valid JSON in response");
                LOG_DBG("  id    : %ld", parser.id);

                if (parser.result >= 0) {
                    LOG_DBG("  result: %s", monitor->buffer + parser.t[parser.result].start);
                }

                if (parser.error >= 0) {
                    LOG_DBG("  error : %s", monitor->buffer + parser.t[parser.error].start);
                }

                /* handle response */
                if (parser.result >= 0) {
                    handle_changes(monitor, parser.t + parser.result, parser.count - parser.result);
                }
                else if (parser.error >= 0) {
                    status = QS_RETURNED_ERROR;
                }

                monitor->size = total - (parser.end - monitor->buffer);
                memmove(monitor->buffer, parser.end, monitor->size + 1);

                handle_notifications(monitor);
            }
            else {
                status = QS_PROTOCOL_ERROR;
            }
            break;
        }

        if ((ssize_t)sizeof(monitor->buffer) - 1 == total) {
            /* no space left to receive data */
            status = QS_SYSTEM_ERROR;
            break;
        }
    }

    LOG_DBG("totally received %zd bytes", total);

    if (status != QS_SUCCESS) {
        LOG_DBG("failed to receive valid response: %s", monitor->buffer);
        close(fd);
    }
    else {
        monitor->fd = fd;
    }

    return status;
}

void monitor_destroy(ovsdb_monitor_t * monitor)
{
    close(monitor->fd);
    monitor->fd = -1;
}
