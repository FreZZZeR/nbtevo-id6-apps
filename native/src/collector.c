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

int nd_collect(nd_uds_fn send, void *ctx, nd_frame *out)
{
    double coolant, oil, iat, map, baro, rpm, reft, tpct, trans;
    int okmap, okbaro, okrpm, okref, oktp, nok = 0, ndtc, i;
    nd_dtc d[16];
    unsigned long var;

    out->valid = 1;

    out->gauge_ok[NDG_COOLANT] = (uint8_t)read_one(send, ctx, "coolant", &coolant);
    out->gauge[NDG_COOLANT] = coolant; nok += out->gauge_ok[NDG_COOLANT];
    out->gauge_ok[NDG_OIL] = (uint8_t)read_one(send, ctx, "oil_temp", &oil);
    out->gauge[NDG_OIL] = oil; nok += out->gauge_ok[NDG_OIL];
    out->gauge_ok[NDG_IAT] = (uint8_t)read_one(send, ctx, "iat", &iat);
    out->gauge[NDG_IAT] = iat; nok += out->gauge_ok[NDG_IAT];

    /* boost = (MAP - baro)/100, needs both */
    okmap  = read_one(send, ctx, "map",  &map);
    okbaro = read_one(send, ctx, "baro", &baro);
    out->gauge_ok[NDG_BOOST] = (uint8_t)(okmap && okbaro);
    out->gauge[NDG_BOOST] = (okmap && okbaro) ? nd_boost_bar(map, baro) : 0.0;
    nok += out->gauge_ok[NDG_BOOST];

    okrpm = read_one(send, ctx, "rpm", &rpm);
    out->gauge_ok[NDG_RPM] = (uint8_t)okrpm;
    out->gauge[NDG_RPM] = rpm; nok += okrpm;

    /* torque = ref * actual% / 100; power = torque * rpm (needs rpm too) */
    okref = read_one(send, ctx, "ref_torque", &reft);
    oktp  = read_one(send, ctx, "torque_pct", &tpct);
    if (okref && oktp) {
        double tq = nd_torque_nm(reft, tpct);
        out->gauge[NDG_TORQUE] = tq; out->gauge_ok[NDG_TORQUE] = 1; nok++;
        out->gauge_ok[NDG_POWER] = (uint8_t)okrpm;
        out->gauge[NDG_POWER] = okrpm ? nd_power_kw(tq, rpm) : 0.0;
        nok += okrpm;
    } else {
        out->gauge[NDG_TORQUE] = 0.0; out->gauge_ok[NDG_TORQUE] = 0;
        out->gauge[NDG_POWER]  = 0.0; out->gauge_ok[NDG_POWER]  = 0;
    }

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
