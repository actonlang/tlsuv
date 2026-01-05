// Copyright (c) 2024. NetFoundry Inc.
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

#include <catch2/catch_all.hpp>
#include <tlsuv/tlsuv.h>
#include <uv.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "fixtures.h"

#define to_str_(x) #x
#define to_str(x) to_str_(x)
static const char *test_server_cert = to_str(TEST_SERVER_CERT);
static const char *test_server_key = to_str(TEST_SERVER_KEY);

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <unistd.h>
#endif

struct tls_pair_state {
    uv_loop_t *loop;
    tls_context *server_tls;
    tls_context *client_tls;
    tlsuv_private_key_t key;
    tlsuv_certificate_t cert;
    uv_tcp_t *listener;
    tlsuv_stream_t *server_stream;
    tlsuv_stream_t *client_stream;
    bool done;
    bool success;
    char error[256];
};

static void alloc_buf(uv_handle_t *, size_t size, uv_buf_t *buf) {
    buf->base = static_cast<char *>(calloc(1, size));
    buf->len = size;
}

static void on_stream_closed(uv_handle_t *h) {
    free(h);
}

static void on_listener_closed(uv_handle_t *h) {
    free(h);
}

static void finish(tls_pair_state *st, bool success, const char *msg) {
    if (st->done) {
        return;
    }
    st->done = true;
    st->success = success;
    if (!success && msg) {
        snprintf(st->error, sizeof(st->error), "%s", msg);
    }

    if (st->client_stream) {
        tlsuv_stream_close(st->client_stream, on_stream_closed);
        st->client_stream = nullptr;
    }
    if (st->server_stream) {
        tlsuv_stream_close(st->server_stream, on_stream_closed);
        st->server_stream = nullptr;
    }
    if (st->listener && !uv_is_closing((uv_handle_t *) st->listener)) {
        uv_close((uv_handle_t *) st->listener, on_listener_closed);
        st->listener = nullptr;
    }
}

static void on_client_write(uv_write_t *req, int status) {
    auto *st = static_cast<tls_pair_state *>(req->data);
    free(req);
    if (status != 0) {
        finish(st, false, uv_strerror(status));
    }
}

static void on_client_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    auto *st = static_cast<tls_pair_state *>(stream->data);
    if (nread > 0) {
        if (nread >= 4 && memcmp(buf->base, "PONG", 4) == 0) {
            finish(st, true, nullptr);
        } else {
            finish(st, false, "unexpected client response");
        }
    } else if (nread < 0) {
        finish(st, false, uv_strerror((int)nread));
    }
    free(buf->base);
}

static void on_client_connect(uv_connect_t *req, int status) {
    auto *st = static_cast<tls_pair_state *>(req->data);
    free(req);
    if (status != 0) {
        finish(st, false, uv_strerror(status));
        return;
    }

    auto *stream = reinterpret_cast<tlsuv_stream_t *>(st->client_stream);
    int rc = tlsuv_stream_read_start(stream, alloc_buf, on_client_read);
    if (rc != 0) {
        finish(st, false, uv_strerror(rc));
        return;
    }

    auto *wr = static_cast<uv_write_t *>(calloc(1, sizeof(uv_write_t)));
    wr->data = st;
    uv_buf_t buf = uv_buf_init(const_cast<char *>("PING"), 4);
    rc = tlsuv_stream_write(wr, stream, &buf, on_client_write);
    if (rc != 0) {
        free(wr);
        finish(st, false, uv_strerror(rc));
    }
}

static void on_server_write(uv_write_t *req, int status) {
    auto *st = static_cast<tls_pair_state *>(req->data);
    auto *stream = reinterpret_cast<tlsuv_stream_t *>(req->handle);
    free(req);
    if (status != 0) {
        finish(st, false, uv_strerror(status));
        return;
    }
    tlsuv_stream_close(stream, on_stream_closed);
}

static void on_server_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    auto *st = static_cast<tls_pair_state *>(stream->data);
    if (nread > 0) {
        if (nread >= 4 && memcmp(buf->base, "PING", 4) == 0) {
            auto *wr = static_cast<uv_write_t *>(calloc(1, sizeof(uv_write_t)));
            wr->data = st;
            uv_buf_t out = uv_buf_init(const_cast<char *>("PONG"), 4);
            int rc = tlsuv_stream_write(wr, reinterpret_cast<tlsuv_stream_t *>(stream), &out, on_server_write);
            if (rc != 0) {
                free(wr);
                finish(st, false, uv_strerror(rc));
            }
        } else {
            finish(st, false, "unexpected server request");
        }
    } else if (nread < 0) {
        finish(st, false, uv_strerror((int)nread));
    }
    free(buf->base);
}

static void on_server_handshake(uv_connect_t *req, int status) {
    auto *st = static_cast<tls_pair_state *>(req->data);
    free(req);
    if (status != 0) {
        finish(st, false, uv_strerror(status));
        return;
    }

    auto *stream = reinterpret_cast<tlsuv_stream_t *>(st->server_stream);
    int rc = tlsuv_stream_read_start(stream, alloc_buf, on_server_read);
    if (rc != 0) {
        finish(st, false, uv_strerror(rc));
    }
}

static void on_server_accept(uv_stream_t *server, int status) {
    auto *st = static_cast<tls_pair_state *>(server->data);
    if (status != 0) {
        finish(st, false, uv_strerror(status));
        return;
    }

    auto *client = static_cast<uv_tcp_t *>(calloc(1, sizeof(uv_tcp_t)));
    uv_tcp_init(st->loop, client);
    int rc = uv_accept(server, reinterpret_cast<uv_stream_t *>(client));
    if (rc != 0) {
        uv_close(reinterpret_cast<uv_handle_t *>(client), on_listener_closed);
        finish(st, false, uv_strerror(rc));
        return;
    }

    uv_os_fd_t fd;
    rc = uv_fileno(reinterpret_cast<uv_handle_t *>(client), &fd);
    if (rc != 0) {
        uv_close(reinterpret_cast<uv_handle_t *>(client), on_listener_closed);
        finish(st, false, uv_strerror(rc));
        return;
    }

#if !defined(_WIN32)
    int dup_fd = dup((int)fd);
    uv_close(reinterpret_cast<uv_handle_t *>(client), on_listener_closed);
    if (dup_fd < 0) {
        finish(st, false, "failed to duplicate accepted socket");
        return;
    }
    fd = (uv_os_fd_t)dup_fd;
#endif

    auto *stream = static_cast<tlsuv_stream_t *>(calloc(1, sizeof(tlsuv_stream_t)));
    tlsuv_stream_init(st->loop, stream, st->server_tls);
    tlsuv_stream_set_server(stream, 1);
    stream->data = st;
    st->server_stream = stream;

    auto *creq = static_cast<uv_connect_t *>(calloc(1, sizeof(uv_connect_t)));
    creq->data = st;
    rc = tlsuv_stream_open(creq, stream, (uv_os_sock_t)fd, on_server_handshake);
    if (rc != 0) {
        free(creq);
        finish(st, false, uv_strerror(rc));
        return;
    }

    if (!uv_is_closing(reinterpret_cast<uv_handle_t *>(server))) {
        uv_close(reinterpret_cast<uv_handle_t *>(server), on_listener_closed);
        st->listener = nullptr;
    }
}

static void start_client(tls_pair_state *st, int port) {
    auto *stream = static_cast<tlsuv_stream_t *>(calloc(1, sizeof(tlsuv_stream_t)));
    tlsuv_stream_init(st->loop, stream, st->client_tls);
    stream->data = st;
    st->client_stream = stream;

    auto *req = static_cast<uv_connect_t *>(calloc(1, sizeof(uv_connect_t)));
    req->data = st;
    int rc = tlsuv_stream_connect(req, stream, "localhost", port, on_client_connect);
    if (rc != 0) {
        free(req);
        finish(st, false, uv_strerror(rc));
    }
}

TEST_CASE("tls server/client", "[stream]") {
#if defined(_WIN32)
    WARN("tls server test not supported on Windows");
    SUCCEED();
#else
    UvLoopTest test;
    tls_pair_state st{};
    st.loop = test.loop;
    st.server_tls = default_tls_context(nullptr, 0);
    st.client_tls = testServerTLS();

    REQUIRE(st.server_tls != nullptr);
    REQUIRE(st.client_tls != nullptr);

    int rc = st.server_tls->load_key(&st.key, test_server_key, strlen(test_server_key));
    REQUIRE(rc == 0);
    rc = st.server_tls->load_cert(&st.cert, test_server_cert, strlen(test_server_cert));
    REQUIRE(rc == 0);
    rc = st.server_tls->set_own_cert(st.server_tls, st.key, st.cert);
    REQUIRE(rc == 0);

    st.listener = static_cast<uv_tcp_t *>(calloc(1, sizeof(uv_tcp_t)));
    uv_tcp_init(st.loop, st.listener);
    st.listener->data = &st;

    sockaddr_in addr{};
    rc = uv_ip4_addr("127.0.0.1", 0, &addr);
    REQUIRE(rc == 0);
    rc = uv_tcp_bind(st.listener, reinterpret_cast<const struct sockaddr *>(&addr), 0);
    REQUIRE(rc == 0);

    rc = uv_listen(reinterpret_cast<uv_stream_t *>(st.listener), 1, on_server_accept);
    REQUIRE(rc == 0);

    int namelen = sizeof(addr);
    rc = uv_tcp_getsockname(st.listener, reinterpret_cast<struct sockaddr *>(&addr), &namelen);
    REQUIRE(rc == 0);

    int port = ntohs(addr.sin_port);
    start_client(&st, port);

    test.run();

    CHECK(st.done);
    CHECK(st.success);
    if (!st.success) {
        FAIL(st.error);
    }

    if (st.server_tls) {
        st.server_tls->free_ctx(st.server_tls);
        st.server_tls = nullptr;
    }
    if (st.cert) {
        st.cert->free(st.cert);
        st.cert = nullptr;
    }
    if (st.key) {
        st.key->free(st.key);
        st.key = nullptr;
    }
#endif
}
