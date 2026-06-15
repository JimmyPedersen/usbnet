// Minimal HTTP server that always returns a static page.
#include "http_server.h"

#include <lwip/ip_addr.h>
#include <lwip/tcp.h>
#include <stdio.h>

static struct tcp_pcb *http_listener;

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

  // Keep the response static to avoid allocations in the RX callback path.
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

bool http_server_start(uint16_t port) {
  struct tcp_pcb *http_pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
  if (http_pcb == NULL) {
    printf("http: tcp_new failed\n");
    return false;
  }

  if (tcp_bind(http_pcb, IP_ANY_TYPE, port) != ERR_OK) {
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
  printf("http: listening on port %u\n", port);
  return true;
}

void http_server_stop(void) {
  if (http_listener != NULL) {
    tcp_close(http_listener);
    http_listener = NULL;
  }
}
