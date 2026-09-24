# Mock of machine: just enough for card.py and main.py.
class Reset(BaseException):
    pass


def reset():
    raise Reset()


class Pin:
    IN = OUT = ALT = PULL_UP = PULL_DOWN = 0

    def __init__(self, *a, **k):
        pass

    def __call__(self, *a):
        return 1

    def on(self):
        pass

    def off(self):
        pass
