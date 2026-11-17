/* nbtevo-id6-apps — collector process (Apache-2.0).
 *
 * STAGE C shell. Runs on the head unit as a SEPARATE process from HmiMain3D: it owns the
 * HSFZ/UDS gateway link, drives the workers each tick (nd_collect), and ships the frame to
 * the HMI plugin over loopback UDP. Keeping diagnostics out of HmiMain3D is the point — a
 * blocked UI thread bricks the dash. Host-compiles (POSIX); the one TODO is the real HSFZ
 * framing (evocode parity), until which every read honestly refuses.
 */
#define _POSIX_C_SOURCE 200112L
#include "collector.h"
#include "frame.h"
#include "uds.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

/* INADDR_LOOPBACK is a BSD extension hidden under strict _POSIX_C_SOURCE on some hosts. */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001u
#endif

#define ND_PLUGIN_UDP_PORT 42861            /* must match plugin.cpp kFrameUdpPort */
#define ND_GATEWAY_HOST    "169.254.21.3"   /* FEM gateway on ENET — confirm per car */
#define ND_HSFZ_PORT       6801

static volatile sig_atomic_t g_stop;
static void on_sig(int s) { (void)s; g_stop = 1; }

static uint64_t mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* nd_uds_fn over the real HSFZ socket to the FEM gateway (ctx = the connected TCP fd).
 * TODO(Stage C): frame the request as HSFZ exactly like evocode hsfz.py (magic, length,
 * control, src/tgt, payload) and read the reply. Until then this refuses, so the collector
 * ships an honest all-"n/a" frame rather than fabricate values. */
static int hsfz_send(void *ctx, unsigned char ecu, const unsigned char *req, int reqlen,
                     unsigned char *resp, int maxresp)
{
    (void)ctx; (void)ecu; (void)req; (void)reqlen; (void)resp; (void)maxresp;
    return -1;   /* TODO: real HSFZ transport (evocode parity) */
}

int main(void)
{
    int ufd;
    uint32_t seq = 0;
    struct sockaddr_in plugin;
    void *gw = 0;   /* TODO(Stage C): connect TCP to ND_GATEWAY_HOST:ND_HSFZ_PORT; pass fd as ctx */

    signal(SIGTERM, on_sig);
    signal(SIGINT, on_sig);

    ufd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ufd < 0) { perror("socket"); return 1; }
    memset(&plugin, 0, sizeof plugin);
    plugin.sin_family = AF_INET;
    plugin.sin_port = htons(ND_PLUGIN_UDP_PORT);
    plugin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    while (!g_stop) {
        nd_frame f;
        unsigned char buf[ND_FRAME_WIRE];
        int w;
        struct timespec nap;

        memset(&f, 0, sizeof f);
        nd_collect(hsfz_send, gw, &f);          /* reads refuse until hsfz_send is real */
        f.seq = seq++;
        f.sent_ms = mono_ms();
        w = nd_frame_encode(&f, buf, (int)sizeof buf);
        if (w > 0)
            (void)sendto(ufd, buf, (size_t)w, 0, (struct sockaddr *)&plugin, sizeof plugin);

        nap.tv_sec = 0; nap.tv_nsec = 100 * 1000 * 1000;   /* 10 Hz */
        nanosleep(&nap, 0);
    }

    close(ufd);
    return 0;
}
