#ifndef IPERF_SERVER_H
#define IPERF_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#define IPERF_SERVER_PORT 5001

typedef struct {
	uint32_t total_sessions;
	uint32_t bytes_transferred;
	uint32_t duration_ms;
	uint32_t bandwidth_kbitpsec;
	uint16_t local_port;
	uint16_t remote_port;
	bool has_report;
	uint32_t udp_total_sessions;
	uint32_t udp_packets;
	uint32_t udp_bytes;
	uint32_t udp_duration_ms;
	uint32_t udp_bandwidth_kbitpsec;
	uint16_t udp_remote_port;
	bool udp_active;
	bool has_udp_report;
} iperf_server_status_t;

bool iperf_server_start(void);
void iperf_server_stop(void);
iperf_server_status_t iperf_server_get_status(void);
bool iperf_server_is_running(void);
void iperf_server_poll(void);

#endif // IPERF_SERVER_H
