/* nbtevo-id6-apps — signal decode core (Apache-2.0).
 *
 * One affine decode shared with the Python catalog (nbtevo-diag signals.py, class Dec):
 *   value = raw * num / den + offset,  raw = big-endian int of `width` bytes (signed opt).
 * The table ND_SIGS in the generated dids.h is emitted from that same catalog, so the C
 * collector and the Python reader can never disagree. The on-car collector #includes this.
 */
#ifndef ND_DECODE_H
#define ND_DECODE_H

#include <stddef.h>

typedef struct {
    const char   *key;
    unsigned char ecu;      /* diagnostic target byte: DME 0x12, EGS 0x18, FEM 0x10, ... */
    unsigned char service;  /* 0x01 = OBD mode-01 PID, 0x22 = UDS ReadDataByIdentifier */
    unsigned short addr;    /* PID (OBD) or DID (UDS) */
    unsigned char width;    /* bytes to read: 1 or 2 */
    unsigned char is_signed;
    long          num;      /* value = raw * num / den + offset */
    long          den;
    double        offset;
    unsigned char has_lo; double lo;   /* plausibility bounds (0 = unbounded) */
    unsigned char has_hi; double hi;
} nd_sig;

/* Big-endian integer in the first `width` bytes (two's-complement if is_signed). width<=2. */
double nd_rawint(const unsigned char *d, int width, int is_signed);

/* Decode one signal's bytes to a number. Returns 1 and sets *out if len >= width, else 0.
 * Does NOT apply plausibility bounds (see nd_plausible) — this is the pure decode. */
int nd_value(const nd_sig *s, const unsigned char *d, int len, double *out);

/* 1 if v is within [lo,hi] (unbounded ends ignored). */
int nd_plausible(const nd_sig *s, double v);

/* Computed "sport displays" math (cross-signal; mirrors signals.py). */
double nd_boost_bar(double map_kpa, double baro_kpa);
double nd_torque_nm(double ref_nm, double actual_pct);
double nd_power_kw(double torque_nm, double rpm);

#endif /* ND_DECODE_H */
