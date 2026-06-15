#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include <stdbool.h>
#include <stdint.h>

// Start listening for telnet-style TCP traffic on the given port.
bool telnet_server_start(uint16_t port);
// Stop the telnet listener if running.
void telnet_server_stop(void);

#endif // TELNET_SERVER_H
