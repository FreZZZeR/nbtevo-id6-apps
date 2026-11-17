/* nbtevo-id6-apps — collector<->plugin frame (Apache-2.0).
 *
 * The one snapshot the collector PROCESS ships to the HMI plugin over loopback UDP. The
 * plugin (inside HmiMain3D) must never do network diagnostics itself — that would block the
 * UI dispatcher — so it only receives this frame and paints it. Encoding is explicit
 * little-endian so the contract is stable and host-testable (no raw struct over the wire).
 */
#ifndef ND_FRAME_H
#define ND_FRAME_H

#include <stdint.h>

/* Gauge slots the collector fills and the plugin paints — fixed order IS the wire contract. */
enum {
    NDG_COOLANT, NDG_OIL, NDG_IAT, NDG_BOOST, NDG_RPM, NDG_TORQUE, NDG_POWER, NDG_TRANS,
    ND_NGAUGE
};

typedef struct {
    uint32_t seq;                 /* increments per snapshot (caller-owned) */
    uint32_t valid;              /* 1 once the collector has produced a real frame */
    uint64_t sent_ms;            /* collector CLOCK_MONOTONIC at send (freshness) */
    double   gauge[ND_NGAUGE];   /* decoded values in catalog units (incl. computed boost/torque/power) */
    uint8_t  gauge_ok[ND_NGAUGE];/* 1 = read + plausible */
    uint16_t dtc_count;          /* stored DTCs on the engine module */
    uint8_t  dtc_active;         /* 1 = any confirmed/testFailed */
    uint8_t  xdrive_mode;        /* 0 = RWD, 1 = AWD, 0xFF = unknown */
    uint8_t  xdrive_variant_ok;  /* DSC variant is in the supported set */
} nd_frame;

#define ND_FRAME_WIRE (4 + 4 + 8 + ND_NGAUGE*8 + ND_NGAUGE*1 + 2 + 1 + 1 + 1)

/* Little-endian pack/unpack. encode returns bytes written (= ND_FRAME_WIRE) or -1 on a short
 * buffer; decode returns 1 or 0 (short buffer). */
int nd_frame_encode(const nd_frame *f, unsigned char *out, int n);
int nd_frame_decode(const unsigned char *in, int n, nd_frame *out);

/* 1 if a frame sent at sent_ms is still fresh at now_ms (age <= stale_ms). */
int nd_frame_fresh(uint64_t sent_ms, uint64_t now_ms, unsigned stale_ms);

#endif /* ND_FRAME_H */
