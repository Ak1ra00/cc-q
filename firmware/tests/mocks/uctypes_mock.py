# A fake for the parts of uctypes that touch raw memory addresses (PSRAM, the
# firmware header in flash), so tests run the exact same code on ordinary
# bytearrays instead of real device memory. Installed into sys.modules as
# 'uctypes' before the module under test is imported; every other uctypes
# name tests might need is re-exported from the real (builtin) module.
import uctypes as _real

bytearray_at = _real.bytearray_at
bytes_at = _real.bytes_at

# address -> bytearray, for tests to set up and inspect
regions = {}


def bytearray_at(addr, length):        # noqa: F811 - intentional override
    return regions.setdefault(addr, bytearray(length))


def bytes_at(addr, length):            # noqa: F811 - intentional override
    return bytes(regions.setdefault(addr, bytearray(length))[0:length])


for _name in dir(_real):
    if _name not in globals() and not _name.startswith('_'):
        globals()[_name] = getattr(_real, _name)
