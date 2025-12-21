#include "mod_proto.h"
#include <stdint.h>
#include "esp_log.h"

static const char *TAG = "MOD_PROTO_METRICS";

/* Atomic counters */
static uint32_t s_malformed_artnet = 0;
static uint32_t s_malformed_sacn = 0;
static uint32_t s_socket_errors = 0;
static uint32_t s_igmp_failures = 0;

void mod_proto_metrics_inc_malformed_artnet(void)
{
    __atomic_add_fetch(&s_malformed_artnet, 1, __ATOMIC_SEQ_CST);
}

void mod_proto_metrics_inc_malformed_sacn(void)
{
    __atomic_add_fetch(&s_malformed_sacn, 1, __ATOMIC_SEQ_CST);
}

void mod_proto_metrics_inc_socket_error(void)
{
    __atomic_add_fetch(&s_socket_errors, 1, __ATOMIC_SEQ_CST);
}

void mod_proto_metrics_inc_igmp_failure(void)
{
    __atomic_add_fetch(&s_igmp_failures, 1, __ATOMIC_SEQ_CST);
}

void mod_proto_get_metrics(mod_proto_metrics_t *out)
{
    if (!out) return;
    out->malformed_artnet_packets = __atomic_load_n(&s_malformed_artnet, __ATOMIC_SEQ_CST);
    out->malformed_sacn_packets = __atomic_load_n(&s_malformed_sacn, __ATOMIC_SEQ_CST);
    out->socket_errors = __atomic_load_n(&s_socket_errors, __ATOMIC_SEQ_CST);
    out->igmp_failures = __atomic_load_n(&s_igmp_failures, __ATOMIC_SEQ_CST);
}
