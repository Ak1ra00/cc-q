#!/usr/bin/env python3
#
# check-repro.py - compare a locally built QUASAR .dfu with the published one.
#
# Every byte must match except what signing changes by design on each build:
# the timestamp and the signature inside the firmware header.
#
#   python3 firmware/tools/check-repro.py release/quasar-1.0.0-q1.dfu published.dfu
#
import struct, sys

HDR = 0x4000 - 128      # FW_HEADER_OFFSET: header position inside the firmware image
TIMESTAMP = range(HDR + 4, HDR + 12)
SIGNATURE = range(HDR + 64, HDR + 128)


def firmware(path):
    data = open(path, 'rb').read()
    if data[:5] != b'DfuSe':
        sys.exit('%s: not a DFU file' % path)
    # DfuSe prefix (11 bytes), target prefix (274), then the element's address and size
    addr, size = struct.unpack_from('<2I', data, 11 + 274)
    return addr, data[11 + 274 + 8:11 + 274 + 8 + size]


(a_addr, a), (b_addr, b) = firmware(sys.argv[1]), firmware(sys.argv[2])
skip = set(TIMESTAMP) | set(SIGNATURE)
bad = [i for i in range(max(len(a), len(b)))
       if i not in skip and (i >= len(a) or i >= len(b) or a[i] != b[i])]
if a_addr != b_addr or bad:
    print('DIFFERENT: %d byte(s) differ outside the timestamp and signature%s'
          % (len(bad), (', first at firmware offset 0x%x' % bad[0]) if bad else ''))
    sys.exit(1)
print('IDENTICAL apart from the build timestamp and signature (%d firmware bytes compared)' % len(a))
