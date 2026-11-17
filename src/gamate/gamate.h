/* Copyright (c) 2026 Magnominus
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ARMNES_GAMATE_H
#define ARMNES_GAMATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { GAMATE_WIDTH = 160, GAMATE_HEIGHT = 150 };
enum { GAMATE_BIOS_SIZE = 0x1000, GAMATE_MAX_ROM_SIZE = 0x80000 };

enum GamateMapper { GAMATE_MAPPER_PLAIN, GAMATE_MAPPER_BANKED, GAMATE_MAPPER_4IN1 };

struct Gamate {
	uint8_t ram[0x400];
	uint8_t vram[0x4000];
	uint8_t bios[GAMATE_BIOS_SIZE];
	uint8_t* rom;
	size_t romSize;

	uint16_t vramAddress;
	uint8_t scrollX, scrollY;
	uint8_t bank, firstBank;
	bool vramPlane, incrementRow, window, swapPlanes, displayBlank, displayStopped;
	enum GamateMapper mapper;
	bool protectionPassed, protectionFailed, protectionReply;
	uint8_t protectionPosition, protectionByte, protectionSequence;

	uint8_t buttons, openBus;
	uint64_t masterClocks;
	uint32_t frameClocks;
	bool frameDone, irq;

	uint8_t ayAddress;
	uint8_t ayRegisters[16];

	uint16_t frameBuffer[GAMATE_WIDTH * GAMATE_HEIGHT];
};

void gamateInit(struct Gamate* gamate);
bool gamateLoad(struct Gamate* gamate, const uint8_t* rom, size_t romSize,
	const uint8_t* bios, size_t biosSize);
void gamateSetMapper(struct Gamate* gamate, enum GamateMapper mapper);
void gamateFree(struct Gamate* gamate);
void gamateReset(struct Gamate* gamate);

uint8_t gamateRead(struct Gamate* gamate, uint16_t address);
void gamateWrite(struct Gamate* gamate, uint16_t address, uint8_t value);
void gamateRunClocks(struct Gamate* gamate, unsigned clocks);
void gamateRunFrame(struct Gamate* gamate);
void gamateRender(struct Gamate* gamate);

size_t gamateStateSize(const struct Gamate* gamate);
bool gamateSaveState(const struct Gamate* gamate, void* data, size_t size);
bool gamateLoadState(struct Gamate* gamate, const void* data, size_t size);

#endif
