/* nbtevo-id6-apps — frame encode/decode test (Apache-2.0). No car, no QNX. */
#include <stdio.h>
#include <string.h>

#include "frame.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    nd_frame a, b;
    unsigned char buf[ND_FRAME_WIRE + 4];
    int i, w;

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    a.seq = 7; a.valid = 1; a.sent_ms = 123456789ULL;
    for (i = 0; i < ND_NGAUGE; i++) { a.gauge[i] = i * 1.25 - 3.5; a.gauge_ok[i] = (uint8_t)(i % 2); }
    a.dtc_count = 3; a.dtc_active = 1; a.xdrive_mode = 1; a.xdrive_variant_ok = 1;

    /* roundtrip */
    w = nd_frame_encode(&a, buf, (int)sizeof buf);
    CHECK(w == ND_FRAME_WIRE);
    CHECK(nd_frame_decode(buf, w, &b) == 1);
    CHECK(a.seq == b.seq && a.valid == b.valid && a.sent_ms == b.sent_ms);
    for (i = 0; i < ND_NGAUGE; i++) {
        CHECK(a.gauge[i] == b.gauge[i]);         /* LE roundtrip preserves the bits exactly */
        CHECK(a.gauge_ok[i] == b.gauge_ok[i]);
    }
    CHECK(a.dtc_count == b.dtc_count && a.dtc_active == b.dtc_active);
    CHECK(a.xdrive_mode == b.xdrive_mode && a.xdrive_variant_ok == b.xdrive_variant_ok);

    /* short buffers refused */
    CHECK(nd_frame_encode(&a, buf, ND_FRAME_WIRE - 1) == -1);
    CHECK(nd_frame_decode(buf, ND_FRAME_WIRE - 1, &b) == 0);

    /* freshness */
    CHECK(nd_frame_fresh(1000, 1500, 2000));
    CHECK(!nd_frame_fresh(1000, 4000, 2000));    /* too old */
    CHECK(!nd_frame_fresh(2000, 1000, 2000));    /* now before sent */

    if (fails) { printf("\n%d FAILURE(S)\n", fails); return 1; }
    printf("frame OK: LE encode/decode roundtrip + freshness\n");
    return 0;
}
