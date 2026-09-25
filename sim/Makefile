# QUASAR desktop build: a headless runner used for tests and screenshots, and
# the tests for TETRIS and the home screen. The game code is the same C that
# runs on the Coldcard Q.
#
#   make            the headless runner
#   make test       the TETRIS and PAC-MAN rules and home screen tests, and a short fuzz
#   make test SAN=1 the same, with AddressSanitizer and UndefinedBehaviorSanitizer
#   make fuzz       the long randomised run (FRAMES=..., SEED=...; best with SAN=1)

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

pac_test: pac_test.c $(GAME)/pac.c $(GAME)/pac_gen.c $(GAME)/gmath.c $(HDR)
	$(CC) $(CFLAGS) -o $@ pac_test.c $(GAME)/pac_gen.c $(GAME)/gmath.c -lm

arcade_test: arcade_test.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ arcade_test.c $(SRC) -lm

fuzz_test: fuzz_test.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ fuzz_test.c $(SRC) -lm

FRAMES ?= 1000000
SEED ?= 1

test: tetris_test pac_test arcade_test fuzz_test
	./tetris_test
	./pac_test
	./arcade_test
	./fuzz_test 60000 $(SEED)

fuzz: fuzz_test
	./fuzz_test $(FRAMES) $(SEED)

clean:
	rm -f quasar_headless tetris_test pac_test arcade_test fuzz_test

.PHONY: all test fuzz clean
