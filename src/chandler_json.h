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
#ifndef CHANDLER_JSON_H
#define CHANDLER_JSON_H

/* Header-only wrapper for jsmn library to allow it being included in multiple .c files */
#define JSMN_HEADER
#include "jsmn.h"

/**
 * Checks the token is of string type and compares its value to provided string
 * \arg s.
 *
 * \param json   String containing JSON
 * \param token  Valid token related to \arg json
 * \param s      String value to compare with \arg token value
 *
 * \return       0 if not equal or token is not a string
 */
int is_json_token_equal_to_str(const char * json, jsmntok_t * token, const char * s);

/**
 * Checks the token is of primitive type (null, false, true or some number) and
 * compares its value to provided string \arg s.
 *
 * \param json   String containing JSON
 * \param token  Valid token related to \arg json
 * \param s      String value to compare with \arg token value
 *
 * \return       0 if not equal or token is not a primitive
 */
int is_json_token_equal_to_primitive(const char * json, jsmntok_t * token, const char * s);

/**
 * Checks the token is of primitive type and is null.
 *
 * \param json   String containing JSON
 * \param token  Valid token related to \arg json
 *
 * \return       0 if not null
 */
int is_json_token_equal_to_null(const char * json, jsmntok_t * token);

/**
 * Returns index of the next token of the same level with the provided token
 * index. Keys and values of an object are supposed to be on the same level.
 *
 * \param tokens  Array of tokens
 * \param count   Total number of tokens
 * \param index   Index of the token in tokens array
 *
 * \return        Index of the next token on success, or the \arg count on failure
 */
int json_next_index(jsmntok_t * tokens, int count, int index);

#endif  /* CHANDLER_JSON_H */
