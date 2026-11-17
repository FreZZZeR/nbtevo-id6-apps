/* nbtevo-id6-apps — xDrive control (Apache-2.0). See xdrive.h. Mirrors control.py. */
#include "xdrive.h"

#define DSC 0x29   /* stability/DSC module — commands the transfer case */
#define EGS 0x18   /* transmission (Park) */
#define DME 0x12   /* engine (speed, rpm) */

#define DID_VARIANT    0xF150   /* 22 F150 -> DSC variant index */
#define DID_CAPABILITY 0xDBE1   /* 22 DBE1 read mode; 2E DBE1 <v> is the control */
#define DID_GEAR       0xDA2E   /* EGS: 00 = Park */

static const unsigned long SUPPORTED[2] = { 0x0F2510UL, 0x0F2850UL };

int nd_xdrive_variant_supported(unsigned long v)
{
    return v == SUPPORTED[0] || v == SUPPORTED[1];
}

unsigned long nd_xdrive_variant(nd_uds_fn send, void *ctx)
{
    unsigned char d[16];
    int len, i;

    len = nd_uds_read(send, ctx, DSC, 0x22, DID_VARIANT, d, (int)sizeof d);
    if (len < 3)
        return 0;
    for (i = 0; i + 3 <= len; i++) {   /* first non-zero 3-byte index (as control.py parses) */
        unsigned long idx = ((unsigned long)d[i] << 16)
                          | ((unsigned long)d[i + 1] << 8)
                          |  (unsigned long)d[i + 2];
        if (idx)
            return idx;
    }
    return 0;
}

int nd_xdrive_mode(nd_uds_fn send, void *ctx)
{
    unsigned char d[8];
    int len = nd_uds_read(send, ctx, DSC, 0x22, DID_CAPABILITY, d, (int)sizeof d);
    if (len < 1)
        return ND_UNKNOWN;
    if (d[0] == ND_XDRIVE_AWD) return ND_XDRIVE_AWD;
    if (d[0] == ND_XDRIVE_RWD) return ND_XDRIVE_RWD;
    return ND_UNKNOWN;
}

int nd_xdrive_interlock(int speed_kmh, int in_park, int engine_running,
                        int require_park, int require_engine_off, const char **why)
{
    if (speed_kmh < 0)        { if (why) *why = "could not read vehicle speed - refusing"; return 0; }
    if (speed_kmh != 0)       { if (why) *why = "vehicle is moving - engage RWD only at a standstill"; return 0; }
    if (require_park) {
        if (in_park < 0)      { if (why) *why = "could not confirm Park - refusing (waive to override)"; return 0; }
        if (!in_park)         { if (why) *why = "not in Park"; return 0; }
    }
    if (require_engine_off) {
        if (engine_running < 0) { if (why) *why = "could not read engine state - refusing (waive to override)"; return 0; }
        if (engine_running)     { if (why) *why = "engine is running - switch it off, or waive"; return 0; }
    }
    if (why) *why = "ok";
    return 1;
}

/* --- condition reads over the seam (mirror control.read_speed/read_park/read_engine_running) --- */
static int read_speed(nd_uds_fn send, void *ctx)
{
    unsigned char d[4];
    int n = nd_uds_read(send, ctx, DME, 0x01, 0x000D, d, (int)sizeof d);
    return n < 1 ? ND_UNKNOWN : (int)d[0];
}

static int read_park(nd_uds_fn send, void *ctx)
{
    unsigned char d[4];
    int n = nd_uds_read(send, ctx, EGS, 0x22, DID_GEAR, d, (int)sizeof d);
    return n < 1 ? ND_UNKNOWN : (d[0] == 0);
}

static int read_engine(nd_uds_fn send, void *ctx)
{
    unsigned char d[4];
    int n = nd_uds_read(send, ctx, DME, 0x01, 0x000C, d, (int)sizeof d);   /* rpm; both bytes 0 == off */
    return n < 2 ? ND_UNKNOWN : ((d[0] | d[1]) != 0);
}

nd_xdrive_result nd_xdrive_set(nd_uds_fn send, void *ctx, int target, int commit,
                               int require_park, int require_engine_off, const char **why)
{
    unsigned long var = nd_xdrive_variant(send, ctx);
    unsigned char wr[4], resp[16];
    int wn, mode;

    if (!nd_xdrive_variant_supported(var)) {
        if (why) *why = "DSC variant not in the supported set - validate on this car first";
        return ND_XDRIVE_BAD_VARIANT;
    }

    if (target == ND_XDRIVE_RWD) {
        if (!nd_xdrive_interlock(read_speed(send, ctx), read_park(send, ctx), read_engine(send, ctx),
                                 require_park, require_engine_off, why))
            return ND_XDRIVE_BLOCKED;
    } else {                                   /* restoring AWD is the safe direction */
        if (var != 0x0F2850UL) {
            if (why) *why = "this DSC restores AWD on an ignition cycle (no live write)";
            return ND_XDRIVE_NEEDS_IGN_CYCLE;
        }
    }

    if (!commit) {
        if (why) *why = "dry run - would send 2E DBE1 <target> on DSC 0x29";
        return ND_XDRIVE_DRYRUN;
    }

    /* NB: the caller is expected to have enforced the VIN consent gate before commit. */
    wr[0] = 0x2E;
    wr[1] = (unsigned char)(DID_CAPABILITY >> 8);
    wr[2] = (unsigned char)(DID_CAPABILITY & 0xFF);
    wr[3] = (unsigned char)target;
    wn = send(ctx, DSC, wr, 4, resp, (int)sizeof resp);
    if (wn <= 0 || resp[0] == 0x7F) {          /* transport error or UDS negative response */
        if (why) *why = "DSC refused the write";
        return ND_XDRIVE_WRITE_FAILED;
    }

    mode = nd_xdrive_mode(send, ctx);          /* read-back verify, like control.py */
    if (mode == target) {
        if (why) *why = "confirmed by read-back";
        return ND_XDRIVE_OK;
    }
    if (why) *why = "write accepted; mode unconfirmed by read-back";
    return ND_XDRIVE_UNCONFIRMED;
}
