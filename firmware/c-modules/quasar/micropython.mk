# QUASAR game engine, compiled into the firmware as a MicroPython user C module.
# The sources live in ../game (portable C, shared with the desktop build);
# the hardware glue and Python bindings are in the board directory (modquasar.c).

QUASAR_DIR := $(USERMOD_DIR)

SRC_USERMOD += $(wildcard $(QUASAR_DIR)/game/*.c)
CFLAGS_USERMOD += -I$(QUASAR_DIR)/game
