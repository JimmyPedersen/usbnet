#include "iperf_server.h"

#include <lwip/apps/lwiperf.h>
#include <lwip/def.h>
#include <lwip/ip_addr.h>
#include <lwip/sys.h>
#include <lwip/udp.h>
#include <string.h>
#include <stdio.h>

static void *iperf_session;
static iperf_server_status_t iperf_status;
static bool iperf_tcp_running;
static bool iperf_udp_running;
static struct udp_pcb *iperf_udp_pcb;

static bool udp_session_active;
static ip_addr_t udp_remote_addr;
static uint16_t udp_remote_port;
static uint32_t udp_session_start_ms;
static uint32_t udp_last_packet_ms;
static uint32_t udp_session_packets;
static uint32_t udp_session_bytes;
static uint32_t udp_max_seq;
static uint32_t udp_out_of_order;
static bool udp_seq_initialized;

#define UDP_SESSION_IDLE_TIMEOUT_MS 1500
#define IPERF2_HEADER_VERSION1 0x80000000UL

typedef struct {
  uint32_t words[26];
} iperf2_udp_ack_t;

static const char *report_type_str(enum lwiperf_report_type report_type) {
  switch (report_type) {
  case LWIPERF_TCP_DONE_SERVER:
    return "DONE_SERVER";
  case LWIPERF_TCP_DONE_CLIENT:
    return "DONE_CLIENT";
  case LWIPERF_TCP_ABORTED_LOCAL:
    return "ABORTED_LOCAL";
  case LWIPERF_TCP_ABORTED_LOCAL_DATAERROR:
    return "ABORTED_LOCAL_DATAERROR";
  case LWIPERF_TCP_ABORTED_LOCAL_TXERROR:
    return "ABORTED_LOCAL_TXERROR";
  case LWIPERF_TCP_ABORTED_REMOTE:
    return "ABORTED_REMOTE";
  default:
    return "UNKNOWN";
  }
}

static void iperf_report_cb(void *arg, enum lwiperf_report_type report_type, const ip_addr_t *local_addr, u16_t local_port,
                            const ip_addr_t *remote_addr, u16_t remote_port, u32_t bytes_transferred, u32_t ms_duration,
                            u32_t bandwidth_kbitpsec) {
  (void)arg;
  (void)local_addr;
  (void)remote_addr;

  iperf_status.total_sessions++;
  iperf_status.bytes_transferred = bytes_transferred;
  iperf_status.duration_ms = ms_duration;
  iperf_status.bandwidth_kbitpsec = bandwidth_kbitpsec;
  iperf_status.local_port = local_port;
  iperf_status.remote_port = remote_port;
  iperf_status.has_report = true;

  printf("iperf: %s local_port=%u remote_port=%u bytes=%lu duration_ms=%lu bw_kbps=%lu\n", report_type_str(report_type),
         local_port, remote_port, (unsigned long)bytes_transferred, (unsigned long)ms_duration,
         (unsigned long)bandwidth_kbitpsec);
}

static void iperf_udp_finalize_session(const char *reason, uint32_t now_ms) {
  if (!udp_session_active) {
    return;
  }

  uint32_t duration_ms = now_ms - udp_session_start_ms;
  if (duration_ms == 0) {
    duration_ms = 1;
  }
  uint32_t bandwidth_kbitpsec = (uint32_t)(((uint64_t)udp_session_bytes * 8U) / duration_ms);

  iperf_status.has_udp_report = true;
  iperf_status.udp_active = false;
  iperf_status.udp_packets = udp_session_packets;
  iperf_status.udp_bytes = udp_session_bytes;
  iperf_status.udp_duration_ms = duration_ms;
  iperf_status.udp_bandwidth_kbitpsec = bandwidth_kbitpsec;
  iperf_status.udp_remote_port = udp_remote_port;

  printf("iperf-udp: %s remote_port=%u packets=%lu bytes=%lu duration_ms=%lu bw_kbps=%lu\n", reason, udp_remote_port,
         (unsigned long)udp_session_packets, (unsigned long)udp_session_bytes, (unsigned long)duration_ms,
         (unsigned long)bandwidth_kbitpsec);

  udp_session_active = false;
}

static void iperf_udp_send_ackfin(const ip_addr_t *addr, uint16_t port, uint32_t last_seq, uint32_t now_ms) {
  iperf2_udp_ack_t ack;
  memset(&ack, 0, sizeof(ack));

  uint32_t duration_ms = now_ms - udp_session_start_ms;
  if (duration_ms == 0) {
    duration_ms = 1;
  }

  uint32_t error_cnt = 0;
  if (udp_seq_initialized && (udp_max_seq + 1U > udp_session_packets)) {
    error_cnt = (udp_max_seq + 1U) - udp_session_packets;
  }

  uint32_t stop_sec = duration_ms / 1000U;
  uint32_t stop_usec = (duration_ms % 1000U) * 1000U;

  // UDP_datagram words expected before server_hdr.
  ack.words[0] = lwip_htonl(last_seq | 0x80000000UL); // id
  ack.words[1] = 0; // tv_sec
  ack.words[2] = 0; // tv_usec
  ack.words[3] = 0; // id2

  // server_hdr_v1 words.
  ack.words[4] = lwip_htonl(IPERF2_HEADER_VERSION1); // flags
  ack.words[5] = 0; // total_len1
  ack.words[6] = lwip_htonl(udp_session_bytes); // total_len2
  ack.words[7] = lwip_htonl(stop_sec);
  ack.words[8] = lwip_htonl(stop_usec);
  ack.words[9] = lwip_htonl(error_cnt);
  ack.words[10] = lwip_htonl(udp_out_of_order);
  ack.words[11] = lwip_htonl(udp_session_packets); // datagrams
  ack.words[12] = 0; // datagrams2 when seqno64 is enabled on client
  ack.words[13] = 0; // jitter1
  ack.words[14] = 0; // jitter2

  struct pbuf *resp = pbuf_alloc(PBUF_TRANSPORT, sizeof(ack), PBUF_RAM);
  if (resp == NULL) {
    printf("iperf-udp: AckFIN alloc failed\n");
    return;
  }

  memcpy(resp->payload, &ack, sizeof(ack));
  err_t send_err = udp_sendto(iperf_udp_pcb, resp, addr, port);
  pbuf_free(resp);
  if (send_err != ERR_OK) {
    printf("iperf-udp: AckFIN send failed err=%d\n", (int)send_err);
  }
}

static void iperf_udp_recv_cb(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port) {
  (void)arg;
  (void)upcb;

  if (p == NULL) {
    return;
  }

  uint32_t now_ms = sys_now();
  bool new_session = !udp_session_active || (port != udp_remote_port) || !ip_addr_cmp(addr, &udp_remote_addr);
  if (new_session) {
    if (udp_session_active) {
      iperf_udp_finalize_session("switch-peer", now_ms);
    }

    udp_session_active = true;
    udp_remote_addr = *addr;
    udp_remote_port = port;
    udp_session_start_ms = now_ms;
    udp_last_packet_ms = now_ms;
    udp_session_packets = 0;
    udp_session_bytes = 0;
    udp_max_seq = 0;
    udp_out_of_order = 0;
    udp_seq_initialized = false;
    iperf_status.udp_total_sessions++;
    iperf_status.udp_active = true;
  }

  udp_last_packet_ms = now_ms;
  udp_session_bytes += p->tot_len;

  if (p->tot_len >= sizeof(uint32_t)) {
    uint32_t seq_raw = 0;
    pbuf_copy_partial(p, &seq_raw, sizeof(seq_raw), 0);
    seq_raw = lwip_ntohl(seq_raw);
    bool is_final = (seq_raw & 0x80000000UL) != 0;
    uint32_t seq = seq_raw & 0x7FFFFFFFUL;

    if (is_final) {
      iperf_udp_send_ackfin(addr, port, seq, now_ms);
      iperf_udp_finalize_session("end", now_ms);
    } else {
      udp_session_packets++;
      if (!udp_seq_initialized) {
        udp_seq_initialized = true;
        udp_max_seq = seq;
      } else if (seq > udp_max_seq) {
        udp_max_seq = seq;
      } else {
        udp_out_of_order++;
      }
    }
  } else {
    udp_session_packets++;
  }

  pbuf_free(p);
}

bool iperf_server_start(void) {
  iperf_status = (iperf_server_status_t){0};
  iperf_tcp_running = false;
  iperf_udp_running = false;
  iperf_udp_pcb = NULL;
  udp_session_active = false;

  iperf_session = lwiperf_start_tcp_server(IP_ANY_TYPE, IPERF_SERVER_PORT, iperf_report_cb, NULL);
  if (iperf_session == NULL) {
    printf("iperf: tcp start failed\n");
  } else {
    iperf_tcp_running = true;
    printf("iperf: tcp listening on port %u\n", IPERF_SERVER_PORT);
  }

  iperf_udp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (iperf_udp_pcb == NULL) {
    printf("iperf: udp start failed (udp_new)\n");
  } else if (udp_bind(iperf_udp_pcb, IP_ANY_TYPE, IPERF_SERVER_PORT) != ERR_OK) {
    printf("iperf: udp start failed (udp_bind)\n");
    udp_remove(iperf_udp_pcb);
    iperf_udp_pcb = NULL;
  } else {
    udp_recv(iperf_udp_pcb, iperf_udp_recv_cb, NULL);
    iperf_udp_running = true;
    printf("iperf: udp listening on port %u\n", IPERF_SERVER_PORT);
  }

  if (iperf_tcp_running || iperf_udp_running) {
    printf("iperf: protocol is iperf2-compatible (use iperf2 client, not iperf3)\n");
    return true;
  }

  printf("iperf: no listeners started\n");
  return false;
}

void iperf_server_stop(void) {
  if (udp_session_active) {
    iperf_udp_finalize_session("stop", sys_now());
  }

  if (iperf_session != NULL) {
    lwiperf_abort(iperf_session);
    iperf_session = NULL;
  }
  iperf_tcp_running = false;

  if (iperf_udp_pcb != NULL) {
    udp_remove(iperf_udp_pcb);
    iperf_udp_pcb = NULL;
  }
  iperf_udp_running = false;
}

iperf_server_status_t iperf_server_get_status(void) {
  return iperf_status;
}

bool iperf_server_is_running(void) {
  return iperf_tcp_running || iperf_udp_running;
}

void iperf_server_poll(void) {
  if (!udp_session_active) {
    return;
  }

  uint32_t now_ms = sys_now();
  if ((now_ms - udp_last_packet_ms) >= UDP_SESSION_IDLE_TIMEOUT_MS) {
    iperf_udp_finalize_session("timeout", now_ms);
  }
}
