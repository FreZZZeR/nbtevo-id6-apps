/* nbtevo-id6-apps — live gauges worker (Apache-2.0).
 *
 * Reads one catalog signal (an nd_sig from the generated dids.h table) over the transport
 * seam and decodes it with the shared decode core, gating on plausibility — mirror of
 * nbtevo-diag transport.read_signal. This is the "Gauges / Sport" app's collector; the
 * cross-signal sport math (boost/torque/power) is in decode.h.
 */
#ifndef ND_GAUGES_H
#define ND_GAUGES_H

#include "decode.h"   /* nd_sig, nd_value, nd_plausible */
#include "uds.h"      /* nd_uds_fn */

typedef struct {
    const nd_sig *sig;
    double        value;
    int           ok;   /* 1 = read + decoded + within plausibility bounds; 0 otherwise */
} nd_reading;

/* Read + decode one signal over `send`. Fills *out always (out->value set when it decodes).
 * Returns out->ok: 1 only when the value is present AND plausible — an implausible value is
 * reported with ok=0 (a wrong-but-plausible number is worse than "n/a"). */
int nd_read_signal(nd_uds_fn send, void *ctx, const nd_sig *s, nd_reading *out);

#endif /* ND_GAUGES_H */
