#include <uv.h>
#include <tlsuv/um_http.h>
#include <tlsuv/uv_mbed.h>

static struct opts {
    int keepalive;
    int timeout;
    int ping_count;
    int delay;
} opts = {
        .keepalive = 10000,
        .timeout = 1000,
        .delay = 2000,
        .ping_count = 3,
};

static void on_response(um_http_resp_t *resp, void* ctx) {
    printf("%d %s\n", resp->code, resp->status);
}

static void on_clt_close(um_http_t* http) {
    printf("HTTP is closed\n");
}

static void do_request(uv_timer_t *t) {
    um_http_t *http = t->data;

    um_http_req(http, "GET", "/json", on_response, NULL);
    if (--opts.ping_count <= 0) {
        uv_close((uv_handle_t *) t, NULL);
        um_http_close(http, on_clt_close);
    }
}

void logger(int level, const char *file, unsigned int line, const char *msg) {

    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    fprintf(stderr, "[%9ld.%03ld] %s:%d %s\n", spec.tv_sec, spec.tv_nsec/1000000, file, line, msg);
}

int main(int argc, char *argv[]) {
    uv_mbed_set_debug(6, logger);
    uv_loop_t *l = uv_default_loop();
    um_http_t http;
    um_http_init(l, &http, "https://httpbin.org");
    um_http_idle_keepalive(&http, opts.keepalive);
    um_http_connect_timeout(&http, opts.timeout);

    uv_timer_t timer;
    uv_timer_init(l, &timer);
    timer.data = &http;

    uv_timer_start(&timer, do_request, 0, opts.delay);
    uv_run(l, UV_RUN_DEFAULT);
}
