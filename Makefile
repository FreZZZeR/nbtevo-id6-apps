# nbtevo-id6-apps — host build + tests (Apache-2.0).
# Host-only: no QNX, no car. The installable plugin/collector cross-build is Stage D (QNX SDP
# 6.5 + framework libs); `make shells` syntax-checks those shells against the ABI header.

CC      ?= cc
CXX     ?= c++
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Werror
NBTEVO_DIAG ?= $(HOME)/Documents/nbtevo-diag
GEN     := native/generated
IDEMU_DIR ?=

.PHONY: test gen shells idemu clean

# Regenerate the C DID table + parity vectors from nbtevo-diag's catalog (source of truth).
gen:
	python3 codegen/gen_dids.py --diag "$(NBTEVO_DIAG)" --out "$(GEN)"

# Codegen, then compile + run every host test (no car, no QNX), then syntax-check the shells.
test: gen
	$(CC) $(CFLAGS) -I"$(GEN)" -Inative/src \
		native/tests/test_decode.c native/src/decode.c -o native/test_decode -lm
	./native/test_decode
	$(CC) $(CFLAGS) -Inative/src \
		native/tests/test_codes.c native/src/codes.c -o native/test_codes
	./native/test_codes
	$(CC) $(CFLAGS) -I"$(GEN)" -Inative/src \
		native/tests/test_gauges.c native/src/gauges.c native/src/uds.c native/src/decode.c -o native/test_gauges -lm
	./native/test_gauges
	$(CC) $(CFLAGS) -Inative/src \
		native/tests/test_xdrive.c native/src/xdrive.c native/src/uds.c -o native/test_xdrive
	./native/test_xdrive
	$(CC) $(CFLAGS) -Inative/src \
		native/tests/test_frame.c native/src/frame.c -o native/test_frame
	./native/test_frame
	$(CC) $(CFLAGS) -Inative/src \
		native/tests/test_hmi_view.c native/src/hmi_view.c -o native/test_hmi_view
	./native/test_hmi_view
	$(CC) $(CFLAGS) -I"$(GEN)" -Inative/src \
		native/tests/test_collect.c native/src/collector.c native/src/gauges.c native/src/uds.c \
		native/src/decode.c native/src/codes.c native/src/xdrive.c -o native/test_collect -lm
	./native/test_collect
	@$(MAKE) --no-print-directory shells

# The HMI plugin (plugin.cpp) and the collector process (collector_main.c) only LINK in the
# Stage-D cross-build (framework libs + QNX). Syntax-check them here against the ABI header so
# typos surface without the toolchain. (plugin.cpp needs -Inative for "sdk/bmw_list.h".)
shells:
	$(CC) -std=c99 -Wall -Wextra -Werror -Inative/src -fsyntax-only native/src/collector_main.c
	$(CXX) -std=c++98 -Wall -Wextra -Inative -Inative/src -fsyntax-only native/src/plugin.cpp
	@echo "shells OK: collector_main.c + plugin.cpp syntax-check vs the ABI (link is Stage D)"

# IDEmu (herrbimmer/IDEmu) — Stage B, manual; upstream d03ecd84f363 checked 2026-11-20.
# Real QNX/id6 HMI on macOS or Linux/WSL2; use fake collector loopback frames for plugin UI.
# Main profiles: default 007_051_002 and limited 1803 (006_017_006, rotor/buttons).
# PR #16 d73b049 includes main d03ecd8; open/unmerged, mergeable/clean on 2026-11-20.
# Its registry adds 007_001_016, 007_030_001 and 007_039_008; all four 007 HMI boots checked.
# Exact HMI + companion hashes required; no full SWFL/profile for our 006_016_002.
# Neighbouring 006_017_006 does not validate our HMI/plugin ABI.
# Virtual CAN/replay/GPS/cluster/HUD do not supply HSFZ 6801. Real N57 signals, DSC and
# exact on-car plugin ABI remain Stage C. Set IDEMU_DIR for the CLI presence check only.
idemu:
	@echo "IDEmu: QNX/id6 HMI on macOS/Linux/WSL2; plugin UI checks use fake loopback frames."
	@echo "Main: default 007_051_002; limited 1803=006_017_006. Our 006_016_002 has no validated profile/ABI."
	@echo "PR #16 d73b049: open/unmerged, mergeable/clean (2026-11-20); adds 007_001_016/030_001/039_008."
	@echo "Virtual CAN/replay/GPS/cluster/HUD; no HSFZ 6801. Real N57/DSC + on-car ABI: Stage C."
	@if [ -n "$(IDEMU_DIR)" ] && [ -d "$(IDEMU_DIR)" ]; then \
		echo "--- smoke check: python3 -m idemu list ---"; \
		( cd "$(IDEMU_DIR)" && python3 -m idemu list ) || echo "(iDemu present but not runnable here)"; \
	else \
		echo "Set IDEMU_DIR=/path/to/IDEmu to run the smoke check (python3 -m idemu list)."; \
	fi

clean:
	rm -f native/test_decode native/test_codes native/test_gauges native/test_xdrive \
		native/test_frame native/test_hmi_view native/test_collect \
		$(GEN)/dids.h $(GEN)/test_vectors.h
