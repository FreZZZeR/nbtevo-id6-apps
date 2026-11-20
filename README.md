# nbtevo-id6-apps

Native **BMW NBT EVO (iDrive6 / QNX)** apps that run **on the head unit** — fault codes,
live engine/vehicle gauges (incl. the "sport" numbers: boost, torque, power) and xDrive
control — with **no OBD adapter and no laptop**. The head unit already sits on the car's
diagnostic network; these apps read it directly.

This is the **on-car layer** of a two-layer product. The **host layer** —
[`nbtevo-diag`](https://github.com/FreZZZeR/nbtevo-diag) + evocode, our delivery toolchain —
owns the **signal catalog** (the single source of truth), discovers and validates the real
signals on the car, and deploys these apps. The native collector's DID table is
**code-generated from that catalog**, so the two never drift.

> **Status: Stage A (host-side, in progress).** The logic builds and is tested on a normal
> machine with no QNX and no car. The installable QNX plugins come later (Stage D), gated on a
> QNX SDP 6.5 toolchain and the two framework libraries pulled from the unit.
>
> All three collectors (Codes, Gauges, xDrive) and the HMI-plugin skeleton are in and
> host-tested. The plugin never does diagnostics itself — a separate **collector process**
> speaks HSFZ/UDS and ships decoded **frames over loopback UDP** to the plugin (the proven
> ID6_apps split), so `HmiMain3D` never blocks on the network. Only the cross-build is Stage D.

## Architecture

| Layer | Where | What |
|---|---|---|
| Host (Python: `nbtevo-diag` + evocode) | laptop / ENET | catalog = source of truth; discover + validate DIDs; **codegen** the C table; deploy + recover over SSH |
| On-car (this repo, C / C++ QNX) | head unit | collector workers + HMI plugins that **consume** the generated catalog |

Planned apps, mapped from `nbtevo-diag`:

| App | From | Reads |
|---|---|---|
| **Codes** | `dtc.py` | module fault memory (`19 02 FF`) — parser + collector built & host-tested (`native/src/codes.c`); real HSFZ transport is Stage C/D |
| **Gauges / Sport** | `signals.py` + probe | boost, power, torque, temps, pressures, shift-light — collector built & host-tested (`native/src/gauges.c`: decode + plausibility over the seam). Mode-01 where the engine answers it; on the N57, where it is dead, the DDE's own DIDs through a dynamic DID (`2C 03` clear, `2C 01` define F303, `22 F303`). Real signals Stage C |
| **xDrive** | `control.py` | temporary RWD with a software interlock — fuse + set-mode ported & host-tested (`native/src/xdrive.c`); DSC variant/write validated on-car (Stage C) |
| **HMI plugin** | ID6_apps ABI (MIT) | iDrive menu surfacing the three apps — shell (`native/src/plugin.cpp`) + collector process (`native/src/collector_main.c`), with the host-tested `frame`/`hmi_view`/`collector` backbone; cross-build is Stage D |

## Build & test (host — no car, no QNX)

```bash
make test
```

`make` runs `codegen/gen_dids.py` against your `nbtevo-diag` checkout
(`NBTEVO_DIAG=../nbtevo-diag` by default) to generate `native/generated/dids.h` plus parity
vectors, then compiles the C decode core and asserts it decodes **every signal identically to
the Python catalog**. That is the single-source-of-truth guarantee, checked on every build.
`make test` then runs the rest of the host suite — Codes, Gauges, xDrive, the collector→frame
pipeline, the frame codec and the HMI formatting — and finishes with `make shells`, which
syntax-checks the Stage-D plugin and collector shells against the ABI header.

## The two build systems

- **Host** (now): `cc` / clang, for logic + tests. No QNX, no frameworks, no car.
- **Target** (Stage D): QNX SDP 6.5 + `qcc -V4.4.2,gcc_ntoarmv7le` (ARMv7, 32-bit) on a Linux
  x86 host, linking `libAppFramework.A.so` + `libCoreFramework.A.so` pulled from the unit —
  produces the installable `.so` plugins + worker ELFs.

## Testing tiers

1. **Local (here):** host compile + fakes + decode parity (`make test`). Covers all logic.
2. **iDemu:** on its live validated stand (`007_051_002`) it runs the real id6 HMI and **can
   host our HMI plugin** (render + rotary/touch), fed by loopback frames from a fake/stub
   collector. Caveats: that stand is pinned to `007_051_002` (our head is `006_016_002`, so the
   exact `.so` load is confirmed only on the car) and it has **no** diagnostic transport
   (no HSFZ/DME) — real N57 signals + the DSC variant are on-car.
3. **On-car (parked):** the only place the exact N18032I ABI and real N57 DIDs are confirmed;
   SSH recovery pre-staged.

## License

**Apache-2.0** (see `LICENSE`). The reverse-engineered HMI-plugin ABI header
`native/sdk/bmw_list.h` is vendored from
[ID6_apps_prerelease](https://github.com/inotbarnacle/ID6_apps_prerelease) (MIT); its notice
is kept in `NOTICE` and in the file header. The plugin shell and collector are our own
Apache-2.0 code. Our xDrive interlock is independent (not ID6's idrive7-derived `controls.c`).
See `NOTICE`.

## Development guardrail

Committed git hooks block publishing private data and run the tests before every push.
Activate once per clone: `git config core.hooksPath hooks` (details in `hooks/README.md`).
