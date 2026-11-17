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

## Libretro integration references

### Official libretro core-info example and RetroArch core installation guide

- Core-info template: <https://github.com/libretro/libretro-core-info/blob/master/00_example_libretro.info>
- Installation guide: <https://docs.libretro.com/guides/download-cores/>
- Used for: declaring save-state support and its `serialized` feature level in
  `armgamate_libretro.info`, and confirming that an out-of-tree core may load without its info file
  while RetroArch leaves metadata-gated features such as save states disabled.
- Implementation impact: the shipped info file now advertises the serialization backend already
  implemented by the core. This metadata affects frontend feature discovery only; it does not
  supply or replace serialization behavior.

## Audio specification and comparison

### General Instrument AY-3-8910/8912/8913 data manual

- Scan: <https://map.grauw.nl/resources/sound/generalinstrument_ay-3-8910.pdf>
- Status: original chip-manufacturer data manual.
- Used for: register layout and masks, tone/noise/mixer signal paths, amplitude/envelope controls,
  period widths, envelope shape bits, reset state, and the relationship between the input clock and
  the tone, noise, and envelope dividers.

### Ayumi

- URL: <https://github.com/true-grue/ayumi>
- License: MIT.
- Status: independent high-accuracy AY-3-8910/YM2149 implementation.
- Used for: comparison of counter organization, mixer gating, envelope restart/hold/alternate
  semantics, sample-rate conversion, and DC-removal strategy. No Ayumi source was copied.

### emu2149

- Example source: <https://github.com/emestee/bk-emulator/blob/master/emu2149.c>
- Status: Mitsutaka Okazaki's established AY/YM implementation; its source lists the AY data sheet,
  MSX documentation, and earlier emulators as references.
- Used for: cross-checking the AY rather than YM logarithmic 16-level amplitude curve and zero-period
  handling. The compact level table in `gamate.c` is an independently scaled integer expression of
  the documented AY curve, not imported code.

Gamate-specific clocking and channel routing continue to come from Kevtris's physical measurements:
the PSG runs at 4.433 MHz / 4; channel A goes left, B right, and C to both sides.

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
- Audio validation over the same representative set observed PSG register writes and nonzero output
  without clipping. `gamatetest` separately verifies tone timing/sample count, noise-LFSR advance,
  envelope restart/advance, register masks, A-left isolation, B-right isolation, and equal C routing
  to both sides. `gamateboot` records deterministic per-frame audio hashes without storing samples.
- LCD execution now follows the measured 450 row-latch events per frame: 150 visible rows, three
  162-master-clock refresh passes per row, totaling 72,900 clocks. Plane 0 is sampled once and plane
  1 twice (reversed by plane-swap), reproducing the physical four-level accumulation described by
  Kevtris. A regression test changes VRAM between passes and verifies that completed earlier passes
  are not retroactively redrawn. The partial accumulator, row event, clock phase, and completed image
  are included in save states.
- The progressive renderer was rerun for 300 post-entry Bomb Blaster frames. Early hashes changed
  from the former frame-end snapshot, as expected; the settled frame-300 hash remained `e324a485`.
  No local MAME executable or hardware capture was available for pixel-by-pixel external comparison,
  so those hashes are regression baselines rather than claims of independent visual equivalence.
- An interactive RetroArch run of Bomb Blaster confirmed correct panel orientation, working audio,
  Start, and all four directions. The initial isolated frontend configuration did not produce an
  observable response from its implicit X/Z face-button bindings. Kevtris's physical controller
  readout was rechecked to distinguish frontend configuration from emulated wiring: `$4400` bits 4
  and 5 are active-low A and B, matching the core; xrip independently uses the same bit order. A
  temporary live trace then confirmed both A and B press/release transitions reached those exact
  bits, and Bomb Blaster began accepting bomb input. The trace was removed after diagnosis. The
  libretro core polls once immediately before each emulated frame (at 60.8093 Hz), bounding its own
  input-sampling delay to one frame; remaining perceived sluggishness has not yet been separated
  from Bomb Blaster's game logic or display behavior.
- Interactive GP Race testing initially reported “save states unsupported.” Temporary traces proved
  RetroArch rejected the operation before calling any serialization entry point, while a direct
  query of the built core returned its 89,568-byte fixed state size. The cause was the isolated test
  configuration not loading this out-of-tree core's metadata. The test configuration now points at
  the repository info directory and treats `build/` as its core directory, allowing RetroArch to
  index and associate the out-of-tree `.so` with `armgamate_libretro.info`. With that installation-
  equivalent layout, GP Race successfully saved and restored an 89,568-byte core payload (89,592
  bytes in RetroArch's state file). The shipped info file explicitly declares serialized-state
  support following the official libretro template. Temporary traces were removed. A bypass test
  was used only to isolate metadata discovery and is not required by the installed core.
- A subsequent installed-layout run cleanly unloaded GP Race and loaded Cosmic Fighter with a
  content-specific state path. Cosmic Fighter reached responsive gameplay, successfully saved and
  loaded state, reset through the libretro reset callback, and unloaded without frontend or core
  errors. This validates the observed frontend lifecycle; it is not evidence about original-hardware
  timing accuracy.

## Still needed

- CPU validation traces or tests specifically for the NCR 65CX02, especially decimal mode and the
  reported bit-manipulation instructions.
- Direct test ROMs or logic-analyzer traces for IRQ assertion/clearing and LCD mid-frame writes.
- Identification data for the 4-in-1 cartridge that does not depend only on a filename/database.
