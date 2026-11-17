/* nbtevo-id6-apps — HMI formatting test (Apache-2.0). No car, no QNX. */
#include <stdio.h>
#include <string.h>

#include "hmi_view.h"
#include "frame.h"
#include "codes.h"

static int fails;
/* format into a local buffer `b` and compare to the expected string */
#define EQ(call, exp) do { char b[64]; (void)(call); \
    if (strcmp(b, exp)) { printf("FAIL %s:%d got[%s] exp[%s]\n", __FILE__, __LINE__, b, exp); fails++; } \
    } while (0)

int main(void)
{
    nd_frame f;
    memset(&f, 0, sizeof f);
    f.gauge[NDG_COOLANT] = 50;  f.gauge_ok[NDG_COOLANT] = 1;
    f.gauge[NDG_BOOST]   = 1.3; f.gauge_ok[NDG_BOOST]   = 1;
    f.gauge[NDG_RPM]     = 3000;f.gauge_ok[NDG_RPM]     = 1;
    /* NDG_OIL left not ok */

    EQ(nd_fmt_gauge(&f, NDG_COOLANT, b, (int)sizeof b), "Coolant: 50 C");
    EQ(nd_fmt_gauge(&f, NDG_BOOST,   b, (int)sizeof b), "Boost: 1.30 bar");
    EQ(nd_fmt_gauge(&f, NDG_RPM,     b, (int)sizeof b), "RPM: 3000");
    EQ(nd_fmt_gauge(&f, NDG_OIL,     b, (int)sizeof b), "Oil: ---");

    { char b[8]; if (nd_fmt_gauge(&f, 99, b, (int)sizeof b) != -1) { printf("FAIL bad slot\n"); fails++; } }

    { nd_dtc d; d.code = 0x101A05UL; d.status = 0x08;
      EQ(nd_fmt_dtc(&d, b, (int)sizeof b), "101A05  [08] active"); }
    { nd_dtc d; d.code = 0x480011UL; d.status = 0x04;   /* pending only -> not active */
      EQ(nd_fmt_dtc(&d, b, (int)sizeof b), "480011  [04]"); }

    f.dtc_count = 0;                      EQ(nd_fmt_codes(&f, b, (int)sizeof b), "No faults");
    f.dtc_count = 1; f.dtc_active = 1;    EQ(nd_fmt_codes(&f, b, (int)sizeof b), "1 fault - active");
    f.dtc_count = 3; f.dtc_active = 0;    EQ(nd_fmt_codes(&f, b, (int)sizeof b), "3 faults");

    f.xdrive_variant_ok = 1;
    f.xdrive_mode = 1;    EQ(nd_fmt_xdrive(&f, b, (int)sizeof b), "xDrive: AWD");
    f.xdrive_mode = 0;    EQ(nd_fmt_xdrive(&f, b, (int)sizeof b), "xDrive: RWD (front axle off)");
    f.xdrive_mode = 0xFF; EQ(nd_fmt_xdrive(&f, b, (int)sizeof b), "xDrive: n/a");
    f.xdrive_variant_ok = 0; f.xdrive_mode = 1;
    EQ(nd_fmt_xdrive(&f, b, (int)sizeof b), "xDrive: AWD (variant unverified)");

    if (fails) { printf("\n%d FAILURE(S)\n", fails); return 1; }
    printf("hmi_view OK: gauge/dtc/codes/xdrive formatting\n");
    return 0;
}
