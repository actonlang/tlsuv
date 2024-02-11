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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#else
#include <sys/param.h>
#endif
#include "common.h"
#include "bio.h"

struct msg {
    size_t len;
    uint8_t *buf;

    STAILQ_ENTRY(msg) next;
};

tlsuv_BIO *tlsuv_BIO_new(void) {
    tlsuv_BIO * bio = tlsuv_calloc(1, sizeof(tlsuv_BIO));
    bio->available = 0;
    bio->headoffset = 0;
    bio->qlen = 0;

    STAILQ_INIT(&bio->message_q);
    return bio;
}

void tlsuv_BIO_free(tlsuv_BIO *bio) {
    while(!STAILQ_EMPTY(&bio->message_q)) {
        struct msg *m = STAILQ_FIRST(&bio->message_q);
        STAILQ_REMOVE_HEAD(&bio->message_q, next);
        tlsuv_free(m->buf);
        tlsuv_free(m);
    }

    tlsuv_free(bio);
}

size_t tlsuv_BIO_available(tlsuv_BIO *bio) {
    return bio->available;
}

int tlsuv_BIO_put(tlsuv_BIO *bio, const uint8_t *buf, size_t len) {
    struct msg *m = tlsuv_malloc(sizeof(struct msg));
    if (m == NULL) {
        return -1;
    }

    m->buf = tlsuv_malloc(len);
    if (m->buf == NULL) {
        tlsuv_free(m);
        return -1;
    }
    memcpy(m->buf, buf, len);

    m->len = len;

    STAILQ_INSERT_TAIL(&bio->message_q, m, next);
    bio->available += len;
    bio->qlen += 1;

    return 0;
}

int tlsuv_BIO_read(tlsuv_BIO *bio, uint8_t *buf, size_t len) {

    size_t total = 0;

    while (! STAILQ_EMPTY(&bio->message_q) && total < len) {
        struct msg *m = STAILQ_FIRST(&bio->message_q);

        size_t recv_size = MIN(len - total, m->len - bio->headoffset);
        memcpy(buf + total, m->buf + bio->headoffset, recv_size);
        bio->headoffset += recv_size;
        bio->available -= recv_size;
        total += recv_size;

        if (bio->headoffset == m->len) {
            STAILQ_REMOVE_HEAD(&bio->message_q, next);
            bio->headoffset = 0;
            bio->qlen -= 1;

            tlsuv_free(m->buf);
            tlsuv_free(m);
        }
    }

    return (int) total;
}
