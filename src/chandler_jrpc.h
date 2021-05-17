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
#ifndef CHANDLER_JRPC_H
#define CHANDLER_JRPC_H

#include "chandler_json.h"

#define MAX_TOKENS_COUNT  128

#define ID_NOT_FOUND      (-1)  // field "id" was not found in JRPC message
#define ID_NULL           (-2)  // field "id" was set to null

#define TOKEN_NOT_FOUND   (-1)  // token was not found in JRPC message
#define TOKEN_NULL        (-2)  // token was found in JRPC message and equals to null primitive

/* Type of incoming JRPC message from OVSDB */
typedef enum ovsdb_message_type_t {
    OVSDBMT_UNKNOWN,
    OVSDBMT_RESPONSE,
    OVSDBMT_METHOD_UPDATE
} ovsdb_message_type_t;

/* Structure contains parsing information for incoming JRPC message */
typedef struct ovsdb_message_parser_t {
    jsmntok_t             t[MAX_TOKENS_COUNT];  // array of tokens, used to parse JSON object
    int                   count;                // number of tokens in parsed JSON object
    char                 *end;                  // pointer to the upper bound of JSON object in parsed JRPC string
    long                  id;                   // value of id field from JRPC mesasge (can also be equal to ID_NOT_FOUND or ID_NULL)
    int                   error;                // index of token related to "error" field value from JRPC string (-1 if not found)
    int                   result;               // index of token related to "result" field value from JRPC string (-1 if not found)
    int                   method;               // index of token related to "method" field value from JRPC string (-1 if not found)
    int                   params;               // index of token related to "params" field value from JRPC string (-1 if not found)
    ovsdb_message_type_t  message_type;         // type of the parsed JRPC messge
} ovsdb_message_parser_t;

/**
 * Function initializes \arg parser, tries to parse the JRPC message referenced by \arg str and
 * on successful parsing sets appropriate fields in \arg parser struct.
 *
 * \param parser  Pointer to ovsdb_message_parser_t structure
 * \param str     JSON string
 *
 * \return        1 - on success, 0 - on failure
 */
int parse_jrpc(ovsdb_message_parser_t * parser, char * str);

#endif  /* CHANDLER_JRPC_H */
