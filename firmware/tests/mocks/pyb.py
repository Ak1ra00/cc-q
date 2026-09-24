# Mock of pyb: nothing here is reached in the tests.
class SDCard:
    def power(self, on):
        pass


class Flash:
    def __init__(self, **k):
        raise OSError('no internal flash in tests')
