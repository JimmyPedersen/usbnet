#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Start the telnet echo server on the given TCP port.
 *
 * Accepts connections and echoes back each character with its alphabetic case
 * inverted (e.g. 'a' → 'A', 'Z' → 'z').
 *
 * @param port TCP port number to listen on (typically 23).
 * @return true  Listener started successfully.
 * @return false Failed to allocate or bind the TCP control block.
 */
bool telnet_server_start(uint16_t port);

/**
 * @brief Stop the telnet listener and release its TCP resources.
 *
 * Safe to call even if the server was never started or has already been stopped.
 */
void telnet_server_stop(void);

#endif // TELNET_SERVER_H
