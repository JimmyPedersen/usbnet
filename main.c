#include <hardware/uart.h>
#include <lwip/apps/mdns.h>
#include <lwip/ip.h>
#include <pico/stdlib.h>
#include <stdio.h>

#include "dhcpserver/dhcpserver.h"
#include "http_server.h"
#include "iperf_server.h"
#include "telnet_server.h"
#include "usb_network.h"

// usb network addresses
static const ip4_addr_t ownip = IPADDR4_INIT_BYTES(192, 168, 7, 1);
static const ip4_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
static const ip4_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);

#define HTTP_PORT 80
#define TELNET_PORT 23
#define STATUS_INTERVAL_MS 5000

int main() {
  stdio_uart_init();

  // setup USB network
  if (!usb_network_init(&ownip, &netmask, &gateway, true)) {
    printf("failed to start usb network\n");
    return -1;
  }

  // setup DHCP server
  dhcp_server_t dhcp_server;
  dhcp_server_init(&dhcp_server, (ip_addr_t *)&ownip, (ip_addr_t *)&netmask, false);

  // enable mDNS
  mdns_resp_init();
  mdns_resp_add_netif(netif_default, "demo");

  // start network services
  if (!http_server_start(HTTP_PORT)) {
    printf("failed to start HTTP server\n");
    mdns_resp_remove_netif(netif_default);
    dhcp_server_deinit(&dhcp_server);
    usb_network_deinit();
    return -1;
  }
  if (!telnet_server_start(TELNET_PORT)) {
    printf("failed to start telnet server\n");
    http_server_stop();
    mdns_resp_remove_netif(netif_default);
    dhcp_server_deinit(&dhcp_server);
    usb_network_deinit();
    return -1;
  }
  if (!iperf_server_start()) {
    printf("warning: iperf disabled, network services continue\n");
  }

  // enter main loop
  printf("setup complete, entering main loop\n");
  printf("http:  http://192.168.7.1:%u\n", HTTP_PORT);
  printf("telnet: 192.168.7.1:%u\n", TELNET_PORT);
  printf("iperf: %s 192.168.7.1:%u\n", iperf_server_is_running() ? "enabled" : "disabled", IPERF_SERVER_PORT);
  printf("status: every %u ms on UART\n", STATUS_INTERVAL_MS);

  int key = 0;
  uint32_t next_status_ms = to_ms_since_boot(get_absolute_time()) + STATUS_INTERVAL_MS;
  while ((key != 's') && (key != 'S')) {
    usb_network_update();
    iperf_server_poll();

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if ((int32_t)(now_ms - next_status_ms) >= 0) {
      iperf_server_status_t iperf_status = iperf_server_get_status();
      printf("status: up_ms=%lu usb=%s http=%u telnet=%u iperf=%s:%u tcp_sessions=%lu udp_sessions=%lu", (unsigned long)now_ms,
             usb_network_is_up() ? "up" : "down", HTTP_PORT, TELNET_PORT, iperf_server_is_running() ? "on" : "off",
             IPERF_SERVER_PORT, (unsigned long)iperf_status.total_sessions, (unsigned long)iperf_status.udp_total_sessions);
      if (iperf_status.has_report) {
        printf(" tcp_last_bw_kbps=%lu tcp_last_bytes=%lu tcp_last_ms=%lu", (unsigned long)iperf_status.bandwidth_kbitpsec,
               (unsigned long)iperf_status.bytes_transferred, (unsigned long)iperf_status.duration_ms);
      }
      if (iperf_status.udp_active) {
        printf(" udp_active=1");
      }
      if (iperf_status.has_udp_report) {
        printf(" udp_last_bw_kbps=%lu udp_last_bytes=%lu udp_last_ms=%lu", (unsigned long)iperf_status.udp_bandwidth_kbitpsec,
               (unsigned long)iperf_status.udp_bytes, (unsigned long)iperf_status.udp_duration_ms);
      }
      printf("\n");

      next_status_ms += STATUS_INTERVAL_MS;
    }

    key = getchar_timeout_us(0); // get any pending key press but don't wait
  }

  printf("shutting down\n");
  iperf_server_stop();
  telnet_server_stop();
  http_server_stop();
  mdns_resp_remove_netif(netif_default);
  dhcp_server_deinit(&dhcp_server);
  usb_network_deinit();

  return 0;
}
