# Mock of qhw: battery and power.
level = 3
volts = None
backlights = []


class PowerOff(BaseException):
    pass


def display_setup():
    return 'spi'


def backlight(v):
    backlights.append(v)


def battery_level():
    return level


def battery_volts():
    return volts


def power_off():
    raise PowerOff()
