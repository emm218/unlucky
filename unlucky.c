#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VERSION "0.1"
#define MAGIC_NUMBER 0x1A53454E

struct ines_header {
	uint32_t magic;
	uint8_t prg_rom_size;
	uint8_t chr_rom_size;
	uint8_t flags[5];
	char garbage[5];
};

void usage(void);

int
main(int argc, char **argv)
{
	struct ines_header header;
	FILE *rom;

	if (argc < 2) {
		usage();
		return 1;
	}

	if ((rom = fopen(argv[1], "rb")) == NULL) {
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		return 1;
	}

	if (fread(&header, sizeof(struct ines_header), 1, rom) == 0) {
		if (feof(rom)) {
			fprintf(stderr,
			    "%s: error reading ROM file, unexpected EOF\n",
			    argv[0]);
		} else if (ferror(rom)) {
			fprintf(stderr, "%s: error reading ROM file\n",
			    argv[0]);
		}
		return 1;
	}

	if (header.magic != MAGIC_NUMBER)
		fprintf(stderr, "%s: warning: bad magic number in ROM file\n",
		    argv[0]);

	fclose(rom);

	return 0;
}

void
usage()
{
	fprintf(stderr,
	    "USAGE: unlucky [OPTIONS] FILE\nTry 'unlucky --help' for more information\n");
}
