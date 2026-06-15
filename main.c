#include <hardware/uart.h>
#include <lwip/apps/mdns.h>
#include <lwip/ip.h>
#include <lwip/tcp.h>
#include <pico/stdlib.h>
#include <stdio.h>

#include "dhcpserver/dhcpserver.h"
#include "usb_network.h"

// usb network addresses
static const ip4_addr_t ownip = IPADDR4_INIT_BYTES(192, 168, 7, 1);
static const ip4_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
static const ip4_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);

static struct tcp_pcb *http_listener;
static struct tcp_pcb *telnet_listener;

static char invert_case_char(char c) {
  if (c >= 'a' && c <= 'z') {
    return (char)(c - ('a' - 'A'));
  }
  if (c >= 'A' && c <= 'Z') {
    return (char)(c + ('a' - 'A'));
  }
  return c;
}

static err_t http_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  (void)arg;

  if (err != ERR_OK) {
    if (p != NULL) {
      pbuf_free(p);
    }
    tcp_close(tpcb);
    return err;
  }

  if (p == NULL) {
    tcp_close(tpcb);
    return ERR_OK;
  }

  static const char response[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Connection: close\r\n"
      "\r\n"
      "<html><body><h1>Hello, world!</h1></body></html>\r\n";

  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);

  err_t write_err = tcp_write(tpcb, response, sizeof(response) - 1, TCP_WRITE_FLAG_COPY);
  if (write_err == ERR_OK) {
    tcp_output(tpcb);
  }
  tcp_close(tpcb);
  return write_err;
}

static err_t http_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
  (void)arg;

  if (err != ERR_OK) {
    return err;
  }

  tcp_recv(newpcb, http_recv_cb);
  return ERR_OK;
}

static err_t telnet_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  (void)arg;

  if (err != ERR_OK) {
    if (p != NULL) {
      pbuf_free(p);
    }
    tcp_close(tpcb);
    return err;
  }

  if (p == NULL) {
    tcp_close(tpcb);
    return ERR_OK;
  }

  tcp_recved(tpcb, p->tot_len);

  for (struct pbuf *q = p; q != NULL; q = q->next) {
    uint16_t remaining = q->len;
    uint16_t offset = 0;

    while (remaining > 0) {
      uint16_t chunk = remaining > 128 ? 128 : remaining;
      char out[128];
      const char *in = (const char *)q->payload + offset;
      for (uint16_t i = 0; i < chunk; i++) {
        out[i] = invert_case_char(in[i]);
      }

      err_t write_err = tcp_write(tpcb, out, chunk, TCP_WRITE_FLAG_COPY);
      if (write_err != ERR_OK) {
        pbuf_free(p);
        return write_err;
      }

      offset = (uint16_t)(offset + chunk);
      remaining = (uint16_t)(remaining - chunk);
    }
  }

  pbuf_free(p);
  tcp_output(tpcb);
  return ERR_OK;
}

static err_t telnet_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
  (void)arg;

  if (err != ERR_OK) {
    return err;
  }

  tcp_recv(newpcb, telnet_recv_cb);
  return ERR_OK;
}

static bool start_tcp_servers(void) {
  struct tcp_pcb *http_pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
  if (http_pcb == NULL) {
    printf("http: tcp_new failed\n");
    return false;
  }
  if (tcp_bind(http_pcb, IP_ANY_TYPE, 80) != ERR_OK) {
    printf("http: tcp_bind failed\n");
    tcp_close(http_pcb);
    return false;
  }
  http_listener = tcp_listen_with_backlog(http_pcb, 2);
  if (http_listener == NULL) {
    printf("http: tcp_listen failed\n");
    tcp_close(http_pcb);
    return false;
  }
  tcp_accept(http_listener, http_accept_cb);

  struct tcp_pcb *telnet_pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
  if (telnet_pcb == NULL) {
    printf("telnet: tcp_new failed\n");
    tcp_close(http_listener);
    http_listener = NULL;
    return false;
  }
  if (tcp_bind(telnet_pcb, IP_ANY_TYPE, 23) != ERR_OK) {
    printf("telnet: tcp_bind failed\n");
    tcp_close(telnet_pcb);
    tcp_close(http_listener);
    http_listener = NULL;
    return false;
  }
  telnet_listener = tcp_listen_with_backlog(telnet_pcb, 2);
  if (telnet_listener == NULL) {
    printf("telnet: tcp_listen failed\n");
    tcp_close(telnet_pcb);
    tcp_close(http_listener);
    http_listener = NULL;
    return false;
  }
  tcp_accept(telnet_listener, telnet_accept_cb);

  return true;
}

static void stop_tcp_servers(void) {
  if (http_listener != NULL) {
    tcp_close(http_listener);
    http_listener = NULL;
  }
  if (telnet_listener != NULL) {
    tcp_close(telnet_listener);
    telnet_listener = NULL;
  }
}

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

  // start TCP services
  if (!start_tcp_servers()) {
    printf("failed to start TCP servers\n");
    mdns_resp_remove_netif(netif_default);
    dhcp_server_deinit(&dhcp_server);
    usb_network_deinit();
    return -1;
  }

  // enter main loop
  printf("setup complete, entering main loop\n");
  int key = 0;
  while ((key != 's') && (key != 'S')) {
    usb_network_update();
    key = getchar_timeout_us(0); // get any pending key press but don't wait
  }

  printf("shutting down\n");
  stop_tcp_servers();
  mdns_resp_remove_netif(netif_default);
  dhcp_server_deinit(&dhcp_server);
  usb_network_deinit();

  return 0;
}
