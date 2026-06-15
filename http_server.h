#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Start listening for incoming HTTP connections.
 *
 * Creates a TCP PCB, binds it to @p port, and registers the accept callback.
 * The server always responds with a static HTML page regardless of the request.
 *
 * @param port TCP port number to listen on (typically 80).
 * @return true  Listener started successfully.
 * @return false Failed to allocate or bind the TCP control block.
 */
bool http_server_start(uint16_t port);

/**
 * @brief Stop the HTTP listener and release its TCP resources.
 *
 * Safe to call even if the server was never started or has already been stopped.
 */
void http_server_stop(void);

#endif // HTTP_SERVER_H
