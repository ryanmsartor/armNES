/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"

#include <stdio.h>
#include <stdlib.h>

static uint32_t pictureHash(const struct Gamate* g) {
	uint32_t hash = 2166136261U;
	for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
		hash ^= g->frameBuffer[i];
		hash *= 16777619U;
	}
	return hash;
}

static bool pictureHasInk(const struct Gamate* g) {
	for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
		if (g->frameBuffer[i]) return true;
	}
	return false;
}

static uint32_t audioHash(const int16_t* audio, size_t frames) {
	uint32_t hash = 2166136261U;
	for (size_t i = 0; i < frames * 2; i++) {
		hash ^= (uint16_t) audio[i];
		hash *= 16777619U;
	}
	return hash;
}

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
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "usage: %s gamate_bios.bin cartridge.bin [frames-after-entry]\n", argv[0]);
		return 2;
	}
	unsigned framesWanted = argc == 4 ? (unsigned) strtoul(argv[3], NULL, 0) : 300;
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
	bool entered = false;
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
			entered = true;
			break;
		}
	}
	if (!entered) {
		fprintf(stderr, "boot did not reach $6020 after %llu instructions: PC=$%04X, protection=%u/%u%s\n",
			(unsigned long long) instructions, g.cpu.pc, g.protectionSequence, g.protectionPosition,
			g.protectionPassed ? " unlocked" : " locked");
		gamateFree(&g);
		return 1;
	}

	unsigned nonblank = 0, audible = 0;
	unsigned peak = 0;
	int16_t audio[2048 * 2];
	gamateSetAudio(&g, audio, 2048, 48000);
	uint64_t longestSamePc = 0, samePc = 0;
	uint16_t previousPc = 0xFFFF;
	for (unsigned frame = 1; frame <= framesWanted; frame++) {
		g.frameDone = false;
		g.audioCount = 0;
		while (!g.frameDone) {
			uint16_t pc = g.cpu.pc;
			unsigned cycles = gamateCpuStep(&g);
			gamateRunClocks(&g, cycles * 2);
			if (g.cpu.pc == pc && pc == previousPc) {
				if (++samePc > longestSamePc) longestSamePc = samePc;
			} else {
				samePc = 0;
			}
			previousPc = pc;
		}
		if (pictureHasInk(&g)) nonblank++;
		bool hasAudio = false;
		size_t audioFrames = g.audioCount < g.audioCapacity ? g.audioCount : g.audioCapacity;
		for (size_t i = 0; i < audioFrames * 2; i++) {
			int value = audio[i] < 0 ? -(int) audio[i] : audio[i];
			if (value) hasAudio = true;
			if ((unsigned) value > peak) peak = value;
		}
		if (hasAudio) audible++;
		if (frame == 1 || frame == 60 || frame == framesWanted) {
			printf("frame %u: video=%08X audio=%08X/%zu PC=$%04X bank=%u/%u\n", frame,
				pictureHash(&g), audioHash(audio, audioFrames), audioFrames, g.cpu.pc, g.firstBank, g.bank);
		}
		if (g.diagnostics.unsupportedOpcodes) break;
	}
	printf("post-entry: mapper=%d, nonblank=%u/%u, audible=%u/%u peak=%u, IRQs=%llu, AY writes=%llu, "
		"LCD writes=%llu, VRAM writes=%llu, bank changes=%llu, longest self-loop=%llu\n",
		g.mapper, nonblank, framesWanted, audible, framesWanted, peak,
		(unsigned long long) g.diagnostics.irqsTaken, (unsigned long long) g.diagnostics.ayWrites,
		(unsigned long long) g.diagnostics.lcdWrites,
		(unsigned long long) g.diagnostics.vramWrites, (unsigned long long) g.diagnostics.bankWrites,
		(unsigned long long) longestSamePc);
	if (g.diagnostics.unsupportedOpcodes) {
		fprintf(stderr, "unsupported opcode $%02X at $%04X (%llu total)\n",
			g.diagnostics.lastUnsupportedOpcode, g.diagnostics.lastUnsupportedPc,
			(unsigned long long) g.diagnostics.unsupportedOpcodes);
		gamateFree(&g);
		return 1;
	}
	gamateFree(&g);
	return nonblank ? 0 : 1;
}
