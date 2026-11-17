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
