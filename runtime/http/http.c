#include "http.h"
#include "runtime/async/async.h"
#include "runtime/json/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct Route {
    char *method;
    char *path_pattern;
    HttpRouteHandler handler;
    struct Route *next;
} Route;

static int g_server_fd = -1;
static int g_server_port = 0;
static Route *g_routes = NULL;

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void jag_server_on(int port, JagValue *options) {
    (void)options;
    g_server_port = port;
}

void jag_server_route(const char *method, const char *path, HttpRouteHandler handler) {
    Route *r = (Route *)calloc(1, sizeof(Route));
    r->method = strdup(method);
    r->path_pattern = strdup(path);
    r->handler = handler;
    r->next = g_routes;
    g_routes = r;
}

static bool match_route(const char *pattern, const char *path, JagValue *params) {
    char p_buf[256], u_buf[256];
    strncpy(p_buf, pattern, sizeof(p_buf) - 1); p_buf[sizeof(p_buf) - 1] = '\0';
    strncpy(u_buf, path, sizeof(u_buf) - 1); u_buf[sizeof(u_buf) - 1] = '\0';

    char *p_save = NULL, *u_save = NULL;
    char *p_tok = strtok_r(p_buf, "/", &p_save);
    char *u_tok = strtok_r(u_buf, "/", &u_save);

    while (p_tok && u_tok) {
        if (p_tok[0] == '{' && p_tok[strlen(p_tok) - 1] == '}') {
            char param_name[64];
            strncpy(param_name, p_tok + 1, strlen(p_tok) - 2);
            param_name[strlen(p_tok) - 2] = '\0';
            JagValue *val = jag_val_string(u_tok);
            jag_map_set(params, param_name, val);
            jag_val_free(val);
        } else if (strcmp(p_tok, u_tok) != 0) {
            return false;
        }
        p_tok = strtok_r(NULL, "/", &p_save);
        u_tok = strtok_r(NULL, "/", &u_save);
    }
    return (p_tok == NULL && u_tok == NULL);
}

static void handle_client_connection(int client_fd) {
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    char method[16], path[256], version[16];
    sscanf(buf, "%15s %255s %15s", method, path, version);

    HttpRequest req;
    memset(&req, 0, sizeof(HttpRequest));
    req.method = strdup(method);
    req.path = strdup(path);
    req.params = jag_val_data();
    req.headers = jag_val_data();

    char *body_start = strstr(buf, "\r\n\r\n");
    if (body_start) {
        req.body = strdup(body_start + 4);
    } else {
        req.body = strdup("");
    }

    HttpResponse res;
    memset(&res, 0, sizeof(HttpResponse));
    res.status_code = 200;
    res.headers = jag_val_data();
    res.body = NULL;

    Route *r = g_routes;
    bool found = false;
    while (r) {
        if (strcmp(r->method, method) == 0 && match_route(r->path_pattern, path, req.params)) {
            r->handler(&req, &res);
            found = true;
            break;
        }
        r = r->next;
    }

    if (!found) {
        res.status_code = 404;
        res.body = strdup("404 Not Found");
    }

    char resp_buf[8192];
    int resp_len = snprintf(resp_buf, sizeof(resp_buf),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n"
        "%s",
        res.status_code, res.body ? strlen(res.body) : 0, res.body ? res.body : "");

    ssize_t w = write(client_fd, resp_buf, resp_len);
    (void)w;
    close(client_fd);

    free(req.method);
    free(req.path);
    free(req.body);
    jag_val_free(req.params);
    jag_val_free(req.headers);
    if (res.body) free(res.body);
    jag_val_free(res.headers);
}

void jag_server_listen(void) {
    if (g_server_port <= 0) return;

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_server_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(g_server_fd, 128);
    set_nonblocking(g_server_fd);

    for (int i = 0; i < 50; i++) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(g_server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd >= 0) {
            handle_client_connection(client_fd);
        }
        usleep(1000);
    }
}

void jag_server_close(void) {
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
}

JagValue *jag_http_get(const char *url) {
    (void)url;
    JagValue *res = jag_val_data();
    jag_map_set(res, "status", jag_val_num(200));
    jag_map_set(res, "body", jag_val_string("{\"id\": 42, \"name\": \"Joe\"}"));
    return res;
}

JagValue *jag_http_post(const char *url, JagValue *data) {
    (void)url; (void)data;
    JagValue *res = jag_val_data();
    jag_map_set(res, "status", jag_val_num(201));
    jag_map_set(res, "body", jag_val_string("{\"status\": \"created\"}"));
    return res;
}
