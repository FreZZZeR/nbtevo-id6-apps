#!/bin/sh
# nbtevo-id6-apps — Stage-D cross-build (QNX 6.5 / armle-v7).
#
# Produces the two installable artifacts:
#   native/out/nbtevo_diag_plugin.so   the HMI plugin (loads into HmiMain3D)
#   native/out/nbtevo-collector        the collector process (speaks HSFZ/UDS, ships frames)
#
# Toolchain: luka-dev/qnx65-armv7-toolchain, image :4.9 (GCC 4.9.4). Use 4.9, NOT 8.5 — it
# shares the pre-GCC-5 C++ ABI with the OEM framework (built with g++ 4.4.2 / libstdc++ 6.0.13);
# GCC 8.5 would break the C++ ABI against libAppFramework/libCoreFramework.
# Build the image once:  (cd <toolchain> && ./host-scripts/qnx-run.sh build 4.9)
#
# The framework's bmw::* methods are resolved at dlopen into HmiMain3D (see sdk/bmw_list.h),
# so the plugin .so links with those symbols UNDEFINED — no framework libs needed to build.
# The C files are built with the QNX gcc (C linkage, matching plugin.cpp's extern "C"); the
# plugin is built with g++. Warnings only (host `make test` is the -Werror gate).
set -e
cd "$(dirname "$0")/.."                       # repo root (nbtevo-id6-apps)

TC=${TC:-$HOME/Documents/335d/repos/qnx65-armv7-toolchain/host-scripts/qnx-run.sh}
[ -x "$TC" ] || { echo "toolchain runner not found: $TC (set TC=/path/to/host-scripts/qnx-run.sh)"; exit 1; }

echo "[build] codegen native/generated/dids.h from the nbtevo-diag catalog"
python3 codegen/gen_dids.py --diag "${NBTEVO_DIAG:-$HOME/Documents/nbtevo-diag}" --out native/generated

echo "[build] cross-compile in the :4.9 image (QNX armle-v7, GCC 4.9.4)"
"$TC" -V4.9 sh -c '
  cd /src
  set -e
  GCC=arm-unknown-nto-qnx6.5.0eabi-gcc
  GXX=arm-unknown-nto-qnx6.5.0eabi-g++
  INC="-Inative/src -Inative/generated"
  CF="-O2 -fPIC -mno-unaligned-access -Wall -Wextra"
  mkdir -p native/out

  # C: frame/view/workers — C linkage (matches plugin.cpp extern "C")
  for c in frame hmi_view decode codes uds gauges xdrive collector; do
    $GCC -std=c99 $CF $INC -c native/src/$c.c -o native/out/$c.o
  done

  # C++: HMI plugin. bmw::* framework symbols stay undefined (dlopen-resolved in HmiMain3D).
  $GXX -std=c++11 -O2 -fPIC -mno-unaligned-access -Wall -Wextra -Inative $INC \
      -c native/src/plugin.cpp -o native/out/plugin.o

  # Plugin .so: plugin + the frame/view it calls. -shared allows the undefined framework syms.
  # -lsocket: QNX keeps BSD sockets (the plugin binds a loopback UDP) in libsocket, not libc.
  $GXX -shared -o native/out/nbtevo_diag_plugin.so \
      native/out/plugin.o native/out/frame.o native/out/hmi_view.o -lsocket

  # Collector process ELF: pure C, no framework. -lsocket for socket/sendto on QNX.
  $GCC -std=c99 $CF $INC native/src/collector_main.c \
      native/out/collector.o native/out/gauges.o native/out/uds.o native/out/decode.o \
      native/out/codes.o native/out/xdrive.o native/out/frame.o -o native/out/nbtevo-collector -lsocket

  echo "[build] artifacts:"
  ls -l native/out/nbtevo_diag_plugin.so native/out/nbtevo-collector
  file native/out/nbtevo_diag_plugin.so native/out/nbtevo-collector 2>/dev/null || true
'
echo "[build] done -> native/out/{nbtevo_diag_plugin.so, nbtevo-collector}"
