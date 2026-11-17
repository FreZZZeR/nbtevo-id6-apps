/* nbtevo-id6-apps — fault-code (DTC) worker (Apache-2.0).
 *
 * UDS reportDTCByStatusMask (19 02 FF) -> list of (code, status), mirroring the Python
 * reference nbtevo-diag dtc.py. The parse is pure (host-testable, no car); the transport
 * is a function-pointer seam so tests feed canned bytes and the on-car build plugs in the
 * real HSFZ socket to the FEM gateway (Stage C/D). This is the "Codes" app's collector.
 */
#ifndef ND_CODES_H
#define ND_CODES_H

typedef struct {
    unsigned long code;     /* 3-byte DTC, e.g. 0x101A05 */
    unsigned char status;   /* 1-byte ISO-14229 status mask */
} nd_dtc;

/* ISO 14229 DTC status bits (same meaning as dtc.py STATUS_BITS). */
#define ND_DTC_TEST_FAILED              0x01
#define ND_DTC_FAILED_THIS_CYCLE        0x02
#define ND_DTC_PENDING                  0x04
#define ND_DTC_CONFIRMED                0x08
#define ND_DTC_NOT_COMPLETE_SINCE_CLEAR 0x10
#define ND_DTC_FAILED_SINCE_CLEAR       0x20
#define ND_DTC_NOT_COMPLETE_THIS_CYCLE  0x40
#define ND_DTC_WARNING_INDICATOR        0x80

/* "Active" = testFailed or confirmed (matches dtc.Dtc.active). */
#define ND_DTC_ACTIVE(status) (((status) & (ND_DTC_TEST_FAILED | ND_DTC_CONFIRMED)) != 0)

/* Parse a reportDTCByStatusMask positive response: `59 02 <mask>` then 4-byte records
 * (3-byte code + 1-byte status). Writes up to `max` into out[], returns the count, or
 * -1 if resp is not a valid `59 02 ..` reply. A trailing partial record is ignored —
 * byte-for-byte the same shape dtc.read_dtcs() parses. */
int nd_parse_dtcs(const unsigned char *resp, int len, nd_dtc *out, int max);

/* Transport seam: send a UDS request to one ECU and receive its response.
 * Returns the response length (>0), or <=0 on a transport error. A fake implements this
 * in tests; the head-unit build implements it over the real HSFZ/UDS socket. */
typedef int (*nd_uds_fn)(void *ctx, unsigned char ecu,
                         const unsigned char *req, int reqlen,
                         unsigned char *resp, int maxresp);

/* Read fault memory from one ECU: sends `19 02 FF` via `send`, parses into out[].
 * Returns the DTC count (>=0), or -1 on transport error or a negative/!=59 02 reply. */
int nd_read_dtcs(nd_uds_fn send, void *ctx, unsigned char ecu, nd_dtc *out, int max);

#endif /* ND_CODES_H */
