#!/usr/bin/env python3
"""Code-generate the C DID table + decode parity vectors from nbtevo-diag's catalog.

The signal catalog in nbtevo-diag (nbtevo_diag/signals.py) is the single source of truth.
Every signal whose decode is a declarative `Dec` is emitted into `dids.h`; the same Dec also
computes the expected value for a sample, written to `test_vectors.h`. So the C decoder is
checked against the catalog itself, not a hand-copied table — change a scale in signals.py,
re-run, and the native collector + its test follow. No drift.

Usage: gen_dids.py --diag <path to nbtevo-diag> --out <dir for generated headers>
"""
import argparse
import os
import sys

# Exercises the signed-negative path at width 2 (0x8012), and 0x80 at width 1.
SAMPLE = bytes([0x80, 0x12])


def load_catalog(diag):
    pkg = os.path.join(diag, "nbtevo_diag")
    if not os.path.isfile(os.path.join(pkg, "signals.py")):
        sys.exit("gen_dids: signals.py not found under %s — pass --diag <nbtevo-diag>" % diag)
    sys.path.insert(0, pkg)
    import signals  # noqa: E402  (pure module: no evocode/car deps)
    return signals


def rows_and_vectors(signals):
    rows, vecs = [], []
    for s in signals.ALL:
        d = s.decode
        if not isinstance(d, signals.Dec):
            continue  # computed / probe signals have no single-DID decode
        service = 0x01 if s.service == signals.OBD else 0x22
        has_lo, lo = (1, float(s.lo)) if s.lo is not None else (0, 0.0)
        has_hi, hi = (1, float(s.hi)) if s.hi is not None else (0, 0.0)
        rows.append(
            '    { "%s", 0x%02X, 0x%02X, 0x%04X, %d, %d, %d, %d, %r, %d, %r, %d, %r },'
            % (s.key, s.ecu, service, s.addr, d.width, 1 if d.signed else 0,
               d.num, d.den, float(d.offset), has_lo, lo, has_hi, hi)
        )
        sample = SAMPLE[: d.width]
        expected = d(sample)
        barr = ", ".join("0x%02X" % b for b in sample)
        vecs.append('    { "%s", { %s }, %d, %r },' % (s.key, barr, len(sample), float(expected)))
    return rows, vecs


def write_header(path, guard, body):
    with open(path, "w") as f:
        f.write("/* GENERATED from nbtevo-diag signals.py by codegen/gen_dids.py — DO NOT EDIT. */\n")
        f.write("#ifndef %s\n#define %s\n\n%s\n#endif /* %s */\n" % (guard, guard, body, guard))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--diag", default=os.path.expanduser("~/Documents/nbtevo-diag"))
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    signals = load_catalog(a.diag)
    rows, vecs = rows_and_vectors(signals)
    os.makedirs(a.out, exist_ok=True)

    write_header(
        os.path.join(a.out, "dids.h"), "ND_DIDS_H",
        '#include "decode.h"\n\n'
        "static const nd_sig ND_SIGS[] = {\n" + "\n".join(rows) + "\n};\n"
        "#define ND_NSIGS (sizeof(ND_SIGS) / sizeof(ND_SIGS[0]))\n",
    )
    write_header(
        os.path.join(a.out, "test_vectors.h"), "ND_TEST_VECTORS_H",
        "static const struct nd_vec {\n"
        "    const char *key; unsigned char bytes[4]; int len; double expected;\n"
        "} ND_VEC[] = {\n" + "\n".join(vecs) + "\n};\n"
        "#define ND_NVEC (sizeof(ND_VEC) / sizeof(ND_VEC[0]))\n",
    )
    print("gen_dids: %d signals, %d vectors -> %s" % (len(rows), len(vecs), a.out))


if __name__ == "__main__":
    main()
