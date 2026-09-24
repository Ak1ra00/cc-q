# QUASAR: the Python frozen into the firmware. The game itself is C (the
# quasar user module); this is boot, the system screens, and the few pieces of
# the stock firmware they need to install other firmware.
freeze('$(BOARD_DIR)/python', (
    'main.py',
    'ui.py',
    'system.py',
    'qhw.py',
    'card.py',
    'callgate.py',
    'pinattempt.py',
    'sigheader.py',
))
