/* nbtevo-id6-apps — decode parity test (Apache-2.0).
 *
 * Asserts the C decode core produces the SAME value as the Python catalog for every signal.
 * The expected values in test_vectors.h are computed by nbtevo-diag's own Dec during codegen,
 * so this proves the single source of truth holds across the Python->C boundary. No car, no QNX.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "decode.h"
#include "dids.h"
#include "test_vectors.h"

static const nd_sig *find_sig(const char *key)
{
    size_t i;
    for (i = 0; i < ND_NSIGS; i++)
        if (strcmp(ND_SIGS[i].key, key) == 0)
            return &ND_SIGS[i];
    return NULL;
}

int main(void)
{
    int fails = 0;
    size_t i;

    for (i = 0; i < ND_NVEC; i++) {
        const nd_sig *s = find_sig(ND_VEC[i].key);
        double v;
        if (s == NULL) {
            printf("FAIL %-14s: not in ND_SIGS\n", ND_VEC[i].key);
            fails++;
            continue;
        }
        if (!nd_value(s, ND_VEC[i].bytes, ND_VEC[i].len, &v)) {
            printf("FAIL %-14s: nd_value refused len=%d\n", ND_VEC[i].key, ND_VEC[i].len);
            fails++;
            continue;
        }
        if (fabs(v - ND_VEC[i].expected) > 1e-9) {
            printf("FAIL %-14s: C=%.12g  expected=%.12g\n", ND_VEC[i].key, v, ND_VEC[i].expected);
            fails++;
        }
    }

    /* computed "sport displays" math parity (values mirror signals.py selftest) */
    if (fabs(nd_boost_bar(230, 100) - 1.3) > 1e-9) { printf("FAIL boost_bar\n"); fails++; }
    if (fabs(nd_torque_nm(600, 50) - 300.0) > 1e-9) { printf("FAIL torque_nm\n"); fails++; }
    if (fabs(nd_power_kw(300, 4000) - 125.66370614359172) > 1e-6) { printf("FAIL power_kw\n"); fails++; }

    if (fails) {
        printf("\n%d FAILURE(S)\n", fails);
        return 1;
    }
    printf("decode parity OK: %zu signals in table, %zu vectors, computed math OK\n",
           (size_t)ND_NSIGS, (size_t)ND_NVEC);
    return 0;
}
