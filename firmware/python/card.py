# (c) Copyright 2018 by Coinkite Inc. This file is covered by license found in COPYING-CC.
#
# card.py - microSD access on the Q's two slots, and DFU parsing.
#
# Derived from files.py in the Coldcard firmware (CardSlot, dfu_parse); only the
# reading side is kept.
#
import pyb, os, utime
from machine import Pin

_setup_done = False
_mux = None
_det_a = None
_det_b = None
_led_a = None
_led_b = None

def setup():
    global _setup_done, _mux, _det_a, _det_b, _led_a, _led_b
    if _setup_done:
        return
    _mux = Pin('SD_MUX', Pin.OUT, value=0)
    _det_a = Pin('SD_DETECT', Pin.IN, pull=Pin.PULL_UP)
    _det_b = Pin('SD_DETECT2', Pin.IN, pull=Pin.PULL_UP)
    _led_a = Pin('SD_ACTIVE', Pin.OUT)
    _led_b = Pin('SD_ACTIVE2', Pin.OUT)
    _setup_done = True

def inserted():
    # (slot A present, slot B present); detect lines go low with a card in
    setup()
    return (_det_a() == 0, _det_b() == 0)

class CardMissing(RuntimeError):
    pass

class Card:
    # with Card(slot_b) as c: ... c.root is the mount point
    def __init__(self, slot_b=False):
        setup()
        self.slot_b = slot_b
        self.root = None
        self.led = _led_b if slot_b else _led_a

    def __enter__(self):
        a, b = inserted()
        if not (b if self.slot_b else a):
            raise CardMissing
        _mux(1 if self.slot_b else 0)        # top slot = A
        self.led.on()
        utime.sleep_ms(60)                  # let the detect switch settle
        sd = pyb.SDCard()
        try:
            try:
                os.statvfs('/sd')
            except OSError:
                sd.power(1)
                os.mount(sd, '/sd', readonly=1, mkfs=0)
                os.statvfs('/sd')
        except OSError:
            self._release()
            raise CardMissing
        self.root = '/sd'
        return self

    def __exit__(self, *a):
        self._release()
        return False

    def _release(self):
        self.led.off()
        try:
            os.umount('/sd')
        except Exception:
            pass
        try:
            pyb.SDCard().power(0)
        except Exception:
            pass
        _mux(0)
        self.root = None

    def list(self, suffix):
        # files at the top of the card with this suffix: [(name, size)]
        out = []
        for ent in os.ilistdir(self.root):
            name, kind = ent[0], ent[1]
            if kind & 0x4000:
                continue
            if name.startswith('.') or not name.lower().endswith(suffix):
                continue
            try:
                size = os.stat(self.root + '/' + name)[6]
            except OSError:
                continue
            out.append((name, size))
        out.sort()
        return out

def dfu_parse(fd):
    # find start/length of the firmware inside a DfuSe file (first element only)
    import struct
    fd.seek(0)
    prefix = fd.read(11)
    sig, _ver, _size, targets = struct.unpack('<5sBIB', prefix)
    if sig != b'DfuSe':
        raise ValueError('not a DFU file')
    for _ in range(targets):
        t = fd.read(274)
        tsig, _alt, _named, _name, _tsize, elements = struct.unpack('<6sBI255s2I', t)
        for _ in range(elements):
            addr, size = struct.unpack('<2I', fd.read(8))
            if addr < 0x8008000:
                raise ValueError('bad address')
            return fd.tell(), size
    raise ValueError('no firmware in file')

# EOF
