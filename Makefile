# nbtevo-id6-apps — host build + tests (Apache-2.0).
# Host-only: no QNX, no car. Target (QNX) cross-build lives in native/build.sh (Stage D).

CC      ?= cc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Werror
NBTEVO_DIAG ?= $(HOME)/Documents/nbtevo-diag
GEN     := native/generated
IDEMU_DIR ?=

.PHONY: test gen idemu clean

# Regenerate the C DID table + parity vectors from nbtevo-diag's catalog (source of truth).
gen:
	python3 codegen/gen_dids.py --diag "$(NBTEVO_DIAG)" --out "$(GEN)"

# Codegen, then compile + run the decode parity test and the Codes worker test.
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

# iDemu stage (Stage B, manual). iDemu boots the real id6 HMI but can NOT load our .so plugin
# and has no diagnostic transport (no HSFZ/DME) — it validates the HMI environment + MOST
# registration only, not real signals or our exact ABI. So it is not an automated gate; real
# validation is on the parked car (Stage C/E). Set IDEMU_DIR to run the presence smoke check.
idemu:
	@echo "iDemu: boots the real id6 HMI (environment + MOST registration)."
	@echo "It does NOT load our .so plugin and has no HSFZ/DME, so it is not an automated gate."
	@echo "Real signal + ABI validation happens on the parked car (Stage C/E)."
	@if [ -n "$(IDEMU_DIR)" ] && [ -d "$(IDEMU_DIR)" ]; then \
		echo "--- smoke check: python3 -m idemu list ---"; \
		( cd "$(IDEMU_DIR)" && python3 -m idemu list ) || echo "(iDemu present but not runnable here)"; \
	else \
		echo "Set IDEMU_DIR=/path/to/IDEmu to run the smoke check (python3 -m idemu list)."; \
	fi

clean:
	rm -f native/test_decode native/test_codes native/test_gauges native/test_xdrive \
		$(GEN)/dids.h $(GEN)/test_vectors.h
