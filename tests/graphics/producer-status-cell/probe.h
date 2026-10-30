#ifndef CIDER_TRANSPORT_PROBE_H
#define CIDER_TRANSPORT_PROBE_H
#include <stdint.h>
struct transport_probe
{
    uint32_t size, version, roots, producers, watch_pins, hosts, pe_watches, cleanup_pins;
    uint64_t target_lease;
    uint32_t target_active, target_watched;
};
_Static_assert(sizeof(struct transport_probe)==48,"fixed probe width");
#endif
