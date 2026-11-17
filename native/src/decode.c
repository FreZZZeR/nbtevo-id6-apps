/* nbtevo-id6-apps — signal decode core (Apache-2.0). See decode.h. */
#include "decode.h"

/* pi as a literal to avoid M_PI portability (not in strict C99 <math.h>). */
#define ND_PI 3.14159265358979323846

double nd_rawint(const unsigned char *d, int width, int is_signed)
{
    unsigned long u = 0;
    int i;
    for (i = 0; i < width; i++)
        u = (u << 8) | d[i];
    if (is_signed) {
        /* ponytail: width<=2 in this catalog, so 1<<16 is safe in unsigned long. */
        unsigned long bits = (unsigned long)width * 8u;
        if (u & (1UL << (bits - 1u)))
            return (double)u - (double)(1UL << bits);
    }
    return (double)u;
}

int nd_value(const nd_sig *s, const unsigned char *d, int len, double *out)
{
    if (len < (int)s->width)
        return 0;
    *out = nd_rawint(d, (int)s->width, (int)s->is_signed) * (double)s->num / (double)s->den
           + s->offset;
    return 1;
}

int nd_plausible(const nd_sig *s, double v)
{
    if (s->has_lo && v < s->lo) return 0;
    if (s->has_hi && v > s->hi) return 0;
    return 1;
}

double nd_boost_bar(double map_kpa, double baro_kpa)
{
    double b = (map_kpa - baro_kpa) / 100.0;
    return b < 0.0 ? 0.0 : b;
}

double nd_torque_nm(double ref_nm, double actual_pct)
{
    return ref_nm * actual_pct / 100.0;
}

double nd_power_kw(double torque_nm, double rpm)
{
    return torque_nm * rpm * 2.0 * ND_PI / 60.0 / 1000.0;
}
