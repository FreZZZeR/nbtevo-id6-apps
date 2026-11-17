/* nbtevo-id6-apps — xDrive worker test (Apache-2.0). No car, no QNX.
 *
 * Two layers, both pure/offline:
 *   1. the software fuse (nd_xdrive_interlock) — mirrors control.selftest() case-for-case;
 *   2. the full set-mode flow against a stateful fake DSC/EGS/DME (variant gate, interlock,
 *      dry-run vs commit, 2E DBE1 write, read-back) — the mode actually flips in the fake.
 */
#include <stdio.h>
#include "xdrive.h"

/* A fake car the fake transport answers from; the write mutates car->mode. */
typedef struct {
    unsigned long variant;
    int mode;        /* ND_XDRIVE_RWD / ND_XDRIVE_AWD */
    int speed;       /* km/h */
    int park;        /* 1 = in Park */
    int engine_on;   /* 1 = running */
} fake_car;

static int fake_dsc(void *ctx, unsigned char ecu, const unsigned char *req, int reqlen,
                    unsigned char *resp, int maxresp)
{
    fake_car *car = (fake_car *)ctx;
    (void)maxresp;

    if (ecu == 0x29 && reqlen == 3 && req[0] == 0x22 && req[1] == 0xF1 && req[2] == 0x50) {
        resp[0] = 0x62; resp[1] = 0xF1; resp[2] = 0x50;
        resp[3] = (unsigned char)(car->variant >> 16);
        resp[4] = (unsigned char)(car->variant >> 8);
        resp[5] = (unsigned char)car->variant;
        return 6;
    }
    if (ecu == 0x29 && reqlen == 3 && req[0] == 0x22 && req[1] == 0xDB && req[2] == 0xE1) {
        resp[0] = 0x62; resp[1] = 0xDB; resp[2] = 0xE1; resp[3] = (unsigned char)car->mode;
        return 4;
    }
    if (ecu == 0x29 && reqlen == 4 && req[0] == 0x2E && req[1] == 0xDB && req[2] == 0xE1) {
        car->mode = req[3];                              /* the write takes effect */
        resp[0] = 0x6E; resp[1] = 0xDB; resp[2] = 0xE1;
        return 3;
    }
    if (ecu == 0x18 && reqlen == 3 && req[0] == 0x22 && req[1] == 0xDA && req[2] == 0x2E) {
        resp[0] = 0x62; resp[1] = 0xDA; resp[2] = 0x2E;
        resp[3] = (unsigned char)(car->park ? 0x00 : 0x05);   /* 00 = Park */
        return 4;
    }
    if (ecu == 0x12 && reqlen == 2 && req[0] == 0x01 && req[1] == 0x0D) {
        resp[0] = 0x41; resp[1] = 0x0D; resp[2] = (unsigned char)car->speed;
        return 3;
    }
    if (ecu == 0x12 && reqlen == 2 && req[0] == 0x01 && req[1] == 0x0C) {
        unsigned r = car->engine_on ? (unsigned)(800 * 4) : 0u;   /* rpm*4 raw; 0 == off */
        resp[0] = 0x41; resp[1] = 0x0C; resp[2] = (unsigned char)(r >> 8); resp[3] = (unsigned char)r;
        return 4;
    }
    resp[0] = 0x7F; resp[1] = (reqlen ? req[0] : 0); resp[2] = 0x11;
    return 3;
}

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    const char *why;
    fake_car car;

    /* --- 1) the fuse, case-for-case with control.selftest() --- */
    CHECK( nd_xdrive_interlock(0, 1, 0, 1, 1, &why));                 /* stopped, Park, off -> ok */
    CHECK( nd_xdrive_interlock(0, 1, 0, 0, 1, &why));                 /* Park waived */
    CHECK(!nd_xdrive_interlock(5, 1, 0, 1, 1, &why));                 /* moving -> blocked */
    CHECK(!nd_xdrive_interlock(0, 0, 0, 1, 1, &why));                 /* not in Park -> blocked */
    CHECK(!nd_xdrive_interlock(ND_UNKNOWN, 1, 0, 1, 1, &why));        /* unknown speed -> refuse */
    CHECK(!nd_xdrive_interlock(0, ND_UNKNOWN, 0, 1, 1, &why));        /* unknown Park -> refuse */
    CHECK( nd_xdrive_interlock(0, ND_UNKNOWN, 0, 0, 1, &why));        /* ...unless Park waived */
    CHECK(!nd_xdrive_interlock(0, 1, 1, 1, 1, &why));                 /* engine running -> blocked */
    CHECK( nd_xdrive_interlock(0, 1, 1, 1, 0, &why));                 /* ...engine-off waived */
    CHECK(!nd_xdrive_interlock(0, 1, ND_UNKNOWN, 1, 1, &why));        /* unknown engine -> refuse */
    CHECK( nd_xdrive_interlock(0, 1, ND_UNKNOWN, 1, 0, &why));        /* ...unless waived */

    /* --- 2) full flow against the fake, variant 0x0F2510 (RWD live, AWD needs ign cycle) --- */
    car.variant = 0x0F2510UL; car.mode = ND_XDRIVE_AWD; car.speed = 0; car.park = 1; car.engine_on = 0;

    CHECK(nd_xdrive_variant(fake_dsc, &car) == 0x0F2510UL);
    CHECK(nd_xdrive_variant_supported(0x0F2510UL) && !nd_xdrive_variant_supported(0x123456UL));
    CHECK(nd_xdrive_mode(fake_dsc, &car) == ND_XDRIVE_AWD);

    /* dry run: no write */
    CHECK(nd_xdrive_set(fake_dsc, &car, ND_XDRIVE_RWD, 0, 1, 1, &why) == ND_XDRIVE_DRYRUN);
    CHECK(car.mode == ND_XDRIVE_AWD);

    /* commit RWD at standstill/Park/off: write + read-back */
    CHECK(nd_xdrive_set(fake_dsc, &car, ND_XDRIVE_RWD, 1, 1, 1, &why) == ND_XDRIVE_OK);
    CHECK(car.mode == ND_XDRIVE_RWD);

    /* engine running blocks the write (and nothing is sent) */
    car.mode = ND_XDRIVE_AWD; car.engine_on = 1;
    CHECK(nd_xdrive_set(fake_dsc, &car, ND_XDRIVE_RWD, 1, 1, 1, &why) == ND_XDRIVE_BLOCKED);
    CHECK(car.mode == ND_XDRIVE_AWD);
    car.engine_on = 0;

    /* AWD restore on 0x0F2510 needs an ignition cycle (no live write) */
    car.mode = ND_XDRIVE_RWD;
    CHECK(nd_xdrive_set(fake_dsc, &car, ND_XDRIVE_AWD, 1, 1, 1, &why) == ND_XDRIVE_NEEDS_IGN_CYCLE);

    /* unsupported variant is refused up front */
    car.variant = 0x123456UL;
    CHECK(nd_xdrive_set(fake_dsc, &car, ND_XDRIVE_RWD, 1, 1, 1, &why) == ND_XDRIVE_BAD_VARIANT);

    /* --- 3) variant 0x0F2850 does a live AWD restore --- */
    car.variant = 0x0F2850UL; car.mode = ND_XDRIVE_RWD; car.speed = 0; car.park = 1; car.engine_on = 0;
    CHECK(nd_xdrive_set(fake_dsc, &car, ND_XDRIVE_AWD, 1, 1, 1, &why) == ND_XDRIVE_OK);
    CHECK(car.mode == ND_XDRIVE_AWD);

    if (fails) { printf("\n%d FAILURE(S)\n", fails); return 1; }
    printf("xdrive OK: fuse (standstill/Park/engine-off, fail-safe) + set-mode flow vs fake DSC\n");
    return 0;
}
