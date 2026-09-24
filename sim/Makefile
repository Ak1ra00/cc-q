# QUASAR desktop build: a headless runner used for tests and screenshots.
# The game code is the same C that runs on the Coldcard Q.

GAME = ../game
SRC = $(wildcard $(GAME)/*.c)
CFLAGS = -std=gnu99 -O2 -g -Wall -Werror -Wpointer-arith -Wdouble-promotion \
         -Wfloat-conversion -fsingle-precision-constant -I$(GAME)

all: quasar_headless

quasar_headless: headless.c $(SRC) $(wildcard $(GAME)/*.h)
	$(CC) $(CFLAGS) -o $@ headless.c $(SRC) -lm

clean:
	rm -f quasar_headless

.PHONY: all clean
