#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instructions.h"

#define XSTR(x) #x
#define STR(x) XSTR(x)

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

time_t cpu_timestamp, ppu_timestamp;
uint8_t *page_table[0x100];
uint8_t bus_value;

struct {
	uint16_t pc;
	uint8_t sp;
	uint8_t status;
	uint8_t acc;
	uint8_t irx;
	uint8_t iry;
} cpu;

struct {
	uint8_t cr1;
	uint8_t cr2;
	uint8_t status;
	uint8_t sprite_addr;
	uint8_t sprite_io;
	uint16_t vram_addr;
	uint8_t vram_io;
} ppu;

void run_cpu(time_t);
void run_ppu(time_t);

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
	cpu.acc = 0;
	cpu.irx = 0;
	cpu.iry = 0;
	cpu.sp = 0xFF;
	cpu.status = 0;
	cpu.pc = prg_rom[0][0x3FFC] + 0x8000;

	printf("entry point: 0x%04X\n", cpu.pc);

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

#define GET_CPU_BUS(hi, lo, out)                                             \
	if (hi >= 0x20 && hi < 0x60) {                                       \
		if (hi == 0x20) { /* ppu registers */                        \
			run_ppu(cpu_timestamp);                              \
			switch (lo) {                                        \
			case 2:                                              \
				/*                                           \
				 * TODO: technically only the first 3 bits   \
				 * should be read and the last 5 bits of the \
				 * PPU status register should be open bus    \
				 */                                          \
				out = ppu.status;                            \
				break;                                       \
			default:                                             \
				out = bus_value;                             \
				break;                                       \
			}                                                    \
		} else {                                                     \
			out = bus_value;                                     \
		}                                                            \
	} else                                                               \
		out = page_table[hi][lo];                                    \
	bus_value = out;

void
run_cpu(time_t until)
{
	uint8_t pc_hi, pc_lo, addr_hi, addr_lo, arg, arg_hi, cur;
	uint16_t tmp;
	struct instruction ins;

	while (cpu_timestamp < until) {
		pc_hi = cpu.pc / 0x100;
		pc_lo = cpu.pc & 0xFF;

		GET_CPU_BUS(pc_hi, pc_lo, cur);
		ins = instruction_set[cur];
		printf("0x%04X\t%s", cpu.pc, opcodes[ins.opcode]);

		switch (ins.mode) {
		case IMP:
			printf("\n");
			break;
		case ACC:
			printf(" A\n");
			arg = cpu.acc;
			break;
		case REL:
			GET_CPU_BUS(pc_hi, pc_lo + 1, arg);
			printf(" #$%d\n", *(int8_t *)&arg);
			cpu.pc++;
			break;
		case IMM:
			GET_CPU_BUS(pc_hi, pc_lo + 1, arg);
			printf(" #$%02X\n", arg);
			cpu.pc++;
			break;
		case ZRP:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			printf(" $%02X\n", addr_lo);
			arg = page_table[0][addr_lo];
			cpu.pc++;
			break;
		case ZPX:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			printf(" $%02X,X\n", addr_lo);
			arg = page_table[0][(addr_lo + cpu.irx) & 0xFF];
			cpu.pc++;
			break;
		case ZPY:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			printf(" $%02X,Y\n", addr_lo);
			arg = page_table[0][(addr_lo + cpu.iry) & 0xFF];
			cpu.pc++;
			break;
		case ABS:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			GET_CPU_BUS(pc_hi, pc_lo + 2, addr_hi);
			printf(" $%02X%02X\n", addr_hi, addr_lo);
			GET_CPU_BUS(addr_hi, addr_lo, arg);
			cpu.pc += 2;
			break;
		case ABX:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			GET_CPU_BUS(pc_hi, pc_lo + 2, addr_hi);
			printf(" $%02X%02X,X\n", addr_hi, addr_lo);
			tmp = addr_lo + cpu.irx;
			addr_hi += tmp >> 8;
			/* penalty if page boundary crossed */
			cpu_timestamp += NTSC_CPU_CYCLE * tmp >> 8;
			addr_lo = tmp & 0xFF;
			GET_CPU_BUS(addr_hi, addr_lo, arg);
			cpu.pc += 2;
			break;
		case ABY:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			GET_CPU_BUS(pc_hi, pc_lo + 2, addr_hi);
			printf(" $%02X%02X,Y\n", addr_hi, addr_lo);
			tmp = addr_lo + cpu.iry;
			addr_hi += tmp >> 8;
			/* penalty if page boundary crossed */
			cpu_timestamp += NTSC_CPU_CYCLE * tmp >> 8;
			addr_lo = tmp & 0xFF;
			GET_CPU_BUS(addr_hi, addr_lo, arg);
			cpu.pc += 2;
			break;
		case IND:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			GET_CPU_BUS(pc_hi, pc_lo + 2, addr_hi);
			printf(" ($%02X%02X)\n", addr_hi, addr_lo);
			GET_CPU_BUS(addr_hi, addr_lo, arg);
			GET_CPU_BUS(addr_hi, addr_lo + 1, arg_hi);
			cpu.pc += 2;
			break;
		case IDX:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			printf(" ($%02X,X)\n", addr_lo);
			addr_hi = page_table[0][(addr_lo + cpu.irx + 1) & 0xFF];
			addr_lo = page_table[0][(addr_lo + cpu.irx) & 0xFF];
			GET_CPU_BUS(addr_hi, addr_lo, arg);
			cpu.pc++;
			break;
		case IDY:
			GET_CPU_BUS(pc_hi, pc_lo + 1, addr_lo);
			printf(" ($%02X),Y\n", addr_lo);
			addr_hi = page_table[0][(addr_lo + 1) & 0xFF];
			addr_lo = page_table[0][addr_lo];
			GET_CPU_BUS(addr_hi, addr_lo, arg);
			tmp = addr_lo + cpu.iry;
			addr_hi += tmp >> 8;
			/* penalty if page boundary crossed */
			cpu_timestamp += NTSC_CPU_CYCLE * tmp >> 8;
			addr_lo = tmp & 0xFF;
			GET_CPU_BUS(addr_hi, addr_lo, arg);
			cpu.pc++;
			break;
		}

		cpu.pc++;
		cpu_timestamp += NTSC_CPU_CYCLE * ins.cycles;
	}
	for (int i = 7; i >= 0; i--) {
		putc((cpu.status >> i & 1) ? '1' : '0', stdout);
	}
	putc('\n', stdout);
	return;
}

void
run_ppu(time_t until)
{
	printf("hello from the ppu :)\n");
	while (ppu_timestamp < until) {
		ppu_timestamp += PPU_CYCLE;
	}
	exit(0);
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
	    "USAGE: unlucky [OPTIONS] FILE\n"
	    "Try 'unlucky --help' for more information\n");
}
