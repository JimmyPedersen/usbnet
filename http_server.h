#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>
#include <stdint.h>

// Start listening for HTTP requests on the given TCP port.
bool http_server_start(uint16_t port);
// Stop the HTTP listener if running.
void http_server_stop(void);

#endif // HTTP_SERVER_H
