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

#include <string.h>

/* Implementation wrapper for jsmn library */
#include "jsmn.h"


static int is_json_token_equal_to(const char * json, jsmntok_t * token, jsmntype_t token_type, const char * s)
{
    if (   token->type == token_type
        && (int)strlen(s) == token->end - token->start
        && 0 == strncmp(json + token->start, s, token->end - token->start)
    )
    {
        return 1;
    }

    return 0;
}

int is_json_token_equal_to_str(const char * json, jsmntok_t * token, const char * s)
{
    return is_json_token_equal_to(json, token, JSMN_STRING, s);
}

int is_json_token_equal_to_primitive(const char * json, jsmntok_t * token, const char * s)
{
    return is_json_token_equal_to(json, token, JSMN_PRIMITIVE, s);
}

int is_json_token_equal_to_null(const char * json, jsmntok_t * token)
{
    if (   token->type == JSMN_PRIMITIVE
        && 4 == token->end - token->start
        && *(json + token->start) == 'n'
    )
    {
        return 1;
    }

    return 0;
}

/**
 * Returns total number of tokens building the provided token. It includes the
 * provided token and all other tokens which are part of it.
 *
 * \param t      Array of tokens. t[0] is the token which weight is calculated.
 * \param count  Total number of token in array \arg t
 *
 * \return       Number of tokens building the token t[0]
 */
static int token_weight(jsmntok_t * t, int count)
{
    jsmntok_t *key;
    int        i;
    int        weight = 0;

    if (count == 0) {
        return 0;
    }

    switch (t->type)
    {
    case JSMN_PRIMITIVE:
    case JSMN_STRING:
        return 1;

    case JSMN_OBJECT:
        for (i = 0; i < t->size; ++i) {
            key = t + 1 + weight;
            weight += token_weight(key, count - weight);
            if (key->size > 0) {
                weight += token_weight(t + 1 + weight, count - weight);
            }
        }

        return weight + 1;

    case JSMN_ARRAY:
        for (i = 0; i < t->size; ++i) {
            weight += token_weight(t + 1 + weight, count - weight);
        }

        return weight + 1;

    default:
        return 0;
    }
}

int json_next_index(jsmntok_t * tokens, int count, int index)
{
    if (index >= count) {
        return count;
    }

    return index + token_weight(tokens + index, count - index);
}
