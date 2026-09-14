#ifndef JAG_WS_H
#define JAG_WS_H

#include "runtime/core/value.h"

typedef struct JagSocket JagSocket;
typedef void (*WsMessageHandler)(JagSocket *ws, const char *msg);

struct JagSocket {
    int fd;
    WsMessageHandler on_message;
};

void jag_socket_on(int port);
void jag_socket_route(const char *path, WsMessageHandler handler);
void jag_socket_listen(void);

#endif
