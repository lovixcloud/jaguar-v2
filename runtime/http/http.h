#ifndef JAG_HTTP_H
#define JAG_HTTP_H

#include "runtime/core/value.h"
#include <stdbool.h>

typedef struct {
    char *method;
    char *path;
    JagValue *params;
    JagValue *headers;
    char *body;
} HttpRequest;

typedef struct {
    int status_code;
    JagValue *headers;
    char *body;
} HttpResponse;

typedef void (*HttpRouteHandler)(HttpRequest *req, HttpResponse *res);

void jag_server_on(int port, JagValue *options);
void jag_server_route(const char *method, const char *path, HttpRouteHandler handler);
void jag_server_listen(void);
void jag_server_close(void);

JagValue *jag_http_get(const char *url);
JagValue *jag_http_post(const char *url, JagValue *data);

#endif
