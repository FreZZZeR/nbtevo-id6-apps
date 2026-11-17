/* nbtevo-id6-apps — Gauges worker test (Apache-2.0). No car, no QNX.
 *
 * Drives nd_read_signal against a fake gateway for real catalog signals (from the generated
 * ND_SIGS table), checking the decoded value, the plausibility gate, and the error paths.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "decode.h"
#include "dids.h"
#include "gauges.h"

static const nd_sig *find_sig(const char *key)
{
    size_t i;
    for (i = 0; i < ND_NSIGS; i++)
        if (strcmp(ND_SIGS[i].key, key) == 0)
            return &ND_SIGS[i];
    return NULL;
}

/* Fake gateway: coolant 40C, trans_temp 32C, rpm deliberately out of range; else NRC. */
static int fake_gw(void *ctx, unsigned char ecu, const unsigned char *req, int reqlen,
                   unsigned char *resp, int maxresp)
{
    (void)ctx; (void)maxresp;
    if (ecu == 0x12 && reqlen == 2 && req[0] == 0x01 && req[1] == 0x05) {      /* coolant 01 05 */
        resp[0] = 0x41; resp[1] = 0x05; resp[2] = 0x50;                         /* 0x50=80 -> 40 C */
        return 3;
    }
    if (ecu == 0x12 && reqlen == 2 && req[0] == 0x01 && req[1] == 0x0C) {      /* rpm 01 0C */
        resp[0] = 0x41; resp[1] = 0x0C; resp[2] = 0xFF; resp[3] = 0xFF;         /* 65535/4 -> 16383.75 */
        return 4;
    }
    if (ecu == 0x18 && reqlen == 3 && req[0] == 0x22 && req[1] == 0xDA && req[2] == 0x12) { /* trans_temp */
        resp[0] = 0x62; resp[1] = 0xDA; resp[2] = 0x12; resp[3] = 0x00; resp[4] = 0x50;    /* 80-48=32 C */
        return 5;
    }
    resp[0] = 0x7F; resp[1] = (reqlen ? req[0] : 0); resp[2] = 0x11;            /* serviceNotSupported */
    return 3;
}

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    const nd_sig *cool = find_sig("coolant");
    const nd_sig *rpm  = find_sig("rpm");
    const nd_sig *tt   = find_sig("trans_temp");
    const nd_sig *oil  = find_sig("oil_temp");
    nd_reading r;

    CHECK(cool && rpm && tt && oil);

    /* 1) OBD read + decode + plausible */
    CHECK(nd_read_signal(fake_gw, 0, cool, &r) == 1);
    CHECK(r.ok && fabs(r.value - 40.0) < 1e-9 && r.sig == cool);

    /* 2) UDS DID read + decode */
    CHECK(nd_read_signal(fake_gw, 0, tt, &r) == 1);
    CHECK(r.ok && fabs(r.value - 32.0) < 1e-9);

    /* 3) decodes but out of range -> value set, ok=0 (never report a wrong-but-plausible number) */
    CHECK(nd_read_signal(fake_gw, 0, rpm, &r) == 0);
    CHECK(r.ok == 0 && fabs(r.value - 16383.75) < 1e-9);

    /* 4) UDS negative response (unregistered PID) -> ok=0 */
    CHECK(nd_read_signal(fake_gw, 0, oil, &r) == 0 && r.ok == 0);

    /* 5) null signal guard */
    CHECK(nd_read_signal(fake_gw, 0, NULL, &r) == 0);

    /* 6) computed sport math (shared with signals.py) still available from decode core */
    CHECK(fabs(nd_boost_bar(250, 100) - 1.5) < 1e-9);
    CHECK(fabs(nd_torque_nm(500, 80) - 400.0) < 1e-9);

    if (fails) { printf("\n%d FAILURE(S)\n", fails); return 1; }
    printf("gauges OK: OBD + DID read/decode/plausibility over the transport seam\n");
    return 0;
}
