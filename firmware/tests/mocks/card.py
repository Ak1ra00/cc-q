# Mock of card.py: a virtual microSD with files kept in a plain dict, so
# system.py's file-picking and staging logic can be tested without real
# hardware or a real filesystem.
import io

slots = [{}, {}]        # slot A, slot B: name -> bytes
present = [True, False]


class CardMissing(RuntimeError):
    pass


def inserted():
    return (present[0], present[1])


class Card:
    def __init__(self, slot_b=False):
        self.slot_b = slot_b
        self.root = '/sdmock'

    def __enter__(self):
        if not present[1 if self.slot_b else 0]:
            raise CardMissing
        return self

    def __exit__(self, *a):
        return False

    def list(self, suffix):
        files = slots[1 if self.slot_b else 0]
        return sorted((n, len(b)) for n, b in files.items() if n.lower().endswith(suffix))


_orig_open = open


def open(path, mode='r'):
    assert path.startswith('/sdmock/')
    name = path[len('/sdmock/'):]
    slot_b = present[1] and name in slots[1] and name not in slots[0]
    files = slots[1 if slot_b else 0]
    if 'w' in mode:
        buf = io.BytesIO()
        orig_close = buf.close

        def close():
            files[name] = buf.getvalue()
            orig_close()
        buf.close = close
        return buf
    if name not in files:
        raise OSError('no such mock file: %s' % name)
    return io.BytesIO(files[name])


def dfu_parse(fd):
    # mock .dfu files are just the raw firmware image with an 8-byte prefix
    fd.seek(0)
    assert fd.read(8) == b'MOCKDFU!'
    size = len(fd.read())
    return 8, size
