/* nbtevo-id6-apps — UDS/OBD generic read (Apache-2.0). See uds.h. */
#include "uds.h"
#include <string.h>

int nd_uds_read(nd_uds_fn send, void *ctx, unsigned char ecu, unsigned char service,
                unsigned short addr, unsigned char *out, int maxout)
{
    unsigned char req[3];
    unsigned char resp[256];
    int reqlen, echo, len, i, datalen;

    if (send == 0)
        return -1;

    if (service == 0x01) {               /* OBD mode-01: request 01 <pid>, echo 41 <pid> */
        req[0] = 0x01;
        req[1] = (unsigned char)(addr & 0xFF);
        reqlen = 2;
        echo   = 2;
    } else {                              /* UDS 0x22: request 22 <hi> <lo>, echo 62 <hi> <lo> */
        req[0] = 0x22;
        req[1] = (unsigned char)(addr >> 8);
        req[2] = (unsigned char)(addr & 0xFF);
        reqlen = 3;
        echo   = 3;
    }

    len = send(ctx, ecu, req, reqlen, resp, (int)sizeof resp);
    if (len < echo)
        return -1;                        /* transport error, NRC, or truncated */
    if (resp[0] != (unsigned char)(req[0] + 0x40))
        return -1;                        /* not the matching positive response */
    for (i = 1; i < echo; i++)
        if (resp[i] != req[i])
            return -1;                    /* wrong PID/DID echoed back */

    datalen = len - echo;
    if (datalen > maxout)
        datalen = maxout;
    if (datalen > 0)
        memcpy(out, resp + echo, (size_t)datalen);
    return datalen;
}

int nd_uds_read_dyn(nd_uds_fn send, void *ctx, unsigned char ecu, unsigned short src,
                    unsigned char width, unsigned char *out, int maxout)
{
    const unsigned char dh = (unsigned char)(ND_DYN_DID >> 8);
    const unsigned char dl = (unsigned char)(ND_DYN_DID & 0xFF);
    unsigned char req[8];
    unsigned char resp[16];
    int len;

    if (send == 0)
        return -1;

    /* Clear first: a define on an already-defined DID may APPEND to it, and the read would then
     * start with the previous signal's bytes. The result is ignored — "nothing defined yet" is
     * an NRC on some ECUs; the define below is what must succeed. */
    req[0] = 0x2C; req[1] = 0x03; req[2] = dh; req[3] = dl;
    (void)send(ctx, ecu, req, 4, resp, (int)sizeof resp);

    /* 01 = defineByIdentifier (02 would be defineByMemoryAddress). Position is 1-based. */
    req[1] = 0x01;
    req[4] = (unsigned char)(src >> 8);
    req[5] = (unsigned char)(src & 0xFF);
    req[6] = 0x01;
    req[7] = width;
    len = send(ctx, ecu, req, 8, resp, (int)sizeof resp);
    if (len < 4 || resp[0] != 0x6C || resp[1] != 0x01 || resp[2] != dh || resp[3] != dl)
        return -1;                        /* define refused (NRC) or not our acknowledgement */

    /* ponytail: one signal per define is three round trips per read. For the gauge loop,
     * define every source once and poll 22 F303, slicing the reply by offset. */
    return nd_uds_read(send, ctx, ecu, 0x22, ND_DYN_DID, out, maxout);
}
