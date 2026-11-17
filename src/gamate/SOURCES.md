# Gamate implementation sources

This file records the sources used while developing armNES's Gamate backend, what each source was
used for, and important disagreements between them. It should be updated whenever a new source
influences implementation or tests.

The code in this directory is an original C implementation for this project. Emulator source is
used to cross-check observations and identify test cases, not as source text to translate. Primary
hardware measurements take precedence where they conflict with an emulator.

## Primary hardware research

### Kevtris, “Gamate Inside”, version 1.00 (2016-06-29)

- Archived URL: <https://web.archive.org/web/20160630141941id_/http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt>
- Original URL (currently unavailable): <http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt>
- Status: reverse-engineering notes based on measurements of physical Gamate hardware.
- Used for:
  - 4.433 MHz crystal, 2.2165 MHz CPU, and the NCR 65CX02 being NMOS-6502-like rather than a 65C02.
  - Physical address map, open-bus behavior, active-low controller layout, and BIOS mirroring.
  - Cartridge unlock protocol (`BIT CORPORATION`, nine writes per byte, `$47` reply), ROM validation
    sequence, common banking, and the distinct 4-in-1 banking scheme.
  - LCD register bit definitions, VRAM addressing, 256×256 backing bitmap, 160×150 viewport,
    200-line Y wrapping, both window modes, and the measured 72,900-crystal-clock frame.
  - AY-compatible register behavior, channel routing, and measured divider behavior.
  - The IRQ interval of 32,768 crystal clocks and the UART register behavior.
- Implementation impact: this is the principal behavioral specification for `gamate.c` and its
  tests. It caused corrections to the initial PSG read and IRQ-period implementations.

## Independent implementations used for comparison

### xrip/gamate

- URL: <https://github.com/xrip/gamate>
- Relevant files: `src/main.cpp`, `src/gamate/vdp.cpp`.
- License: repository source identifies its license in-file; `vdp.cpp` is BSD-3-Clause and its
  README says its video renderer was derived from MAME.
- Used for: checking ROM bank write addresses, input bit order, AY clock/routing choices, and how a
  small standalone emulator schedules the periodic IRQ.
- Not used as an independent authority for LCD behavior because its README and source explicitly
  say that portion came from MAME/Kevtris.
- Known limitation: its protection handling is abbreviated and it does not model all bus behavior.

### emulators.org Gamate machine

- Documentation: <https://emulators.org/emulator/gamate/>
- Machine source: <https://emulators.org/emulator/gamate/src/gamate-machine.js>
- Status: independently authored JavaScript machine around the public-domain Fake6502 core, though
  its header says its hardware values were verified against MAME.
- Used for: comparing the clean machine/CPU boundary, side-effect-free debugger reads, basic LCD
  address-port behavior, active-low input, and instruction-driven IRQ accumulation.
- Not treated as authoritative for protection, banking, special Y-window behavior, or exact timing;
  those pieces are absent or simplified.

## Compatibility reference

### MAME Gamate implementation

- Machine: <https://github.com/mamedev/mame/blob/master/src/mame/bitcorp/gamate.cpp>
- LCD: <https://github.com/mamedev/mame/blob/master/src/mame/bitcorp/gamate_v.cpp>
- Cartridge: <https://github.com/mamedev/mame/tree/master/src/devices/bus/gamate>
- License: GPL-2.0-or-later for the machine/LCD files; BSD-3-Clause for the cartridge-device files.
- Used for: identifying currently recognized BIOS variants, known cartridge sizes/types, and
  compatibility-oriented edge cases to investigate against primary notes or tests.
- Not used as the sole specification. In particular, its `$5800/$5A00` card-presence behavior
  differs from Kevtris's measured open-bus/`xx11` description; armNES currently follows the primary
  hardware notes and records the disagreement here.

## CPU specification and validation

### MOS Technology, MCS6500 Microcomputer Family Programming Manual

- Archive: <https://archive.org/details/mos_microcomputers_programming_manual>
- Status: original manufacturer programming manual.
- Used for: official NMOS 6502 instruction semantics, addressing modes, stack behavior, interrupt
  vectors, status flags, and documented cycle counts.

### Masswerk 6502 instruction set

- URL: <https://www.masswerk.at/6502/6502_instruction_set.html>
- Status: independently compiled opcode/addressing/cycle reference based on MOS documentation.
- Used for: cross-checking the official opcode matrix, instruction lengths, base cycles, and
  page-crossing penalties in `cpu.c`.

### Visual6502 decimal-mode research

- URL: <https://www.nesdev.org/wiki/Visual6502wiki/6502DecimalMode>
- Status: transistor-level analysis and exhaustive tests of NMOS 6502 decimal arithmetic.
- Used for: the deliberate NMOS behavior where decimal `ADC`/`SBC` adjust the accumulator and carry
  while N, V, and Z reflect the binary intermediate result. No Visual6502 implementation code is
  used.

### Klaus Dormann 6502 functional tests

- URL: <https://github.com/Klaus2m5/6502_65C02_functional_tests>
- License: tests repository license applies to the external test programs; no test ROM is bundled.
- Used for: external conformance validation of all official opcodes, addressing modes, flags,
  branches, stack operations, and valid-BCD decimal arithmetic. `gamate-6502test` links the
  production `cpu.c` to a separate flat 64 KiB bus and runs the repository's unmodified
  `6502_functional_test.bin` (SHA-1 `55c9ab5b137c8ced3c666bfdb55c8285782baad9`). The image is
  downloaded to `/tmp` for testing and is not distributed by this project.
- Result: passed at the documented `$3469` success trap after 30,646,193 instructions. The first run
  failed in decimal `SBC`; that result led to correcting the nibble borrow/adjust implementation.

## Local compatibility validation (files not distributed)

The following user-supplied files were read in place solely for boot validation. They are not copied
into the repository, build output, tests, or releases.

- UMC/NCR BIOS, SHA-1 `ea449dc607601f9a68d855ad6ab53800d2e99297`.
- BIT BIOS, SHA-1 `4e9dfbfe916ca485530ef4221593ab68738e2217`.
- Representative cartridges at 32, 64, 128, 256, and 512 KiB, plus the 128 KiB 4-in-1 cartridge.
  These were used to exercise the BIOS protection handshake, header text and checksum validation,
  logo delay, and final jump to the documented cartridge entry at `$6020`.
- Result: both BIOS revisions accepted every representative cartridge and reached `$6020`.
  `gamateboot` provides the reproducible diagnostic without containing any copyrighted bytes.
- The known 4-in-1 image has SHA-1 `4dd20632eb31023e18241290eca7d5a5acb1fa84` and CRC32
  `10e81571`. The runtime uses the whole-image CRC32 plus the exact 128 KiB size to select its unique
  mapper; ordinary cartridges continue to use size only to distinguish plain from commonly banked
  hardware. This fingerprint came from the locally validated image, not an emulator source.
- Post-entry validation ran 300 frames for Bomb Blaster under both BIOS revisions and representative
  64, 128, 256, and 512 KiB cartridges plus the 4-in-1. The diagnostic observed changing framebuffer
  hashes, nonblank output, IRQ service, LCD/VRAM traffic, and expected bank changes, with no unknown
  opcodes or persistent self-loop traps. Frame hashes are computed from four-shade pixel indices and
  contain no ROM or BIOS data.

## Still needed

- CPU validation traces or tests specifically for the NCR 65CX02, especially decimal mode and the
  reported bit-manipulation instructions.
- Direct test ROMs or logic-analyzer traces for IRQ assertion/clearing and LCD mid-frame writes.
- Identification data for the 4-in-1 cartridge that does not depend only on a filename/database.
