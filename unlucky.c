#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "0.1"
#define FMT_SCALED_STRSIZE 16

#define MAGIC_NUMBER 0x1A53454E
#define PRG_ROM_CHUNKSIZE 0x4000
#define CHR_ROM_CHUNKSIZE 0x2000

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
	uint8_t mapper, (*prg_rom)[0x4000], (*chr_rom)[0x2000];

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

	fmt_scaled(PRG_ROM_CHUNKSIZE * header.prg_rom_chunks, human_scaled,
	    FMT_SCALED_STRSIZE);
	printf("PRG ROM: %s\n", human_scaled);
	fmt_scaled(CHR_ROM_CHUNKSIZE * header.chr_rom_chunks, human_scaled,
	    FMT_SCALED_STRSIZE);
	printf("CHR ROM: %s\n", human_scaled);
	if (!(prg_rom = malloc(PRG_ROM_CHUNKSIZE * header.prg_rom_chunks))) {
		fprintf(stderr, "%s: out of memory!\n", argv[0]);
		goto error;
	}

	mapper = (header.flags[0] >> 4) + (header.flags[1] & 0xF0);

	printf("mapper: %X\n", mapper);

	if (header.flags[0] & HAS_TRAINER) {
		// TODO figure out wtf a trainer is
		// just throw it away for now
		fseek(rom, 0x200, SEEK_CUR);
	}

	if (!(chr_rom = malloc(CHR_ROM_CHUNKSIZE * header.chr_rom_chunks))) {
		fprintf(stderr, "%s: out of memory!\n", argv[0]);
		goto error;
	}

	if (fread(prg_rom, PRG_ROM_CHUNKSIZE, header.prg_rom_chunks, rom) !=
	    header.prg_rom_chunks)
		goto read_error;

	if (fread(chr_rom, CHR_ROM_CHUNKSIZE, header.chr_rom_chunks, rom) !=
	    header.chr_rom_chunks)
		goto read_error;

	fclose(rom);
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
