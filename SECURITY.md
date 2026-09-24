# Security

## What this firmware can and can't touch

QUASAR has no Bitcoin code in it anywhere — no wallet, no seed words, no BIP-32/39, no PSBT, no
signing. None of that code was stripped out at the last minute; it was never built in. The game
(`game/`) is self-contained C with no knowledge of the secure elements at all, and the Python around
it (`firmware/python/`) only does three things: start the game, save its high-score file to
`/flash`, and run the SYSTEM menu. Even a full compromise of this firmware's own code cannot expose
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

## The bootloader warning is not a bug

Coinkite's bootloader only trusts firmware signed with their own factory key, which nobody outside
Coinkite can do. Every other build — this one included — has to be signed with the **public
developer key** instead (`firmware/keys/00.pem`; its private half is committed on purpose, same as
in Coinkite's own repository, because it is meant for exactly this: anyone building their own
firmware signs with it). The bootloader recognizes that and, on every single boot, shows a forced
warning screen for a few seconds and keeps the genuine-firmware light red for as long as this
firmware is installed. That check runs from the bootloader's own code, before this firmware's code
ever starts, so nothing in this repository can change, skip, or theme it. It's the honest price of
running something Coinkite didn't build, and it's exactly why you should keep an official firmware
`.dfu` on a spare microSD card before you install this (see the README).

## Verifying what you're installing

The `.dfu` in [`release/`](release/) is built by [`firmware/build.sh`](firmware/build.sh) from
exactly the source in this repository — nothing else goes into it. To check that for yourself:

```
git clone https://github.com/Ak1ra00/cc-Q.git && cd cc-Q
git submodule update --init external/micropython
./firmware/build.sh
sha256sum release/quasar-*.dfu
```

`firmware/build.sh sign` prints the header it embeds (version, timestamp, the hardware it's marked
compatible with) and verifies the signature it just made before writing the `.dfu`, so a corrupted
or mismatched build fails loudly there rather than silently on the device.

## Reporting an issue

This is a hobby project with no bug bounty. If you find a real security problem — something that
lets this firmware read, leak, or interfere with anything outside itself (the flash filesystem it
doesn't own, the bootloader's PIN state beyond the one intended round trip, the secure elements) —
please open a GitHub issue, or a private security advisory on the repository if you'd rather not
disclose it in public first. General gameplay bugs are just normal issues.

For anything about the bootloader, the secure elements, or the signing protocol itself, that code
and its trust model are Coinkite's; see [Coldcard/firmware](https://github.com/Coldcard/firmware)
and their own security policy.
