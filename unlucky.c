#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instructions.h"

#define VERSION "0.1"
#define FMT_SCALED_STRSIZE 16

#define MIRRORING 1
#define PERSISTENT_MEM 1 << 1
#define HAS_TRAINER 1 << 2
#define IGNORE_MIRRORING 1 << 3

#define MAGIC_NUMBER 0x1A53454E
#define PRG_ROM_CHUNKSIZE 0x4000
#define CHR_ROM_CHUNKSIZE 0x2000
#define TRAINER_SIZE 0x200
#define NES_PAGE_SIZE 0x100

#define PAL_CPU_CYCLE 16
#define NTSC_CPU_CYCLE 15
#define PPU_CYCLE 5

#define CARRY_FLAG 0x01
#define ZERO_FLAG 0x02
#define INTERRUPT_DISABLE 0x04
#define DECIMAL_MODE 0x08 // ignored by NES but keeping it for completeness
#define BREAK_BIT 0x10
#define OVERFLOW_FLAG 0x20
#define NEGATIVE_FLAG 0x40

struct ines_header {
	uint32_t magic;
	uint8_t prg_rom_chunks;
	uint8_t chr_rom_chunks;
	uint8_t flags[5];
	char garbage[5];
};

size_t cpu_timestamp, ppu_timestamp;
uint8_t *page_table[0x100];
uint16_t pc;
uint8_t sp, acc, irx, iry, status;

void run_cpu(size_t);
void run_ppu(size_t);

int fmt_scaled(size_t, char *, size_t);
void usage(void);

int
main(int argc, char **argv)
{
	struct ines_header header;
	char human_scaled[FMT_SCALED_STRSIZE];
	FILE *rom;
	uint8_t mapper, (*prg_rom)[PRG_ROM_CHUNKSIZE],
	    (*chr_rom)[CHR_ROM_CHUNKSIZE], cpu_ram[0x800], sram[0x2000];
	size_t prg_rom_size, chr_rom_size;
	int i;

	prg_rom = NULL;
	chr_rom = NULL;

	if (argc < 2) {
		usage();
		return 1;
	}

	if ((rom = fopen(argv[1], "rb")) == NULL) {
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		return 1;
	}

	if (fread(&header, sizeof(struct ines_header), 1, rom) == 0)
		goto read_error;

	if (header.magic != MAGIC_NUMBER)
		fprintf(stderr, "%s: warning: bad magic number in ROM file\n",
		    argv[0]);

	prg_rom_size = PRG_ROM_CHUNKSIZE * header.prg_rom_chunks;
	chr_rom_size = CHR_ROM_CHUNKSIZE * header.chr_rom_chunks;

	fmt_scaled(prg_rom_size, human_scaled, FMT_SCALED_STRSIZE);
	printf("PRG ROM: %s\n", human_scaled);
	fmt_scaled(chr_rom_size, human_scaled, FMT_SCALED_STRSIZE);
	printf("CHR ROM: %s\n", human_scaled);

	mapper = (header.flags[0] >> 4) + (header.flags[1] & 0xF0);

	printf("mapper: %X\n", mapper);

	if (header.flags[0] & HAS_TRAINER) {
		// TODO figure out wtf a trainer is
		// just throw it away for now
		fseek(rom, TRAINER_SIZE, SEEK_CUR);
	}

	if (!(prg_rom = malloc(prg_rom_size))) {
		fprintf(stderr, "%s: out of memory!\n", argv[0]);
		goto error;
	}

	if (!(chr_rom = malloc(chr_rom_size))) {
		fprintf(stderr, "%s: out of memory!\n", argv[0]);
		goto error;
	}

	if (fread(prg_rom, 1, prg_rom_size, rom) != prg_rom_size)
		goto read_error;

	if (fread(chr_rom, 1, chr_rom_size, rom) != chr_rom_size)
		goto read_error;

	fclose(rom);

	// set up processor state
	cpu_timestamp = 0;
	ppu_timestamp = 0;
	acc = 0;
	irx = 0;
	iry = 0;
	sp = 0xFF;
	status = 0;
	pc = prg_rom[0][0x3FFC] + 0x8000;

	printf("entry point: 0x%04X\n", pc);

	printf("\n");

	// set up memory map
	// mirror cpu ram 4 times
	for (i = 0; i < 0x20; i++) {
		page_table[i] = cpu_ram + NES_PAGE_SIZE * (i % 8);
	}

	// zero out memory mapped IO and expansion ROM
	for (; i < 0x60; i++) {
		page_table[i] = NULL;
	}

	// set up sram
	for (; i < 0x80; i++) {
		page_table[i] = sram + NES_PAGE_SIZE * (i - 0x60);
	}

	// mirror the 1 chunk of PRG ROM twice
	for (; i < 0x100; i++) {
		page_table[i] = prg_rom[0] + NES_PAGE_SIZE * (i % 0x40);
	}

	printf("memory map:\n");
	for (i = 0; i < 0x100; i += 8) {
		printf("0x%04X: %p\n", i * NES_PAGE_SIZE, page_table[i]);
	}

	printf("\n");

	run_cpu(75);

	return 0;

read_error:
	if (feof(rom)) {
		fprintf(stderr, "%s: error reading ROM file, unexpected EOF\n",
		    argv[0]);
	} else if (ferror(rom)) {
		fprintf(stderr, "%s: error reading ROM file\n", argv[0]);
	}
error:
	fclose(rom);
	if (prg_rom)
		free(prg_rom);
	if (chr_rom)
		free(chr_rom);
	return 1;
}

void
run_cpu(size_t until)
{
	uint8_t pc_upper, pc_lower, op8, *cur;
	uint16_t op16;

	while (cpu_timestamp < until) {
		pc_upper = pc / 0x100;
		pc_lower = pc & 0xFF;
		cur = &page_table[pc_upper][pc_lower];
		op8 = *(cur + 1);
		op16 = *(uint16_t *)(cur + 1);
		printf("0x%02X ", *cur);
		print_instruction(instruction_set[*cur], cur + 1, pc);
		switch (page_table[pc_upper][pc_lower]) {
		case 0x78: // SEI
			status |= INTERRUPT_DISABLE;
			goto one_byte;
		case 0xAD: // LDA absolute
			acc = op16;
			goto three_byte;
		case 0xD8: // CLD - does nothing on NES
			goto one_byte;
		default:
			cpu_timestamp += 9999;
			break;
		three_byte:
			cpu_timestamp += NTSC_CPU_CYCLE;
			pc++;
			cpu_timestamp += NTSC_CPU_CYCLE;
			pc++;
		one_byte:
			cpu_timestamp += 2 * NTSC_CPU_CYCLE;
			pc++;
			break;
		}
	}
	(void)op8;
	for (int i = 7; i >= 0; i--) {
		putc((status >> i & 1) ? '1' : '0', stdout);
	}
	putc('\n', stdout);
	return;
}

int
fmt_scaled(size_t bytes, char *buf, size_t max_bytes)
{
	static const char prefixes[] = "kMGT";
	float f_bytes;
	unsigned int i;

	if (bytes < 0x400)
		return snprintf(buf, max_bytes, "%zuB", bytes);

	i = 0;
	while (bytes > 0x10000 && i < sizeof(prefixes)) {
		bytes /= 0x400;
		i++;
	}
	f_bytes = bytes / 1024.0;
	return snprintf(buf, max_bytes, "%.4g%cB", f_bytes, prefixes[i]);
}

void
usage()
{
	fprintf(stderr,
	    "USAGE: unlucky [OPTIONS] FILE\nTry"
	    " 'unlucky --help' for more information\n");
}
