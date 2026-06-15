#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>
#include <stdint.h>

bool http_server_start(uint16_t port);
void http_server_stop(void);

#endif // HTTP_SERVER_H
