/* nbtevo-id6-apps — collector core (Apache-2.0). See collector.h. */
#include "collector.h"
#include "decode.h"
#include "dids.h"
#include "gauges.h"
#include "codes.h"
#include "xdrive.h"
#include <string.h>

static const nd_sig *find_sig(const char *key)
{
    size_t i;
    for (i = 0; i < ND_NSIGS; i++)
        if (strcmp(ND_SIGS[i].key, key) == 0)
            return &ND_SIGS[i];
    return 0;
}

/* Read one catalog signal by key; returns 1 (ok) and sets *v, or 0. */
static int read_one(nd_uds_fn send, void *ctx, const char *key, double *v)
{
    const nd_sig *sig = find_sig(key);
    nd_reading r;
    *v = 0.0;
    if (!sig)
        return 0;
    nd_read_signal(send, ctx, sig, &r);
    *v = r.value;
    return r.ok;
}

/* Which signals this engine answers. Mode-01 is a standard: where it answers we use it and
 * never touch the DDE's own DIDs. Only where it is dead (the N57 through the FEM gateway) do
 * we read the DDE's values, through the dynamic DID. Cached, because on the N57 the mode-01
 * probe costs a transport timeout: paid once, not on every tick.
 * ponytail: "mode-01 dead and rpm_dde plausible" is the whole engine check. Match the DDE's
 * SGBD index (22 F150) before trusting the DDE set on another diesel. */
enum { FAM_UNKNOWN, FAM_OBD, FAM_DDE };
static int g_family = FAM_UNKNOWN;

static int probe_family(nd_uds_fn send, void *ctx)
{
    double v;
    if (read_one(send, ctx, "rpm", &v))
        return FAM_OBD;
    if (read_one(send, ctx, "rpm_dde", &v))
        return FAM_DDE;
    return FAM_UNKNOWN;
}

int nd_collect(nd_uds_fn send, void *ctx, nd_frame *out)
{
    double coolant, oil, iat, map, baro, rpm, tq, trans;
    int okmap, okbaro, okrpm, oktq, nok = 0, ndtc, i, dde;
    nd_dtc d[16];
    unsigned long var;

    out->valid = 1;

    if (g_family == FAM_UNKNOWN)
        g_family = probe_family(send, ctx);
    dde = (g_family == FAM_DDE);

    out->gauge_ok[NDG_COOLANT] = (uint8_t)read_one(send, ctx, dde ? "coolant_dde" : "coolant", &coolant);
    out->gauge[NDG_COOLANT] = coolant; nok += out->gauge_ok[NDG_COOLANT];
    out->gauge_ok[NDG_OIL] = (uint8_t)read_one(send, ctx, dde ? "oil_temp_dde" : "oil_temp", &oil);
    out->gauge[NDG_OIL] = oil; nok += out->gauge_ok[NDG_OIL];
    out->gauge_ok[NDG_IAT] = (uint8_t)read_one(send, ctx, dde ? "iat_dde" : "iat", &iat);
    out->gauge[NDG_IAT] = iat; nok += out->gauge_ok[NDG_IAT];

    /* boost = absolute - ambient, needs both. Mode-01 reports kPa, the DDE hPa. */
    okmap  = read_one(send, ctx, dde ? "boost_abs_dde" : "map",  &map);
    okbaro = read_one(send, ctx, dde ? "baro_dde"      : "baro", &baro);
    if (dde) { map /= 10.0; baro /= 10.0; }
    out->gauge_ok[NDG_BOOST] = (uint8_t)(okmap && okbaro);
    out->gauge[NDG_BOOST] = (okmap && okbaro) ? nd_boost_bar(map, baro) : 0.0;
    nok += out->gauge_ok[NDG_BOOST];

    okrpm = read_one(send, ctx, dde ? "rpm_dde" : "rpm", &rpm);
    out->gauge_ok[NDG_RPM] = (uint8_t)okrpm;
    out->gauge[NDG_RPM] = rpm; nok += okrpm;

    /* torque: the DDE reports Nm directly; mode-01 gives reference * actual% / 100.
     * power = torque * rpm (needs rpm too). */
    if (dde) {
        oktq = read_one(send, ctx, "torque_dde", &tq);
    } else {
        double reft, tpct;
        int okref = read_one(send, ctx, "ref_torque", &reft);
        int oktp  = read_one(send, ctx, "torque_pct", &tpct);
        oktq = okref && oktp;
        tq = oktq ? nd_torque_nm(reft, tpct) : 0.0;
    }
    out->gauge_ok[NDG_TORQUE] = (uint8_t)oktq;
    out->gauge[NDG_TORQUE] = oktq ? tq : 0.0;
    out->gauge_ok[NDG_POWER] = (uint8_t)(oktq && okrpm);
    out->gauge[NDG_POWER] = (oktq && okrpm) ? nd_power_kw(tq, rpm) : 0.0;
    nok += oktq + (oktq && okrpm);

    /* The family stopped answering (ignition off, or the probe was wrong): look again next tick. */
    if (!okrpm)
        g_family = FAM_UNKNOWN;

    out->gauge_ok[NDG_TRANS] = (uint8_t)read_one(send, ctx, "trans_temp", &trans);
    out->gauge[NDG_TRANS] = trans; nok += out->gauge_ok[NDG_TRANS];

    /* fault memory on the engine module (DME 0x12) */
    ndtc = nd_read_dtcs(send, ctx, 0x12, d, 16);
    out->dtc_count = (uint16_t)(ndtc < 0 ? 0 : ndtc);
    out->dtc_active = 0;
    for (i = 0; i < ndtc; i++)
        if (ND_DTC_ACTIVE(d[i].status))
            out->dtc_active = 1;

    /* xDrive current mode + whether the DSC variant is one we support */
    out->xdrive_mode = (uint8_t)nd_xdrive_mode(send, ctx);   /* ND_UNKNOWN (-1) -> 0xFF */
    var = nd_xdrive_variant(send, ctx);
    out->xdrive_variant_ok = (uint8_t)(nd_xdrive_variant_supported(var) ? 1 : 0);

    return nok;
}
