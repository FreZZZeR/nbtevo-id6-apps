/* nbtevo-id6-apps — a model N57 DDE for host tests (Apache-2.0).
 *
 * Behaves like the engine ECU on the car: mode-01 and a direct 22 <did> are refused, and live
 * values exist only behind the dynamic DID F303. A 2C 01 define APPENDS to the slot (ISO 14229
 * allows it) and only 2C 03 empties it — so two signals read in a row come out right only if
 * the code clears before each define. The memory-address form (2C 02) is refused.
 */
#ifndef ND_FAKE_DDE_H
#define ND_FAKE_DDE_H

static unsigned short fd_src[8];   /* source DIDs currently defined into F303, in order */
static int fd_n;

/* The two raw bytes of a source DID this model has; 0 for a DID it lacks. */
static int fd_value(unsigned short src, unsigned char *d)
{
    unsigned short raw;
    switch (src) {
        case 0x461B: raw = 15000; break;   /* coolant  15000/100 - 100       =   50.00 C   */
        case 0x4517: raw = 16000; break;   /* oil      16000/100 - 100       =   60.00 C   */
        case 0x4846: raw = 2931;  break;   /* intake   2931/10 - 273.14      =   19.96 C   */
        case 0x4841: raw = 25122; break;   /* boost    25122 * 0.091554      = 2300.02 hPa */
        case 0x4CF0: raw = 32768; break;   /* ambient  32768 * 0.030518      = 1000.01 hPa */
        case 0x5955: raw = 6000;  break;   /* rpm      6000 / 2              = 3000        */
        case 0x48A5: raw = 24466; break;   /* torque   24466*0.114443 - 2500 =  299.96 Nm  */
        default: return 0;
    }
    d[0] = (unsigned char)(raw >> 8);
    d[1] = (unsigned char)(raw & 0xFF);
    return 1;
}

static int fake_dde(void *ctx, unsigned char ecu, const unsigned char *req, int reqlen,
                    unsigned char *resp, int maxresp)
{
    unsigned char d[2];
    int i, n = 3;
    (void)ctx; (void)maxresp;

    if (ecu == 0x12 && reqlen == 4 && req[0] == 0x2C && req[1] == 0x03) {             /* clear */
        fd_n = 0;
        resp[0] = 0x6C; resp[1] = 0x03; resp[2] = req[2]; resp[3] = req[3];
        return 4;
    }
    if (ecu == 0x12 && reqlen == 8 && req[0] == 0x2C && req[1] == 0x01                 /* define: appends */
        && req[2] == 0xF3 && req[3] == 0x03 && req[6] == 1 && req[7] == 2 && fd_n < 8
        && fd_value((unsigned short)((req[4] << 8) | req[5]), d)) {
        fd_src[fd_n++] = (unsigned short)((req[4] << 8) | req[5]);
        resp[0] = 0x6C; resp[1] = 0x01; resp[2] = 0xF3; resp[3] = 0x03;
        return 4;
    }
    if (ecu == 0x12 && reqlen == 3 && req[0] == 0x22 && req[1] == 0xF3 && req[2] == 0x03 && fd_n) {
        resp[0] = 0x62; resp[1] = 0xF3; resp[2] = 0x03;
        for (i = 0; i < fd_n; i++, n += 2)
            fd_value(fd_src[i], resp + n);
        return n;
    }
    resp[0] = 0x7F; resp[1] = (unsigned char)(reqlen ? req[0] : 0); resp[2] = 0x31;   /* requestOutOfRange */
    return 3;
}

#endif /* ND_FAKE_DDE_H */
