#include "telnet_server.h"

#include <lwip/ip_addr.h>
#include <lwip/tcp.h>
#include <stdio.h>

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

bool telnet_server_start(uint16_t port) {
  struct tcp_pcb *telnet_pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
  if (telnet_pcb == NULL) {
    printf("telnet: tcp_new failed\n");
    return false;
  }

  if (tcp_bind(telnet_pcb, IP_ANY_TYPE, port) != ERR_OK) {
    printf("telnet: tcp_bind failed\n");
    tcp_close(telnet_pcb);
    return false;
  }

  telnet_listener = tcp_listen_with_backlog(telnet_pcb, 2);
  if (telnet_listener == NULL) {
    printf("telnet: tcp_listen failed\n");
    tcp_close(telnet_pcb);
    return false;
  }

  tcp_accept(telnet_listener, telnet_accept_cb);
  printf("telnet: listening on port %u\n", port);
  return true;
}

void telnet_server_stop(void) {
  if (telnet_listener != NULL) {
    tcp_close(telnet_listener);
    telnet_listener = NULL;
  }
}
