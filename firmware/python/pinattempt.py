# (c) Copyright 2018 by Coinkite Inc. This file is covered by license found in COPYING-CC.
#
# pinattempt.py - talk to the bootloader about the main PIN.
#
# Trimmed from pincodes.py in the Coldcard firmware: QUASAR keeps only what it
# needs to install firmware -- read the PIN state, log in, and hand a staged
# image to the bootloader. The struct layout and round trip are unchanged.
#
import ustruct, ckcc

MAX_PIN_LEN = const(32)
AE_SECRET_LEN = const(72)

PA_MAGIC_V2 = const(0x2eaf6312)

# state_flags
PA_SUCCESSFUL = const(0x01)
PA_IS_BLANK = const(0x02)
PA_ZERO_SECRET = const(0x10)

# change_flags
CHANGE_FIRMWARE = const(0x040)

PA_ERROR_CODES = {
    -100: "HMAC_FAIL",
    -101: "HMAC_REQUIRED",
    -102: "BAD_MAGIC",
    -103: "RANGE_ERR",
    -104: "BAD_REQUEST",
    -105: "I_AM_BRICK",
    -106: "AE_FAIL",
    -107: "MUST_WAIT",
    -108: "PIN_REQUIRED",
    -109: "WRONG_SUCCESS",
    -110: "OLD_ATTEMPT",
    -111: "AUTH_MISMATCH",
    -112: "AUTH_FAIL",
    -113: "OLD_AUTH_FAIL",
    -114: "PRIMARY_ONLY",
    -115: "SE2_FAIL",
}

EPIN_I_AM_BRICK = const(-105)
EPIN_AUTH_FAIL = const(-112)

# uint32_t magic; int is_secondary; char pin[32]; int pin_len; uint32_t delay_achieved,
# delay_required, num_fails, attempts_left, state_flags, private_state; uint8_t hmac[32];
# int change_flags; char old_pin[32]; int old_pin_len; char new_pin[32]; int new_pin_len;
# uint8_t secret[72]; uint8_t cached_main_pin[32]
PIN_ATTEMPT_FMT = 'Ii32si6I32si32si32si72s32s'
PIN_ATTEMPT_SIZE = const(248 + 32)


def retry_ae_fail(*args):
    err = ckcc.gate(*args)
    if err == -106:     # AE_FAIL: serial noise with the secure element, one retry
        err = ckcc.gate(*args)
    return err


class BootloaderError(RuntimeError):
    pass


class PinAttempt:
    def __init__(self):
        self.pin = b''
        self.delay_achieved = 0         # mk4+: trick pin arg
        self.delay_required = 0         # mk4+: trick pin flags that are not hidden
        self.num_fails = 0
        self.attempts_left = 0
        self.state_flags = 0
        self.private_state = 0
        self.hmac = bytes(32)
        self.cached_main_pin = bytes(32)

    def marshal(self, msg, fw_upgrade=None):
        change_flags = 0
        new_secret = bytes(AE_SECRET_LEN)
        if fw_upgrade:
            change_flags = CHANGE_FIRMWARE
            new_secret = ustruct.pack('2I', *fw_upgrade) + bytes(AE_SECRET_LEN - 8)

        ustruct.pack_into(PIN_ATTEMPT_FMT, msg, 0,
                          PA_MAGIC_V2, 0,
                          self.pin, len(self.pin),
                          self.delay_achieved, self.delay_required,
                          self.num_fails, self.attempts_left,
                          self.state_flags, self.private_state,
                          self.hmac, change_flags,
                          self.pin, len(self.pin),      # old_pin (unused here)
                          b'', 0,                       # new_pin
                          new_secret, self.cached_main_pin)

    def unmarshal(self, msg):
        (_magic, _sec, pin, pin_len, self.delay_achieved, self.delay_required,
         self.num_fails, self.attempts_left, self.state_flags, self.private_state,
         self.hmac, _chg, _old, _old_len, _new, _new_len, secret,
         self.cached_main_pin) = ustruct.unpack_from(PIN_ATTEMPT_FMT, msg)
        self.pin = pin[0:pin_len]
        return secret

    def roundtrip(self, method_num, **kws):
        buf = bytearray(PIN_ATTEMPT_SIZE)
        self.marshal(buf, **kws)
        err = retry_ae_fail(18, buf, method_num)
        if err <= -100:
            if err == EPIN_I_AM_BRICK:
                # nothing more can be done with this chip
                raise BootloaderError('I_AM_BRICK', err)
            raise BootloaderError(PA_ERROR_CODES.get(err, 'ERR%d' % err), err)
        elif err:
            raise RuntimeError(err)
        return self.unmarshal(buf)

    def is_blank(self):
        return bool(self.state_flags & PA_IS_BLANK)

    def is_successful(self):
        return bool(self.state_flags & PA_SUCCESSFUL)

    def setup(self, pin):
        # reset the struct and read counters for this PIN attempt
        self.pin = pin
        self.hmac = bytes(32)
        self.roundtrip(0)
        return self.state_flags

    def login(self):
        # returns True if the PIN was right; the bootloader counts failures itself
        self.roundtrip(2)
        return self.is_successful()

    def visible_trick_flags(self):
        # the few trick-PIN flags the bootloader lets firmware see
        return self.delay_required

    def firmware_upgrade(self, start, length):
        # bootloader checks the signature of the image staged in PSRAM, records it
        # with the secure element, copies it to flash and reboots. Not reached.
        self.roundtrip(7, fw_upgrade=(start, length))

# EOF
