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
#include "chandler_jrpc.h"

#include "chandler_log.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/**
 * Some examples of incoming JRPC messages.
 *
 * Commands list response:
 * {
 *     "id":0,
 *     "result":"List of available commands\n...",
 *     "error":null
 * }
 *
 * Monitor request response:
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
 *
 * Monitor notification:
 * {
 *     "id":null,
 *     "method":"update",
 *     "params":[
 *          null,
 *          {"Controller":{
 *              "afc1a2e8-e999-49df-ab4e-943b3a2cdaf0":{"new":{"is_connected":true}},
 *              "b85f9c78-438a-4b6d-9492-3dd907f3897c":{"old":{"is_connected":false}}
 *          }}
 *     ]
 * }
 */
int parse_jrpc(ovsdb_message_parser_t * parser, char * str)
{
    jsmn_parser  p;
    char         chr;
    char        *end;

    jsmn_init(&p);

    parser->message_type = OVSDBMT_UNKNOWN;
    parser->id           = ID_NOT_FOUND;
    parser->error        = TOKEN_NOT_FOUND;
    parser->result       = TOKEN_NOT_FOUND;
    parser->method       = TOKEN_NOT_FOUND;
    parser->params       = TOKEN_NOT_FOUND;
    parser->end          = NULL;

    parser->count = jsmn_parse(&p, str, strlen(str), parser->t, MAX_TOKENS_COUNT);
    if (parser->count < 0) {
        LOG_ERROR("failed to parse JSON: %d", parser->count);
        return 0;
    }

    if (parser->count == 0 || parser->t[0].type != JSMN_OBJECT) {
        LOG_ERROR("no JSON object");
        return 0;
    }

    for (int i = 1; i < parser->count; i = json_next_index(parser->t, parser->count, i)) {
        if (is_json_token_equal_to_str(str, &parser->t[i], "id")) {
            ++i;
            end = str + parser->t[i].end;
            chr = *end;
            *end = '\0';
            if (is_json_token_equal_to_null(str, &parser->t[i])) {
                parser->id = ID_NULL;
            }
            else {
                parser->id = strtol(str + parser->t[i].start, NULL, 10);
            }
            *end = chr;
        }
        else if (is_json_token_equal_to_str(str, &parser->t[i], "error")) {
            ++i;
            *(str + parser->t[i].end) = '\0';
            if (is_json_token_equal_to_null(str, &parser->t[i])) {
                parser->error = TOKEN_NULL;
            }
            else {
                parser->error = i;
            }
        }
        else if (is_json_token_equal_to_str(str, &parser->t[i], "result")) {
            ++i;
            *(str + parser->t[i].end) = '\0';
            parser->message_type = OVSDBMT_RESPONSE;
            if (is_json_token_equal_to_null(str, &parser->t[i])) {
                parser->result = TOKEN_NULL;
            }
            else {
                parser->result = i;
            }
        }
        else if (is_json_token_equal_to_str(str, &parser->t[i], "method")) {
            ++i;
            *(str + parser->t[i].end) = '\0';
            if (is_json_token_equal_to_null(str, &parser->t[i])) {
                parser->method = TOKEN_NULL;
            }
            else {
                parser->method = i;
                if (is_json_token_equal_to_str(str, &parser->t[i], "update")) {
                    parser->message_type = OVSDBMT_METHOD_UPDATE;
                }
            }
        }
        else if (is_json_token_equal_to_str(str, &parser->t[i], "params")) {
            ++i;
            if (is_json_token_equal_to_null(str, &parser->t[i])) {
                parser->params = TOKEN_NULL;
            }
            else {
                parser->params = i;
            }
        }
        else {
            ++i;
        }
    }

    parser->end = str + parser->t[0].end;

    return 1;
}
