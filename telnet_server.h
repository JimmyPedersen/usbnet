#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include <stdbool.h>
#include <stdint.h>

bool telnet_server_start(uint16_t port);
void telnet_server_stop(void);

#endif // TELNET_SERVER_H
