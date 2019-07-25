// Copyright (c) 2009 Michael Steil
// All rights reserved. License: 2-clause BSD

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "fake6502.h"
#include "disasm.h"
#include "memory.h"
#include "video.h"
#include "via.h"
#include "loadsave.h"
#include "glue.h"

#define MHZ 8

//#define DEBUG

#ifdef DEBUG
#include "rom_labels.h"
char *
label_for_address(uint16_t address)
{
	for (int i = 0; i < sizeof(addresses) / sizeof(*addresses); i++) {
		if (address == addresses[i]) {
			return labels[i];
		}
	}
	return NULL;
}
#endif

int
main(int argc, char **argv)
{
	if (argc < 3) {
		printf("Usage: %s <rom.bin> <chargen.bin> [<app.prg>[,<load_addr>]]\n\n", argv[0]);
		printf("<rom.bin>:     ROM file:\n");
		printf("                 $0000-$1FFF bank #0 of banked ROM (BASIC)\n");
		printf("                 $2000-$3FFF fixed ROM at $E000-$FFFF (KERNAL)\n");
		printf("                 $4000-$5FFF bank #1 of banked ROM\n");
		printf("                 $6000-$7FFF bank #2 of banked ROM\n");
		printf("                 ...\n");
		printf("               The file needs to be at least $4000 bytes in size.\n\n");
		printf("<chargen.bin>: Character ROM file:\n");
		printf("                 $0000-$07FF upper case/graphics\n");
		printf("                 $0800-$0FFF lower case\n\n");
		printf("<app.prg>:     Application PRG file (with 2 byte start address header)\n\n");
		printf("<load_addr>:   Override load address (hex, no prefix)\n\n");
		exit(1);
	}

	// 1st argument: ROM
	FILE *f = fopen(argv[1], "r");
	if (!f) {
		printf("Cannot open %s!\n", argv[1]);
		exit(1);
	}
	fread(ROM, 1, ROM_SIZE, f);
	fclose(f);

	// 2nd argument: Character ROM
	f = fopen(argv[2], "r");
	if (!f) {
		printf("Cannot open %s!\n", argv[2]);
		exit(1);
	}
	uint8_t chargen[4096];
	fread(chargen, 1, sizeof(chargen), f);
	fclose(f);

	// 3rd argument: application (optional)
	FILE *prg_file = NULL;
	int prg_override_start = -1;
	if (argc == 4) {
		char *filename = argv[3];
		char *comma = strchr(filename, ',');
		if (comma) {
			prg_override_start = (uint16_t)strtol(comma + 1, NULL, 16);
			*comma = 0;
		}

		prg_file = fopen(filename, "r");
		if (!prg_file) {
			printf("Cannot open %s!\n", argv[3]);
			exit(1);
		}
	}

	video_init(chargen);

	reset6502();

	int instruction_counter = 0;
	for (;;) {
#ifdef DEBUG
		printf("\t\t\t\t[%6d] ", instruction_counter);

		char *label = label_for_address(pc);
		int label_len = label ? strlen(label) : 0;
		if (label) {
			printf("%s", label);
		}
		for (int i = 0; i < 10 - label_len; i++) {
			printf(" ");
		}
		printf(" .,%04x ", pc);
		char disasm_line[15];
		int len = disasm(pc, RAM, disasm_line, sizeof(disasm_line));
		for (int i = 0; i < len; i++) {
			printf("%02x ", read6502(pc + i));
		}
		for (int i = 0; i < 9 - 3 * len; i++) {
			printf(" ");
		}
		printf("%s", disasm_line);
		for (int i = 0; i < 15 - strlen(disasm_line); i++) {
			printf(" ");
		}

		printf("a=$%02x x=$%02x y=$%02x s=$%02x p=", a, x, y, sp);
		for (int i = 7; i >= 0; i--) {
			printf("%c", (status & (1 << i)) ? "czidb.vn"[i] : '-');
		}
		printf("\n");
#endif

		if (pc == 0xffd5 || pc == 0xffd8) {
			if (pc == 0xffd5) {
				LOAD();
			} else {
				SAVE();
			}
			pc = (RAM[0x100 + sp + 1] | (RAM[0x100 + sp + 2] << 8)) + 1;
			sp += 2;
		}

		step6502();
		ps2_step();
		instruction_counter++;

		if (instruction_counter % (20000 * MHZ) == 0) {
			if (!video_update()) {
				break;
			}
			usleep(20000);
//			printf("IRQ!\n");
			irq6502();
		}

		if (pc == 0xffcf && prg_file) {
			// as soon as BASIC starts reading a line,
			// inject the app
			uint8_t start_lo = fgetc(prg_file);
			uint8_t start_hi = fgetc(prg_file);
			uint16_t start;
			if (prg_override_start >= 0) {
				start = prg_override_start;
			} else {
				start = start_hi << 8 | start_lo;
			}
			fread(RAM + start, 1, 65536-start, prg_file);
			fclose(prg_file);
			prg_file = NULL;
		}
	}

	video_end();

	return 0;
}

