
// Copyright (c) NetFoundry Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef UV_MBED_COMPRESSION_H
#define UV_MBED_COMPRESSION_H

#include <stdint.h>
#include <stdlib.h>

#if !defined(_SSIZE_T_) && !defined(_SSIZE_T_DEFINED)
typedef intptr_t ssize_t;
#ifndef SSIZE_MAX
# define SSIZE_MAX INTPTR_MAX
#endif
#ifndef _SSIZE_T_
# define _SSIZE_T_
#endif
#ifndef _SSIZE_T_DEFINED
# define _SSIZE_T_DEFINED
#endif
#endif

typedef struct tlsuv_http_inflater_s http_inflater_t;

#if __cplusplus
extern "C" {
#endif
typedef void (*data_cb)(void *ct, const char* data, ssize_t datalen);

extern const char *um_available_encoding(void);
extern http_inflater_t* um_get_inflater(const char *encoding, data_cb cb, void *ctx);
extern int um_inflate_state(http_inflater_t *inflater);
extern void um_free_inflater(http_inflater_t *inflater);

extern int um_inflate(http_inflater_t *inflater, const char* input, size_t input_len);


#if __cplusplus
}
#endif
#endif //UV_MBED_COMPRESSION_H
