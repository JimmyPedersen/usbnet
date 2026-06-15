#ifndef IPERF_SERVER_H
#define IPERF_SERVER_H

#include <stdbool.h>
#include <stdint.h>

// Default port used by iperf2 clients.
#define IPERF_SERVER_PORT 5001

// Snapshot of latest TCP/UDP iperf session metrics.
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

/**
 * @brief Start both TCP and UDP iperf2-compatible listeners on @ref IPERF_SERVER_PORT.
 *
 * @return true  At least one listener (TCP or UDP) started successfully.
 * @return false Both TCP and UDP failed to start.
 */
bool iperf_server_start(void);

/**
 * @brief Stop all iperf listeners and finalize any active UDP session.
 */
void iperf_server_stop(void);

/**
 * @brief Obtain a snapshot of the latest iperf session metrics.
 *
 * The returned struct is a value copy; safe to read without additional
 * synchronization.
 *
 * @return Copy of the internal @ref iperf_server_status_t.
 */
iperf_server_status_t iperf_server_get_status(void);

/**
 * @brief Check whether at least one iperf listener is currently active.
 *
 * @return true  TCP or UDP (or both) listeners are running.
 * @return false No listeners are active.
 */
bool iperf_server_is_running(void);

/**
 * @brief Drive UDP session housekeeping; call from the main loop.
 *
 * Detects UDP sessions idle longer than @c UDP_SESSION_IDLE_TIMEOUT_MS and
 * finalizes them.
 */
void iperf_server_poll(void);

#endif // IPERF_SERVER_H
