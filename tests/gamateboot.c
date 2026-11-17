/* Copyright (c) 2026 Magnominus
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t* readFile(const char* path, size_t* size) {
	FILE* file = fopen(path, "rb");
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END) || (*size = (size_t) ftell(file), fseek(file, 0, SEEK_SET))) {
		fclose(file);
		return NULL;
	}
	uint8_t* data = malloc(*size);
	if (!data || fread(data, 1, *size, file) != *size) {
		free(data);
		data = NULL;
	}
	fclose(file);
	return data;
}

int main(int argc, char** argv) {
	if (argc != 3) {
		fprintf(stderr, "usage: %s gamate_bios.bin cartridge.bin\n", argv[0]);
		return 2;
	}
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	if (!bios || !rom) {
		fprintf(stderr, "could not read BIOS or cartridge\n");
		free(bios); free(rom);
		return 2;
	}
	struct Gamate g;
	gamateInit(&g);
	if (!gamateLoad(&g, rom, romSize, bios, biosSize)) {
		fprintf(stderr, "invalid BIOS or cartridge size\n");
		free(bios); free(rom);
		return 2;
	}
	free(bios); free(rom);
	printf("reset PC=$%04X, cartridge=%zu bytes, mapper=%d\n", g.cpu.pc, g.romSize, g.mapper);

	bool reportedSequence = false, reportedUnlock = false;
	uint64_t instructions;
	for (instructions = 0; instructions < 20000000; instructions++) {
		uint16_t pc = g.cpu.pc;
		unsigned cycles = gamateCpuStep(&g);
		gamateRunClocks(&g, cycles * 2);
		if (!reportedSequence && g.protectionSequence == 15) {
			reportedSequence = true;
			printf("protection key accepted after %llu instructions at PC=$%04X\n",
				(unsigned long long) instructions + 1, pc);
		}
		if (!reportedUnlock && g.protectionPassed) {
			reportedUnlock = true;
			printf("cartridge unlocked after %llu instructions at PC=$%04X\n",
				(unsigned long long) instructions + 1, pc);
		}
		if (reportedUnlock && g.cpu.pc == 0x6020) {
			printf("BIOS reached cartridge entry $6020 after %llu instructions (%llu CPU cycles)\n",
				(unsigned long long) instructions + 1, (unsigned long long) g.cpu.cycles);
			gamateFree(&g);
			return 0;
		}
	}
	fprintf(stderr, "boot did not reach $6020 after %llu instructions: PC=$%04X, protection=%u/%u%s\n",
		(unsigned long long) instructions, g.cpu.pc, g.protectionSequence, g.protectionPosition,
		g.protectionPassed ? " unlocked" : " locked");
	gamateFree(&g);
	return 1;
}
