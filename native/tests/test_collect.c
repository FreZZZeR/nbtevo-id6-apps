/* nbtevo-id6-apps — collector-core test (Apache-2.0). No car, no QNX.
 *
 * Drives nd_collect against a fake gateway that answers the gauge PIDs/DIDs, a DTC read, and
 * the xDrive variant/mode, then checks the assembled frame — the whole workers->frame pipe.
 */
#include <stdio.h>
#include <math.h>

#include "collector.h"
#include "frame.h"
#include "fake_dde.h"

static int obd(unsigned char *resp, unsigned char pid, const unsigned char *data, int dn)
{ int i; resp[0] = 0x41; resp[1] = pid; for (i = 0; i < dn; i++) resp[2 + i] = data[i]; return 2 + dn; }

static int did(unsigned char *resp, unsigned char hi, unsigned char lo, const unsigned char *data, int dn)
{ int i; resp[0] = 0x62; resp[1] = hi; resp[2] = lo; for (i = 0; i < dn; i++) resp[3 + i] = data[i]; return 3 + dn; }

static int fake(void *ctx, unsigned char ecu, const unsigned char *req, int reqlen,
                unsigned char *resp, int maxresp)
{
    (void)ctx; (void)maxresp;
    if (ecu == 0x12 && reqlen == 2 && req[0] == 0x01) {
        unsigned char b;
        switch (req[1]) {
            case 0x05: b = 0x5A; return obd(resp, 0x05, &b, 1);                         /* coolant 90 -> 50 C */
            case 0x5C: b = 0x64; return obd(resp, 0x5C, &b, 1);                         /* oil 100 -> 60 C */
            case 0x0F: b = 0x3C; return obd(resp, 0x0F, &b, 1);                         /* iat 60 -> 20 C */
            case 0x0B: b = 0xE6; return obd(resp, 0x0B, &b, 1);                         /* map 230 kPa */
            case 0x33: b = 0x64; return obd(resp, 0x33, &b, 1);                         /* baro 100 kPa -> boost 1.30 */
            case 0x0C: { unsigned char d[2] = { 0x2E, 0xE0 }; return obd(resp, 0x0C, d, 2); } /* rpm 12000/4 = 3000 */
            case 0x63: { unsigned char d[2] = { 0x02, 0x58 }; return obd(resp, 0x63, d, 2); } /* ref_torque 600 */
            case 0x62: b = 0xAF; return obd(resp, 0x62, &b, 1);                         /* torque_pct 175-125 = 50% */
            default: break;
        }
    }
    if (ecu == 0x18 && reqlen == 3 && req[0] == 0x22 && req[1] == 0xDA && req[2] == 0x12) {
        unsigned char d[2] = { 0x00, 0x64 }; return did(resp, 0xDA, 0x12, d, 2);        /* trans 100-48 = 52 C */
    }
    if (ecu == 0x12 && reqlen == 3 && req[0] == 0x19 && req[1] == 0x02 && req[2] == 0xFF) {
        int i; unsigned char d[7] = { 0x59, 0x02, 0xFF, 0x10, 0x1A, 0x05, 0x08 };       /* one DTC, confirmed */
        for (i = 0; i < 7; i++) resp[i] = d[i]; return 7;
    }
    if (ecu == 0x29 && reqlen == 3 && req[0] == 0x22 && req[1] == 0xF1 && req[2] == 0x50) {
        unsigned char d[3] = { 0x0F, 0x25, 0x10 }; return did(resp, 0xF1, 0x50, d, 3);  /* variant 0x0F2510 */
    }
    if (ecu == 0x29 && reqlen == 3 && req[0] == 0x22 && req[1] == 0xDB && req[2] == 0xE1) {
        unsigned char d[1] = { 0x01 }; return did(resp, 0xDB, 0xE1, d, 1);              /* mode AWD */
    }
    resp[0] = 0x7F; resp[1] = (reqlen ? req[0] : 0); resp[2] = 0x11;
    return 3;
}

/* Our N57: engine values only from the DDE model (mode-01 dead), the other modules as before. */
static int n57_mode01;   /* how many mode-01 requests reached the engine */
static int fake_n57(void *ctx, unsigned char ecu, const unsigned char *req, int reqlen,
                    unsigned char *resp, int maxresp)
{
    if (ecu == 0x12 && reqlen > 0 && req[0] == 0x01)
        n57_mode01++;
    if (ecu == 0x12 && !(reqlen == 3 && req[0] == 0x19))
        return fake_dde(ctx, ecu, req, reqlen, resp, maxresp);
    return fake(ctx, ecu, req, reqlen, resp, maxresp);
}

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define NEAR(a, b) (fabs((a) - (b)) < 1e-3)

int main(void)
{
    nd_frame f;
    int nok = nd_collect(fake, 0, &f);

    CHECK(f.valid == 1);
    CHECK(nok == 8);
    CHECK(f.gauge_ok[NDG_COOLANT] && NEAR(f.gauge[NDG_COOLANT], 50.0));
    CHECK(f.gauge_ok[NDG_OIL]     && NEAR(f.gauge[NDG_OIL],     60.0));
    CHECK(f.gauge_ok[NDG_IAT]     && NEAR(f.gauge[NDG_IAT],     20.0));
    CHECK(f.gauge_ok[NDG_BOOST]   && NEAR(f.gauge[NDG_BOOST],   1.30));
    CHECK(f.gauge_ok[NDG_RPM]     && NEAR(f.gauge[NDG_RPM],     3000.0));
    CHECK(f.gauge_ok[NDG_TORQUE]  && NEAR(f.gauge[NDG_TORQUE],  300.0));
    CHECK(f.gauge_ok[NDG_POWER]   && NEAR(f.gauge[NDG_POWER],   94.2478));
    CHECK(f.gauge_ok[NDG_TRANS]   && NEAR(f.gauge[NDG_TRANS],   52.0));
    CHECK(f.dtc_count == 1 && f.dtc_active == 1);
    CHECK(f.xdrive_mode == 1 && f.xdrive_variant_ok == 1);

    /* no transport at all -> every read fails, fail-safe values */
    {
        nd_frame g;
        int n2 = nd_collect(0, 0, &g);
        CHECK(n2 == 0);
        CHECK(!g.gauge_ok[NDG_COOLANT] && !g.gauge_ok[NDG_BOOST] && !g.gauge_ok[NDG_POWER]);
        CHECK(g.dtc_count == 0 && g.dtc_active == 0);
        CHECK(g.xdrive_mode == 0xFF && g.xdrive_variant_ok == 0);
    }

    /* the N57: mode-01 is dead, so the same eight slots come from the DDE's own DIDs through
     * the dynamic DID. (The run above read nothing, so the engine family is probed afresh.) */
    {
        nd_frame n;
        CHECK(nd_collect(fake_n57, 0, &n) == 8);
        CHECK(n.gauge_ok[NDG_COOLANT] && NEAR(n.gauge[NDG_COOLANT], 50.0));
        CHECK(n.gauge_ok[NDG_OIL]     && NEAR(n.gauge[NDG_OIL],     60.0));
        CHECK(n.gauge_ok[NDG_IAT]     && NEAR(n.gauge[NDG_IAT],     19.96));
        CHECK(n.gauge_ok[NDG_BOOST]   && NEAR(n.gauge[NDG_BOOST],   1.30));      /* (2300.02-1000.01) hPa */
        CHECK(n.gauge_ok[NDG_RPM]     && NEAR(n.gauge[NDG_RPM],     3000.0));
        CHECK(n.gauge_ok[NDG_TORQUE]  && NEAR(n.gauge[NDG_TORQUE],  299.9624));
        CHECK(n.gauge_ok[NDG_POWER]   && NEAR(n.gauge[NDG_POWER],   94.2360));
        CHECK(n.gauge_ok[NDG_TRANS]   && NEAR(n.gauge[NDG_TRANS],   52.0));
        CHECK(n.dtc_count == 1);
        /* the family is cached: the next tick asks mode-01 nothing (on the car that is a timeout) */
        CHECK(n57_mode01 == 1);
        CHECK(nd_collect(fake_n57, 0, &n) == 8 && NEAR(n.gauge[NDG_RPM], 3000.0));
        CHECK(n57_mode01 == 1);
    }

    if (fails) { printf("\n%d FAILURE(S)\n", fails); return 1; }
    printf("collect OK: workers -> frame (gauges + computed, DTC, xDrive) vs fake gateway, mode-01 and N57 DDE\n");
    return 0;
}
