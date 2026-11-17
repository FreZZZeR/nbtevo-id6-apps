#!/bin/sh
# Local verification stage for nbtevo-id6-apps (run by pre-push).
# Codegen from nbtevo-diag's catalog + compile + decode parity. Host-only, no QNX, no car.
exec make test
