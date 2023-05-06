#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instructions.h"

#define VERSION "0.1"
#define FMT_SCALED_STRSIZE 16

#define MAGIC_NUMBER 0x1A53454E
#define PRG_ROM_CHUNKSIZE 0x4000
#define CHR_ROM_CHUNKSIZE 0x2000
#define TRAINER_SIZE 0x200

#define MIRRORING 1
#define PERSISTENT_MEM 1 << 1
#define HAS_TRAINER 1 << 2
#define IGNORE_MIRRORING 1 << 3

struct ines_header {
	uint32_t magic;
	uint8_t prg_rom_chunks;
	uint8_t chr_rom_chunks;
	uint8_t flags[5];
	char garbage[5];
};

int fmt_scaled(size_t, char *, size_t);
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

	cur = prg_rom[0];
	while (cur < (&(prg_rom[0][0]) + prg_rom_size)) {
		struct instruction cur_instr = instruction_set[*cur];
		if (!cur_instr.opcode) {
			fprintf(stderr,
			    "%s: invalid instruction 0x%2X at offset %4lX\n",
			    argv[0], *cur, cur - prg_rom[0]);
			return 1;
		}
		switch (cur_instr.mode) {
		case IMP:
			printf("%s\n", opcodes[cur_instr.opcode]);
			cur += 1;
			break;
		case ACC:
			printf("%s A\n", opcodes[cur_instr.opcode]);
			cur += 1;
			break;
		case IMM:
			printf("%s #$%2X\n", opcodes[cur_instr.opcode],
			    *(cur + 1));
			cur += 2;
			break;
		case ZRP:
			printf("%s $%2X\n", opcodes[cur_instr.opcode],
			    *(cur + 1));
			cur += 2;
			break;
		case ZPX:
			printf("%s $%2X,X\n", opcodes[cur_instr.opcode],
			    *(cur + 1));
			cur += 2;
			break;
		case ZPY:
			printf("%s $%2X,Y\n", opcodes[cur_instr.opcode],
			    *(cur + 1));
			cur += 2;
			break;
		case REL:
			printf("%s %d\n", opcodes[cur_instr.opcode],
			    *((signed char *)cur + 1));
			cur += 2;
			break;
		case ABS:
			printf("%s $%4X\n", opcodes[cur_instr.opcode],
			    *((uint16_t *)(cur + 1)));
			cur += 3;
			break;
		case ABX:
			printf("%s $%4X,X\n", opcodes[cur_instr.opcode],
			    *((uint16_t *)(cur + 1)));
			cur += 3;
			break;
		case ABY:
			printf("%s $%4X,Y\n", opcodes[cur_instr.opcode],
			    *((uint16_t *)(cur + 1)));
			cur += 3;
			break;
		case IND:
			printf("%s ($%4X)\n", opcodes[cur_instr.opcode],
			    *((uint16_t *)(cur + 1)));
			cur += 3;
			break;
		case IDX:
			printf("%s ($%2X,X)\n", opcodes[cur_instr.opcode],
			    *(cur + 1));
			cur += 2;
			break;
		case IDY:
			printf("%s ($%2X),Y\n", opcodes[cur_instr.opcode],
			    *(cur + 1));
			cur += 2;
			break;
		}
	}

	(void)cur;

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
