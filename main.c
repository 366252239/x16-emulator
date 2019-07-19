// Copyright (c) 2009 Michael Steil
// All rights reserved. License: 2-clause BSD

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "fake6502.h"
#include "glue.h"

int
main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s <rom.bin>\n", argv[0]);
		exit(1);
	}

	FILE *f = fopen(argv[1], "r");
	if (!f) {
		printf("Cannot open %s!\n", argv[1]);
		exit(1);
	}

	uint8_t *rom = malloc(65536);
	size_t rom_size = fread(rom, 1, 65536, f);
	fclose(f);
	memcpy(RAM + 65536 - rom_size, rom, rom_size);
	free(rom);

	reset6502();

	for (;;) {
		printf("pc = %04x; %02x %02x %02x, sp=%02x [%02x, %02x, %02x, %02x, %02x]\n", pc, RAM[pc], RAM[pc+1], RAM[pc+2], sp, RAM[0x100 + sp + 1], RAM[0x100 + sp + 2], RAM[0x100 + sp + 3], RAM[0x100 + sp + 4], RAM[0x100 + sp + 5]);
		step6502();
	}

	return 0;
}
