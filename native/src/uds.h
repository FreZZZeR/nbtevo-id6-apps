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

#define ND_DYN_DID 0xF303   /* the scratch dynamic DID ISTA uses — one shared slot per ECU */

/* Read `width` bytes of source DID `src` through the dynamic DID, for ECUs whose live values
 * refuse a direct 22 (the N57 DDE answers NRC 31): clear F303 (2C 03), define it by identifier
 * (2C 01 F303 <src> position=1 size=width), then read 22 F303. Mirrors transport.dyn_did in
 * Python. Same return contract as nd_uds_read. */
int nd_uds_read_dyn(nd_uds_fn send, void *ctx, unsigned char ecu, unsigned short src,
                    unsigned char width, unsigned char *out, int maxout);

#endif /* ND_UDS_H */
