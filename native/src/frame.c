/* nbtevo-id6-apps — collector<->plugin frame (Apache-2.0). See frame.h. */
#include "frame.h"
#include <string.h>

static unsigned char *p_u16(unsigned char *p, uint16_t v)
{ p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); return p + 2; }
static unsigned char *p_u32(unsigned char *p, uint32_t v)
{ int i; for (i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8 * i)); return p + 4; }
static unsigned char *p_u64(unsigned char *p, uint64_t v)
{ int i; for (i = 0; i < 8; i++) p[i] = (unsigned char)(v >> (8 * i)); return p + 8; }
static unsigned char *p_f64(unsigned char *p, double d)
{ uint64_t u; memcpy(&u, &d, 8); return p_u64(p, u); }

static const unsigned char *g_u16(const unsigned char *p, uint16_t *v)
{ *v = (uint16_t)(p[0] | (p[1] << 8)); return p + 2; }
static const unsigned char *g_u32(const unsigned char *p, uint32_t *v)
{ uint32_t x = 0; int i; for (i = 0; i < 4; i++) x |= (uint32_t)p[i] << (8 * i); *v = x; return p + 4; }
static const unsigned char *g_u64(const unsigned char *p, uint64_t *v)
{ uint64_t x = 0; int i; for (i = 0; i < 8; i++) x |= (uint64_t)p[i] << (8 * i); *v = x; return p + 8; }
static const unsigned char *g_f64(const unsigned char *p, double *d)
{ uint64_t u; p = g_u64(p, &u); memcpy(d, &u, 8); return p; }

int nd_frame_encode(const nd_frame *f, unsigned char *out, int n)
{
    unsigned char *p = out;
    int i;
    if (n < ND_FRAME_WIRE)
        return -1;
    p = p_u32(p, f->seq);
    p = p_u32(p, f->valid);
    p = p_u64(p, f->sent_ms);
    for (i = 0; i < ND_NGAUGE; i++) p = p_f64(p, f->gauge[i]);
    for (i = 0; i < ND_NGAUGE; i++) *p++ = f->gauge_ok[i];
    p = p_u16(p, f->dtc_count);
    *p++ = f->dtc_active;
    *p++ = f->xdrive_mode;
    *p++ = f->xdrive_variant_ok;
    return (int)(p - out);
}

int nd_frame_decode(const unsigned char *in, int n, nd_frame *out)
{
    const unsigned char *p = in;
    int i;
    if (n < ND_FRAME_WIRE)
        return 0;
    p = g_u32(p, &out->seq);
    p = g_u32(p, &out->valid);
    p = g_u64(p, &out->sent_ms);
    for (i = 0; i < ND_NGAUGE; i++) p = g_f64(p, &out->gauge[i]);
    for (i = 0; i < ND_NGAUGE; i++) out->gauge_ok[i] = *p++;
    p = g_u16(p, &out->dtc_count);
    out->dtc_active = *p++;
    out->xdrive_mode = *p++;
    out->xdrive_variant_ok = *p++;
    return 1;
}

int nd_frame_fresh(uint64_t sent_ms, uint64_t now_ms, unsigned stale_ms)
{
    return now_ms >= sent_ms && (now_ms - sent_ms) <= stale_ms;
}
