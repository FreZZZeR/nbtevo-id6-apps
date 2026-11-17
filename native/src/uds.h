/* nbtevo-id6-apps — UDS/OBD transport seam + a generic read (Apache-2.0).
 *
 * nd_uds_fn is THE seam between our logic and the wire: a fake feeds canned bytes in tests,
 * the head-unit build plugs in the real HSFZ socket to the FEM gateway (Stage C/D). Nothing
 * above this line touches a car. nd_uds_read is the shared "ask one PID/DID, get the data
 * bytes" helper used by the Gauges and xDrive workers (mirrors transport.obd/did in Python).
 */
#ifndef ND_UDS_H
#define ND_UDS_H

/* Send a UDS request to one ECU, receive its response. Returns the response length (>0),
 * or <=0 on a transport error. ctx is passed through (e.g. a socket, or a test fixture). */
typedef int (*nd_uds_fn)(void *ctx, unsigned char ecu,
                         const unsigned char *req, int reqlen,
                         unsigned char *resp, int maxresp);

/* Build + send an OBD mode-01 (service 0x01) or UDS ReadDataByIdentifier (0x22) request to
 * `ecu` for `addr` (a PID for OBD, a DID for UDS), check the positive echo (41 <pid> /
 * 62 <hi> <lo>), and copy the data bytes AFTER the echo into out[]. Returns the data length
 * (>=0), or -1 on a transport error, a UDS negative response (7F), or an echo mismatch. */
int nd_uds_read(nd_uds_fn send, void *ctx, unsigned char ecu, unsigned char service,
                unsigned short addr, unsigned char *out, int maxout);

#endif /* ND_UDS_H */
