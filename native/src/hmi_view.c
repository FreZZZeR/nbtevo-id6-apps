/* nbtevo-id6-apps — HMI presentation (Apache-2.0). See hmi_view.h. */
#include "hmi_view.h"
#include <stdio.h>

/* Presentation metadata per gauge slot (ASCII units; the decode source of truth stays in
 * nbtevo-diag signals.py). Order matches the NDG_* enum in frame.h. */
static const struct { const char *label; const char *unit; int dp; } NDG_META[ND_NGAUGE] = {
    { "Coolant", "C",   0 },
    { "Oil",     "C",   0 },
    { "Intake",  "C",   0 },
    { "Boost",   "bar", 2 },
    { "RPM",     "",    0 },
    { "Torque",  "Nm",  0 },
    { "Power",   "kW",  0 },
    { "Trans",   "C",   0 }
};

int nd_fmt_gauge(const nd_frame *f, int slot, char *out, int n)
{
    if (slot < 0 || slot >= ND_NGAUGE)
        return -1;
    if (!f->gauge_ok[slot])
        return snprintf(out, (size_t)n, "%s: ---", NDG_META[slot].label);
    if (NDG_META[slot].unit[0])
        return snprintf(out, (size_t)n, "%s: %.*f %s",
                        NDG_META[slot].label, NDG_META[slot].dp, f->gauge[slot], NDG_META[slot].unit);
    return snprintf(out, (size_t)n, "%s: %.*f",
                    NDG_META[slot].label, NDG_META[slot].dp, f->gauge[slot]);
}

int nd_fmt_dtc(const nd_dtc *d, char *out, int n)
{
    return snprintf(out, (size_t)n, "%06lX  [%02X]%s",
                    d->code, (unsigned)d->status, ND_DTC_ACTIVE(d->status) ? " active" : "");
}

int nd_fmt_codes(const nd_frame *f, char *out, int n)
{
    if (f->dtc_count == 0)
        return snprintf(out, (size_t)n, "No faults");
    return snprintf(out, (size_t)n, "%u fault%s%s",
                    (unsigned)f->dtc_count, f->dtc_count == 1 ? "" : "s",
                    f->dtc_active ? " - active" : "");
}

int nd_fmt_xdrive(const nd_frame *f, char *out, int n)
{
    const char *m = f->xdrive_mode == 0 ? "RWD (front axle off)"
                  : f->xdrive_mode == 1 ? "AWD"
                  : "n/a";
    if (!f->xdrive_variant_ok)
        return snprintf(out, (size_t)n, "xDrive: %s (variant unverified)", m);
    return snprintf(out, (size_t)n, "xDrive: %s", m);
}
