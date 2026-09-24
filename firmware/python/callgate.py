# (c) Copyright 2018 by Coinkite Inc. This file is covered by license found in COPYING-CC.
#
# callgate.py - thin wrapper around the bootloader's call gate.
#
# Trimmed from the Coldcard firmware to the calls QUASAR uses.
#
import ckcc

def get_bl_version():
    # ('1.1.0', [('time', '...'), ('git', '...')])
    rv = bytearray(64)
    ln = ckcc.gate(0, rv, 0)
    ver, *args = str(rv[0:ln], 'utf8').split(' ')
    return ver, [tuple(i.split('=', 1)) for i in args]

def show_logout(dont_clear=0):
    # wipe memory and stop: 2 = restart afterwards, 3 = Q: power down
    ckcc.oneway(3, dont_clear)

def get_is_bricked():
    return ckcc.gate(5, None, 0) != 0

def get_highwater():
    # oldest firmware timestamp the bootloader will still accept (zeros = any)
    arg = bytearray(8)
    ckcc.gate(21, arg, 0)
    return bytes(arg)

# EOF
