/* nbtevo-id6-apps — fault-code (DTC) worker (Apache-2.0). See codes.h. */
#include "codes.h"

int nd_parse_dtcs(const unsigned char *resp, int len, nd_dtc *out, int max)
{
    int i, n = 0;

    if (resp == 0 || len < 3 || resp[0] != 0x59 || resp[1] != 0x02)
        return -1;  /* not a reportDTCByStatusMask positive response */

    /* body starts after `59 02 <statusAvailabilityMask>`; records are 4 bytes each.
     * i + 4 <= len keeps full records only, dropping any trailing partial (as dtc.py does). */
    for (i = 3; i + 4 <= len && n < max; i += 4) {
        out[n].code = ((unsigned long)resp[i]     << 16)
                    | ((unsigned long)resp[i + 1] <<  8)
                    |  (unsigned long)resp[i + 2];
        out[n].status = resp[i + 3];
        n++;
    }
    return n;
}

int nd_read_dtcs(nd_uds_fn send, void *ctx, unsigned char ecu, nd_dtc *out, int max)
{
    static const unsigned char req[3] = { 0x19, 0x02, 0xFF };
    unsigned char resp[1024];
    int len;

    if (send == 0)
        return -1;
    len = send(ctx, ecu, req, (int)sizeof req, resp, (int)sizeof resp);
    if (len <= 0 || resp[0] == 0x7F)   /* transport error, or UDS negative response */
        return -1;
    return nd_parse_dtcs(resp, len, out, max);
}
