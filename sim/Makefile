# QUASAR desktop build: a headless runner used for tests and screenshots, and
# the tests for TETRIS and the home screen. The game code is the same C that
# runs on the Coldcard Q.
#
#   make            the headless runner
#   make test       the TETRIS rules and home screen tests
#   make test SAN=1 the same, with AddressSanitizer and UndefinedBehaviorSanitizer

GAME = ../game
SRC = $(wildcard $(GAME)/*.c)
HDR = $(wildcard $(GAME)/*.h)
CFLAGS = -std=gnu99 -O2 -g -Wall -Werror -Wpointer-arith -Wdouble-promotion \
         -Wfloat-conversion -fsingle-precision-constant -I$(GAME)
ifdef SAN
CFLAGS += -O1 -fsanitize=address,undefined -fno-sanitize-recover
endif

all: quasar_headless

quasar_headless: headless.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ headless.c $(SRC) -lm

tetris_test: tetris_test.c $(GAME)/tetris.c $(GAME)/gmath.c $(HDR)
	$(CC) $(CFLAGS) -o $@ tetris_test.c $(GAME)/gmath.c -lm

arcade_test: arcade_test.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ arcade_test.c $(SRC) -lm

test: tetris_test arcade_test
	./tetris_test
	./arcade_test

clean:
	rm -f quasar_headless tetris_test arcade_test

.PHONY: all test clean
