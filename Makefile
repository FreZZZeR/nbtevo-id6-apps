# nbtevo-id6-apps — host build + tests (Apache-2.0).
# Host-only: no QNX, no car. Target (QNX) cross-build lives in native/build.sh (Stage D).

CC      ?= cc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Werror
NBTEVO_DIAG ?= $(HOME)/Documents/nbtevo-diag
GEN     := native/generated

.PHONY: test gen clean

# Regenerate the C DID table + parity vectors from nbtevo-diag's catalog (source of truth).
gen:
	python3 codegen/gen_dids.py --diag "$(NBTEVO_DIAG)" --out "$(GEN)"

# Codegen, then compile the decode core + parity test and run it.
test: gen
	$(CC) $(CFLAGS) -I"$(GEN)" -Inative/src \
		native/tests/test_decode.c native/src/decode.c -o native/test_decode -lm
	./native/test_decode

clean:
	rm -f native/test_decode $(GEN)/dids.h $(GEN)/test_vectors.h
