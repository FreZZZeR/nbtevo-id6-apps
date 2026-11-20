/* nbtevo-id6-apps — live gauges worker (Apache-2.0). See gauges.h. */
#include "gauges.h"

int nd_read_signal(nd_uds_fn send, void *ctx, const nd_sig *s, nd_reading *out)
{
    unsigned char data[8];
    int len;

    out->sig = s;
    out->value = 0.0;
    out->ok = 0;
    if (s == 0)
        return 0;

    if (s->service == 0x2C)                /* dynamic DID: a direct 22 would be refused */
        len = nd_uds_read_dyn(send, ctx, s->ecu, s->addr, s->width, data, (int)sizeof data);
    else
        len = nd_uds_read(send, ctx, s->ecu, s->service, s->addr, data, (int)sizeof data);
    if (len < 0)
        return 0;                          /* transport error / NRC / echo mismatch */
    if (!nd_value(s, data, len, &out->value))
        return 0;                          /* short reply */
    out->ok = nd_plausible(s, out->value); /* decoded; report only if within bounds */
    return out->ok;
}
