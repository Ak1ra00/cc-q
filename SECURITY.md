# Security

## What this firmware can and can't touch

QUASAR has no Bitcoin code in it anywhere — no wallet, no seed words, no BIP-32/39, no PSBT, no
signing. None of that code was stripped out at the last minute; it was never built in. The games
(`game/`) are self-contained C with no knowledge of the secure elements at all, and the Python around
them (`firmware/python/`) only does three things: start the games, save their settings, scores and
saved games to `/flash`, and run the SYSTEM menu. Even a full compromise of this firmware's own code cannot expose
a seed phrase, because no seed phrase is ever loaded, held, or reachable from any of it.

There is exactly one place this firmware talks to the secure elements: **installing other
firmware**, from the SYSTEM menu. That path exists so you can get back to Coinkite's official
firmware, and it works the same way the stock Coldcard's firmware-upgrade feature does — because
it's the same code, essentially unmodified (`firmware/python/{pinattempt,callgate,sigheader,card}.py`
are Coinkite's own files, trimmed to only what this uses; `firmware/python/system.py` reimplements
their `actions.py`/`auth.py`/`login.py` upgrade flow against that same bootloader call-gate
protocol). It stages the new firmware image in PSRAM — ordinary volatile memory, wiped by any power
loss or reset — and hands control to the bootloader, which does the actual signature verification
and flash write itself, entirely outside this firmware's control. This firmware cannot bypass that
check; nothing running as "firmware" ever could, by design.

**Never enter your real wallet PIN into custom firmware you have not reviewed yourself — on this
project or anyone else's.** Nothing here misuses it, but that's a claim you should be able to verify
by reading the code, not one you should take on faith from a README. If you want to try this on
hardware that has ever held real funds, set a fresh PIN first, or better, use a spare Coldcard.

**Only ever enter your main PIN on the INSTALL FIRMWARE screen, never a trick PIN.** Trick PINs
keep doing whatever you set them up to do, and an install attempted after a trick-PIN login makes
the bootloader wipe the seed (`pin_firmware_upgrade()` in Coinkite's
`stm32/mk4-bootloader/pins.c`), exactly as it would under the official firmware.

## The bootloader warning is not a bug

Coinkite's bootloader only trusts firmware signed with their own factory key, which nobody outside
Coinkite can do. Every other build — this one included — has to be signed with the **public
developer key** instead (`firmware/keys/00.pem`; its private half is committed on purpose, same as
in Coinkite's own repository, because it is meant for exactly this: anyone building their own
firmware signs with it). The bootloader recognizes that and, on every single boot, shows its
unsigned-firmware warning with a progress bar for about 25 seconds (100 × 250 ms in
`warn_fishy_firmware()`, `stm32/mk4-bootloader/verify.c`) before QUASAR starts. That check runs
from the bootloader's own code, before this firmware's code ever starts, so nothing in this
repository can change, skip, or theme it. It's the honest price of running something Coinkite
didn't build.

About the genuine-firmware light: per the same bootloader source, it reflects whether the flash
still matches the image you last approved with your PIN, not who signed it. An install through
the normal PIN-checked upgrade records QUASAR's checksum in the secure element, so the light may
well be green while QUASAR runs; the 25-second warning is what marks it as unofficial. A red light
means the flash changed outside such an install.

Keep an official firmware `.dfu` on a spare microSD card before you install this (see the README).
Three things the bootloader source makes worth knowing:

- Its microSD **recovery mode** only starts when the firmware in flash is corrupt or missing, and
  it only accepts the exact image you were installing. If power is lost in the middle of an
  install, put that same `.dfu` on the card.
- A firmware that is validly signed but fails early would not trigger recovery at all. That is why
  QUASAR brings the display up first and checks for **S** held at power-on before it touches
  anything else: SYSTEM → INSTALL FIRMWARE has to stay reachable. The save files are only read
  after that check, and the code that reads them is tested against thousands of damaged files
  (`sim/fuzz_test.c`).
- The bootloader's downgrade limit (the "highwater" timestamp, kept in one-time-programmable flash)
  only rises when the running firmware asks it to record one (call gate 21, `dispatch.c`). QUASAR
  reads that limit, to warn before an install the bootloader would refuse, but has no code that
  records one, so running it never stops you going back to an official release the bootloader
  accepted before.

## Verifying what you're installing

The `.dfu` in [`release/`](release/) is built by [`firmware/build.sh`](firmware/build.sh) from
exactly the source in this repository — nothing else goes into it. To check that for yourself,
build it with the same toolchain (Arm GNU Toolchain 13.3.Rel1) and compare:

```
git clone https://github.com/Ak1ra00/cc-Q.git && cd cc-Q
git submodule update --init external/micropython
cp release/quasar-*.dfu /tmp/published.dfu
./firmware/build.sh
python3 firmware/tools/check-repro.py release/quasar-*.dfu /tmp/published.dfu
```

The two files will not have the same `sha256sum`: each signing stamps the current time into the
firmware header and makes a fresh ECDSA signature. `check-repro.py` compares every other byte of
the firmware, the same way Coinkite's own `make check-repro` sets the header aside.

The sign step of `build.sh` prints the header it embeds (version, timestamp, the hardware it's
marked compatible with) and verifies the signature it just made before writing the `.dfu`, so a
corrupted or mismatched build fails loudly there rather than silently on the device.

## Reporting an issue

This is a hobby project with no bug bounty. If you find a real security problem — something that
lets this firmware read, leak, or interfere with anything outside itself (the flash filesystem it
doesn't own, the bootloader's PIN state beyond the one intended round trip, the secure elements) —
please open a GitHub issue, or a private security advisory on the repository if you'd rather not
disclose it in public first. General gameplay bugs are just normal issues.

For anything about the bootloader, the secure elements, or the signing protocol itself, that code
and its trust model are Coinkite's; see [Coldcard/firmware](https://github.com/Coldcard/firmware)
and their own security policy.
