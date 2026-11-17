/* Copyright (c) 2026 Magnominus
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void testMemory(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE], rom[0x10000];
	for (size_t i = 0; i < sizeof(bios); i++) bios[i] = (uint8_t) i;
	for (size_t i = 0; i < sizeof(rom); i++) rom[i] = (uint8_t) (i >> 14);
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));

	gamateWrite(&g, 0x0123, 0x42);
	assert(gamateRead(&g, 0x0523) == 0x42);
	assert(gamateRead(&g, 0xE123) == 0x23);
	assert(gamateRead(&g, 0xF123) == 0x23);
	assert(gamateRead(&g, 0x6000) == 0); // protected before the BIOS handshake
	static const char key[] = "BIT CORPORATION";
	for (size_t byte = 0; byte < sizeof(key) - 1; byte++) {
		for (int bit = 7; bit >= 0; bit--) {
			gamateWrite(&g, 0x6000, (key[byte] >> bit & 1) << 2);
		}
		gamateWrite(&g, 0x6000, 0); // latch the completed byte
	}
	uint8_t reply = 0;
	for (int bit = 0; bit < 8; bit++) reply = (reply << 1) | (gamateRead(&g, 0x6000) >> 1);
	assert(reply == 0x47 && g.protectionPassed);
	assert(gamateRead(&g, 0x6000) == 0);
	assert(gamateRead(&g, 0xA000) == 0);
	gamateWrite(&g, 0xC000, 3);
	assert(gamateRead(&g, 0xA000) == 3);
	gamateSetMapper(&g, GAMATE_MAPPER_4IN1);
	gamateWrite(&g, 0x8000, 2);
	assert(gamateRead(&g, 0x6000) == 2);

	assert(gamateRead(&g, 0x4100) == 0x41); // write-only PSG: open bus
	assert(gamateRead(&g, 0x5800) == 0x58);
	assert(gamateRead(&g, 0x5A00) == 0x5B);
	gamateFree(&g);
}

static void testVideo(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	g.protectionPassed = true;
	gamateWrite(&g, 0x5003, 0); // no vertical scroll
	gamateWrite(&g, 0x5004, 0); // plane 0, address low
	gamateWrite(&g, 0x5005, 0); // address high
	gamateWrite(&g, 0x5007, 0x80);
	gamateWrite(&g, 0x5004, 0x80); // plane 1, address low
	gamateWrite(&g, 0x5005, 0);
	gamateWrite(&g, 0x5007, 0x80);
	gamateRender(&g);
	assert(g.frameBuffer[0] == 3);
	assert(g.frameBuffer[1] == 0);
	gamateWrite(&g, 0x5001, 0x80);
	gamateRender(&g);
	assert(g.frameBuffer[0] == 0);
	gamateFree(&g);
}

static void testTimingAndState(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	gamateRunClocks(&g, 32767);
	assert(!g.irq);
	gamateRunClocks(&g, 1);
	assert(g.irq);
	gamateReset(&g);
	gamateRunFrame(&g);
	assert(g.frameDone && g.masterClocks >= 72900);
	assert(g.frameClocks == g.masterClocks - 72900); // preserve an instruction crossing the frame edge
	g.ram[7] = 0x5A;
	size_t size = gamateStateSize(&g);
	void* state = malloc(size);
	assert(state && gamateSaveState(&g, state, size));
	g.ram[7] = 0;
	assert(gamateLoadState(&g, state, size));
	assert(g.ram[7] == 0x5A);
	free(state);
	gamateFree(&g);
}

static void testCpu(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	uint8_t program[] = {
		0xA9, 0x45,       // LDA #$45
		0xF8,             // SED
		0x18,             // CLC
		0x69, 0x55,       // ADC #$55 -> $00 with carry in BCD
		0xD8,             // CLD
		0xA2, 0x03,       // LDX #3
		0xCA,             // DEX
		0xD0, 0xFD,       // BNE $E009
		0x20, 0x10, 0xE0, // JSR $E010
		0xEA,             // next instruction after RTS
		0xE8,             // INX
		0x60,             // RTS
	};
	memcpy(bios, program, sizeof(program));
	bios[0xFFC] = 0x00;
	bios[0xFFD] = 0xE0;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	for (int i = 0; i < 15; i++) gamateCpuStep(&g);
	assert(g.cpu.a == 0 && (g.cpu.p & 1));
	assert(!(g.cpu.p & 2)); // NMOS decimal Z comes from the unadjusted binary result ($9A)
	assert(g.cpu.x == 1 && g.cpu.pc == 0xE00F);
	gamateFree(&g);
}

int main(void) {
	testMemory();
	testVideo();
	testTimingAndState();
	testCpu();
	return 0;
}
