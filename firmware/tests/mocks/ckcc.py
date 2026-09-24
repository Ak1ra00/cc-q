# Mock of the ckcc builtin module (bootloader call gate). Tests drive the PIN
# state through set_pin_state(); gate() plays it back the way the real
# bootloader answers pinattempt.py's roundtrip (see shared/pincodes.py).
import ustruct

PIN_ATTEMPT_FMT = 'Ii32si6I32si32si32si72s32s'

_state = {}


def reset(attempts_left=13, num_fails=0, is_blank=False, correct_pin=b'123456-789',
          brick=False):
    _state.clear()
    _state.update(attempts_left=attempts_left, num_fails=num_fails, is_blank=is_blank,
                  correct_pin=correct_pin, brick=brick, highwater=bytes(8),
                  upgrade_started=None)


reset()


def is_bricked():
    return _state['brick']


def gate(method_num, buf, arg2):
    if method_num == 21:      # get_highwater
        buf[:] = _state['highwater']
        return 0
    if method_num != 18:
        return 0

    fields = list(ustruct.unpack_from(PIN_ATTEMPT_FMT, buf))
    (magic, is_sec, pin, pin_len, delay_a, delay_r, num_fails, attempts_left,
     state_flags, priv, hmac, change_flags, old_pin, old_len, new_pin, new_len,
     secret, cached) = fields
    pin = pin[0:pin_len]

    if _state['brick']:
        return -105          # EPIN_I_AM_BRICK

    if arg2 == 0:             # setup
        flags = 0
        if _state['is_blank']:
            flags |= 0x02     # PA_IS_BLANK
        _write(buf, pin, _state['num_fails'], _state['attempts_left'], flags)
        return 0

    if arg2 == 2:              # login
        ok = pin == _state['correct_pin'] or (_state['is_blank'] and pin == b'')
        flags = 0x01 if ok else 0     # PA_SUCCESSFUL
        _write(buf, pin, _state['num_fails'], _state['attempts_left'], flags)
        return 0

    if arg2 == 7:              # firmware_upgrade
        start, length = ustruct.unpack_from('2I', secret)
        _state['upgrade_started'] = (start, length)
        return 0

    return 0


def _write(buf, pin, num_fails, attempts_left, state_flags):
    ustruct.pack_into(PIN_ATTEMPT_FMT, buf, 0,
                      0x2eaf6312, 0, pin, len(pin), 0, 0, num_fails, attempts_left,
                      state_flags, 0, bytes(32), 0, pin, len(pin), b'', 0,
                      bytes(72), bytes(32))


def oneway(method_num, arg):
    pass


def rng_bytes(n):
    return bytes(range(n % 256)) if n else b''
