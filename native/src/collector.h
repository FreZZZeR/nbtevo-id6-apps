/* nbtevo-id6-apps — collector core (Apache-2.0).
 *
 * Drives the Codes/Gauges/xDrive workers over the transport seam and fills one frame. This
 * is the logic the collector PROCESS runs each tick (collector_main.c adds the HSFZ socket +
 * the loopback-UDP send). Pure except for the `send` calls, so a fake makes it host-testable.
 */
#ifndef ND_COLLECTOR_H
#define ND_COLLECTOR_H

#include "uds.h"     /* nd_uds_fn */
#include "frame.h"

/* Read every gauge (incl. computed boost/torque/power), the engine DTC summary, and the
 * xDrive mode/variant over `send`, into *out (sets out->valid = 1; leaves seq/sent_ms to the
 * caller). Returns the count of gauges read ok. */
int nd_collect(nd_uds_fn send, void *ctx, nd_frame *out);

#endif /* ND_COLLECTOR_H */
