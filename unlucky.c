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

struct {
	size_t cpu_timestamp, ppu_timestamp;
	uint8_t *mem_map[0x20];
	uint16_t pc;
	uint8_t acc, irx, iry, status;
} processor_state;

int fmt_scaled(size_t, char *, size_t);
int print_instruction(struct instruction, const void *const, long int);
void usage(void);

int
main(int argc, char **argv)
{
	struct ines_header header;
	char human_scaled[FMT_SCALED_STRSIZE];
	FILE *rom;
	uint8_t mapper, *cur, (*prg_rom)[PRG_ROM_CHUNKSIZE],
	    (*chr_rom)[CHR_ROM_CHUNKSIZE];
	size_t prg_rom_size, chr_rom_size;
	int offset;

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

	printf("\n");

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

	// this should read whatever's at 0xFFFC
	processor_state.pc = prg_rom[0][0x3FFC];
	printf("entry point: 0x%04X\n", processor_state.pc);

	/* cur = prg_rom[0]; */
	/* while (cur < (&(prg_rom[0][0]) + prg_rom_size)) { */
	/* 	struct instruction cur_instr = instruction_set[*cur]; */
	/* 	offset = print_instruction(cur_instr, cur + 1, */
	/* 	    cur - prg_rom[0]); */
	/* 	if (offset < 0) { */
	/* 		fprintf(stderr, */
	/* 		    "%s: invalid instruction 0x%02X at offset
	 * 0x%04lX\n",
	 */
	/* 		    argv[0], *cur, cur - prg_rom[0]); */
	/* 		return 1; */
	/* 	} */
	/* 	cur += offset; */
	/* } */

	(void)cur;
	(void)offset;

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

int
print_instruction(struct instruction instr, const void *const op,
    long int offset)
{
	printf("0x%04lX\t%s", offset, opcodes[instr.opcode]);
	if (!instr.opcode)
		return -1;
	switch (instr.mode) {
	case IMP:
		printf("\n");
		return 1;
	case ACC:
		printf(" A\n");
		return 1;
	case IMM:
		printf(" #$%02X\n", *(uint8_t *)op);
		return 2;
	case ZRP:
		printf(" $%02X\n", *(uint8_t *)op);
		return 2;
	case ZPX:
		printf(" $%02X,X\n", *(uint8_t *)op);
		return 2;
	case ZPY:
		printf(" $%02X,Y\n", *(uint8_t *)op);
		return 2;
	case REL:
		printf(" %d\n", *(signed char *)op);
		return 2;
	case ABS:
		printf(" $%04X\n", *(uint16_t *)op);
		return 3;
	case ABX:
		printf(" $%04X,X\n", *(uint16_t *)op);
		return 3;
	case ABY:
		printf(" $%04X,Y\n", *(uint16_t *)op);
		return 3;
	case IND:
		printf(" ($%04X)\n", *(uint16_t *)op);
		return 3;
	case IDX:
		printf(" ($%02X,X)\n", *(uint8_t *)op);
		return 2;
	case IDY:
		printf(" ($%02X),Y\n", *(uint8_t *)op);
		return 2;
	default:
		return -1;
	}
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
