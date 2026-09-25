#!/usr/bin/env bash
#
# Runs the QUASAR Python tests (test_system.py, test_main.py) against the real
# MicroPython interpreter from our pinned fork, built for the host (unix port)
# instead of the STM32 target.
#
# Two host-only quirks, neither of which affects the real firmware:
#  - MICROPY_NLR_SETJMP=1: this MicroPython's default x86-64 exception unwinder
#    (py/nlrx64.c) segfaults on this machine's glibc/GCC combo the moment an
#    exception crosses a function call boundary. The ARM build the device
#    actually runs uses a different, unaffected unwinder (py/nlrthumb.c); this
#    flag only changes the host test binary.
#  - -X heapsize=32M: the default unix-port heap is a few hundred KB, too
#    small for the 4MB PSRAM-sized buffer system.py stages firmware into.
#    The real device has actual 8MB PSRAM behind that address, no allocation
#    needed there.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UNIX="$ROOT/external/micropython/ports/unix"
MP="$UNIX/micropython"

if [ ! -x "$MP" ]; then
    echo "== building the unix port for testing (one-off)"
    git -C "$ROOT" submodule update --init external/micropython
    make -C "$UNIX" -j"$(nproc 2>/dev/null || echo 4)" \
        CFLAGS_EXTRA=-DMICROPY_NLR_SETJMP=1 CWARN="-Wall -Wno-error" \
        MICROPY_PY_BTREE=0 MICROPY_PY_USSL=0 MICROPY_PY_FFI=0 \
        MICROPY_PY_AXTLS=0 MICROPY_SSL_AXTLS=0 MICROPY_PY_THREAD=0 \
        FROZEN_MANIFEST= >/dev/null
fi

"$MP" -X heapsize=32M "$ROOT/firmware/tests/test_system.py"
exec "$MP" -X heapsize=32M "$ROOT/firmware/tests/test_main.py"
