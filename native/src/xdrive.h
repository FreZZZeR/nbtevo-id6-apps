/* nbtevo-id6-apps — xDrive (xDelete-style temporary RWD) control (Apache-2.0).
 *
 * Mirror of nbtevo-diag control.py. The DSC (0x29) commands the transfer case via
 * 2E DBE1 <0=RWD | 1=AWD>, after a variant check (22 F150 -> 0x0F2510 / 0x0F2850). Every
 * precondition is a CODE-ENFORCED fuse: standstill is non-bypassable; Park and engine-off
 * are defaults you waive consciously; an unreadable condition REFUSES (fail-safe). Dry-run
 * unless commit; the mode is read back to confirm. Transport is the nd_uds_fn seam.
 *
 * WARNING: disables front-axle drive — changes traction and the DSC/ABS envelope. Track /
 * private land only, never a public road. The DSC variant + DIDs come from a 340i reference;
 * validate them on THIS car's DSC before trusting a live write.
 */
#ifndef ND_XDRIVE_H
#define ND_XDRIVE_H

#include "uds.h"   /* nd_uds_fn */

#define ND_XDRIVE_RWD 0
#define ND_XDRIVE_AWD 1

/* Tri-state inputs to the fuse: a negative value means "could not read" -> the fuse refuses. */
#define ND_UNKNOWN (-1)

typedef enum {
    ND_XDRIVE_OK = 0,          /* commit: write accepted and read back as the target mode */
    ND_XDRIVE_DRYRUN,          /* commit == 0: would have sent the command */
    ND_XDRIVE_BLOCKED,         /* interlock refused (engaging RWD) */
    ND_XDRIVE_BAD_VARIANT,     /* DSC variant unreadable / not in the supported set */
    ND_XDRIVE_NEEDS_IGN_CYCLE, /* AWD restore on a variant that only re-arms on an ignition cycle */
    ND_XDRIVE_UNCONFIRMED,     /* write accepted but the read-back didn't confirm the target */
    ND_XDRIVE_WRITE_FAILED     /* DSC refused the write / transport error */
} nd_xdrive_result;

/* The software fuse. Pure, unit-testable. speed_kmh / in_park / engine_running use ND_UNKNOWN
 * (< 0) for "could not read". Standstill (speed == 0) is never bypassable; require_park and
 * require_engine_off default on (pass 0 to waive each). Returns 1 if engaging RWD is allowed,
 * else 0 with *why (if non-null) pointing at a static reason string. */
int nd_xdrive_interlock(int speed_kmh, int in_park, int engine_running,
                        int require_park, int require_engine_off, const char **why);

/* DSC variant index (22 F150): the first non-zero 3-byte value, or 0 if unreadable. */
unsigned long nd_xdrive_variant(nd_uds_fn send, void *ctx);
int           nd_xdrive_variant_supported(unsigned long variant);

/* Current transfer-case mode (22 DBE1): ND_XDRIVE_RWD / ND_XDRIVE_AWD, or ND_UNKNOWN. */
int nd_xdrive_mode(nd_uds_fn send, void *ctx);

/* Engage RWD (target == ND_XDRIVE_RWD) or restore AWD. Mirrors control.set_mode: variant
 * gate; interlock (only when engaging RWD, reading speed/Park/engine over `send`); dry-run
 * unless commit; write 2E DBE1 <target>; read back. The caller must enforce the VIN consent
 * gate before commit. *why (optional) receives a static explanation. */
nd_xdrive_result nd_xdrive_set(nd_uds_fn send, void *ctx, int target, int commit,
                               int require_park, int require_engine_off, const char **why);

#endif /* ND_XDRIVE_H */
