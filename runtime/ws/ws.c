#include "ws.h"
#include <stdio.h>
#include <stdlib.h>

static int g_ws_port = 0;

void jag_socket_on(int port) {
    g_ws_port = port;
    (void)g_ws_port;
}

void jag_socket_route(const char *path, WsMessageHandler handler) {
    (void)path; (void)handler;
}

void jag_socket_listen(void) {
}
