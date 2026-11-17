/* nbtevo-id6-apps — Codes worker test (Apache-2.0). No car, no QNX.
 *
 * Drives the worker through a fake transport and checks the parser's edges. The canned
 * 19 02 FF response is the SAME shape asserted by nbtevo-diag's test_transport.py
 * (SHARED_DTCS), so the Python reader and the C collector agree on the wire.
 */
#include <stdio.h>
#include <string.h>

#include "codes.h"

/* Shared vector with diag: DTC 0x101A05 status 0x08 (confirmed), 0x480011 status 0x2E. */
static const unsigned char CANNED_2[] = {
    0x59, 0x02, 0xFF,
    0x10, 0x1A, 0x05, 0x08,
    0x48, 0x00, 0x11, 0x2E
};

/* Fake nd_uds_fn: answer 19 02 FF with CANNED_2, else a UDS negative response. */
static int fake_send(void *ctx, unsigned char ecu,
                     const unsigned char *req, int reqlen,
                     unsigned char *resp, int maxresp)
{
    (void)ctx; (void)ecu;
    if (reqlen == 3 && req[0] == 0x19 && req[1] == 0x02 && req[2] == 0xFF) {
        int n = (int)sizeof CANNED_2;
        if (n > maxresp) n = maxresp;
        memcpy(resp, CANNED_2, (size_t)n);
        return n;
    }
    if (maxresp >= 3) { resp[0] = 0x7F; resp[1] = (reqlen ? req[0] : 0); resp[2] = 0x11; }
    return 3;
}

/* Fake that always NRCs the request (serviceNotSupported). */
static int fake_nrc(void *ctx, unsigned char ecu,
                    const unsigned char *req, int reqlen,
                    unsigned char *resp, int maxresp)
{
    (void)ctx; (void)ecu; (void)req; (void)reqlen;
    if (maxresp < 3) return -1;
    resp[0] = 0x7F; resp[1] = 0x19; resp[2] = 0x11;
    return 3;
}

/* Fake that reports a transport failure. */
static int fake_fail(void *ctx, unsigned char ecu,
                     const unsigned char *req, int reqlen,
                     unsigned char *resp, int maxresp)
{
    (void)ctx; (void)ecu; (void)req; (void)reqlen; (void)resp; (void)maxresp;
    return -1;
}

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    nd_dtc d[8];
    int n;

    /* 1) worker end-to-end through the fake transport (shared vector) */
    n = nd_read_dtcs(fake_send, 0, 0x12, d, 8);
    CHECK(n == 2);
    CHECK(d[0].code == 0x101A05UL && d[0].status == 0x08);
    CHECK(d[1].code == 0x480011UL && d[1].status == 0x2E);
    CHECK(ND_DTC_ACTIVE(d[0].status));             /* 0x08 confirmed -> active */
    CHECK((d[1].status & ND_DTC_PENDING) != 0);    /* 0x2E has the pending (0x04) bit */

    /* 2) empty fault memory -> 0 */
    {
        static const unsigned char empty[] = { 0x59, 0x02, 0xFF };
        CHECK(nd_parse_dtcs(empty, (int)sizeof empty, d, 8) == 0);
    }

    /* 3) trailing partial record (3 bytes, no status) is ignored */
    {
        static const unsigned char partial[] = { 0x59, 0x02, 0xFF, 0x10, 0x1A, 0x05 };
        CHECK(nd_parse_dtcs(partial, (int)sizeof partial, d, 8) == 0);
    }

    /* 4) wrong service echo -> rejected */
    {
        static const unsigned char bad[] = { 0x62, 0x00, 0x00 };
        CHECK(nd_parse_dtcs(bad, (int)sizeof bad, d, 8) == -1);
    }

    /* 5) out[] clamp: room for only 1 of 2 */
    CHECK(nd_read_dtcs(fake_send, 0, 0x12, d, 1) == 1);

    /* 6) UDS negative response -> -1 */
    CHECK(nd_read_dtcs(fake_nrc, 0, 0x12, d, 8) == -1);

    /* 7) transport error -> -1 */
    CHECK(nd_read_dtcs(fake_fail, 0, 0x12, d, 8) == -1);

    /* 8) null guards */
    CHECK(nd_parse_dtcs(0, 0, d, 8) == -1);
    CHECK(nd_read_dtcs(0, 0, 0x12, d, 8) == -1);

    if (fails) {
        printf("\n%d FAILURE(S)\n", fails);
        return 1;
    }
    printf("codes OK: worker + parser, shared 19 02 FF vector matches diag\n");
    return 0;
}
