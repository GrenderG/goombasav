// goombasav command line version.
// Works with gcc, g++, and Visual Studio 2013 (for the latter, use /TP to compile as C++ code)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "goombasav.h"
#include "platformname.h"

const char* USAGE = "goombasav (2014-05-13)\n"
"Usage: goombasav {x/extract} gba.sav gbc.sav\n"
"       goombasav {r/replace} gba.sav gbc.sav\n"
"       goombasav {c/clean} gba.sav [output.sav]\n"
"       goombasav gba.sav\n"
"\n"
"  x: extract save data from first file -> store in second file\n"
"     (\"gbc.sav\" can be - for stdout)\n"
"  r: replace data in first file <- read from second file\n"
"  c: clean sram at 0xE000 in first file -> write to second file if specified,\n"
"     replace first file otherwise (second file can be - for stdout)\n"
"\n"
"  one argument: view Goomba headers\n"
"                (file can be - for stdin)\n"
"\n"
"  -L: license information\n"
"  /? or --help: print this message\n";

const char* GPL_NOTICE = "goombasav - extract and replace Goomba/Goomba Color save files\n"
"Copyright (C) 2014 libertyernie\n"
"https://github.com/libertyernie/goombasav\n"
"\n"
"This program is free software: you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation, either version 3 of the License, or\n"
"(at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program. If not, see <http://www.gnu.org/licenses/>.\n";

void usage() {
	fprintf(stderr, USAGE);
	exit(EXIT_FAILURE);
}

void license() {
	fprintf(stdout, GPL_NOTICE);
#ifdef PLATFORM_NAME
	fprintf(stdout, "\nCompiled for %s\n", PLATFORM_NAME);
#endif
#ifdef ARCH_NAME
	fprintf(stdout, "Arch: %s\n", ARCH_NAME);
#endif
	exit(EXIT_FAILURE);
}

void could_not_open(const char* filename) {
	fprintf(stderr, "Could not open file: %s\n", filename);
	exit(EXIT_FAILURE);
}

void print_indent(const char* prefix, const char* multiline_str) {
	printf("  ");
	while (*multiline_str != '\0') {
		putc(*multiline_str, stdout);
		if (*multiline_str == '\n') {
			printf("  ");
		}
		multiline_str++;
	}
	putc('\n', stdout);
}

stateheader* ask(const void* first_header, const char* prompt) {
	stateheader** headers = stateheader_scan(first_header);
	if (headers == NULL) {
		fprintf(stderr, "Error: %s", goomba_last_error());
		exit(EXIT_FAILURE);
	}
	if (headers[0] == NULL) {
		fprintf(stderr, "No headers found\n");
		exit(EXIT_FAILURE);
	}

	int i = 0;
	while (headers[i] != NULL) {
		fprintf(stderr, "%d. %s\n", i, stateheader_summary_str(headers[i]));
		i++;
	}

	int index;
	fprintf(stderr, "%s", prompt);
	fflush(stderr);
	int dump;
	while (scanf("%d", &index) != 1 || index < 0 || index >= i) {
		fprintf(stderr, "Invalid number entered - try again: ");
		fflush(stderr);
		while ((dump = getchar()) != '\n');
	}

	stateheader* selected = headers[index];
	free(headers);
	return selected;
}

void extract(const char* gbafile, const char* gbcfile) {
	FILE* gba = fopen(gbafile, "rb");
	if (gba == NULL) could_not_open(gbafile);

	char* gba_data = (char*)malloc(GOOMBA_COLOR_SRAM_SIZE);
	fread(gba_data, 1, GOOMBA_COLOR_SRAM_SIZE, gba);
	fclose(gba);

	stateheader* sh = ask(gba_data + 4, "Extract: ");
	fprintf(stderr, "%s\n", stateheader_str(sh));
	goomba_size_t uncompressed_size;

	void* gbc_data = goomba_extract(gba_data, sh, &uncompressed_size);
	if (gbc_data == NULL) {
		fprintf(stderr, "Error: %s", goomba_last_error());
		exit(EXIT_FAILURE);
	}

	free(gba_data);

	FILE* gbc = (strcmp("-", gbcfile) == 0)
		? stdout
		: fopen(gbcfile, "wb");
	if (gbc == NULL) could_not_open(gbcfile);
	fwrite(gbc_data, 1, uncompressed_size, gbc);
	free(gbc_data);
	if (gbc != stdout) fclose(gbc);
}

void replace(const char* gbafile, const char* gbcfile) {
	FILE* gba = fopen(gbafile, "rb");
	if (gba == NULL) could_not_open(gbafile);

	FILE* gbc = fopen(gbcfile, "rb");
	if (gbc == NULL) could_not_open(gbcfile);

	char* gba_data = (char*)malloc(GOOMBA_COLOR_SRAM_SIZE);
	fread(gba_data, GOOMBA_COLOR_SRAM_SIZE, 1, gba);
	fclose(gba);

	stateheader* sh = ask(gba_data + 4, "Replace: ");
	fprintf(stderr, "%s\n", stateheader_str(sh));
	fseek(gbc, 0, SEEK_END);
	size_t gbc_length = ftell(gbc);
	fseek(gbc, 0, SEEK_SET);

	void* gbc_data = malloc(gbc_length);
	fread(gbc_data, gbc_length, 1, gbc);
	fclose(gbc);

	void* new_gba_sram = goomba_new_sav(gba_data, sh, gbc_data, gbc_length);
	if (new_gba_sram == NULL) {
		fprintf(stderr, "Error: %s", goomba_last_error());
		exit(EXIT_FAILURE);
	}
	gba = fopen(gbafile, "wb");
	fseek(gba, 0, SEEK_SET);
	fwrite(new_gba_sram, 1, GOOMBA_COLOR_SRAM_SIZE, gba); // Subtract diff from GOOMBA_COLOR_SRAM_SIZE to keep the file at 65536 bytes
	fclose(gba);
	free(new_gba_sram);
	free(gba_data);
	free(gbc_data);
}

// infile and outfile are not open at the same time, so they can be the same file path.
void clean(const char* infile, const char* outfile) {
	FILE* gba1 = fopen(infile, "rb");
	if (gba1 == NULL) could_not_open(infile);

	char* gba_data = (char*)malloc(GOOMBA_COLOR_SRAM_SIZE);
	fread(gba_data, GOOMBA_COLOR_SRAM_SIZE, 1, gba1);
	fclose(gba1);

	void* new_gba_data = goomba_cleanup(gba_data);
	if (new_gba_data == NULL) {
		fprintf(stderr, "Error: %s", goomba_last_error());
		exit(EXIT_FAILURE);
	} else if (new_gba_data == gba_data) {
		fprintf(stderr, "File is already clean - copying\n");
	}

	FILE* gba2 = strcmp("-", outfile) == 0
		? stdout
		: fopen(outfile, "wb");
	if (gba2 == NULL) could_not_open(outfile);
	fwrite(new_gba_data, 1, GOOMBA_COLOR_SRAM_SIZE, gba2);
	if (gba2 != stdout) fclose(gba2);
}

void list(const char* gbafile) {
	FILE* gba = (strcmp("-", gbafile) == 0)
		? stdin
		: fopen(gbafile, "rb");
	if (gba == NULL) could_not_open(gbafile);

	char* gba_data = (char*)malloc(GOOMBA_COLOR_SRAM_SIZE);
	fread(gba_data, 1, GOOMBA_COLOR_SRAM_SIZE, gba);
	stateheader** headers = stateheader_scan(gba_data);
	if (headers == NULL) {
		fprintf(stderr, "Error: %s", goomba_last_error());
		exit(EXIT_FAILURE);
	}
	if (headers[0] == NULL) {
		fprintf(stderr, "No headers found\n");
		exit(EXIT_FAILURE);
	}

	int i = 0;
	while (headers[i] != NULL) {
		printf("%d. ", i);
		printf("%s\n", stateheader_summary_str(headers[i]));
		print_indent("  ", stateheader_str(headers[i]));
		if (little_endian_conv_16(headers[i]->size) > sizeof(stateheader)) {
			printf("  [3-byte compressed data hash: %6X]\n", goomba_compressed_data_checksum(headers[i], 3));
		}
		i++;
	}

	free(headers);
	if (gba != stdin) fclose(gba);
}

int main(int argc, char** argv) {
	if (argc > 4 || argc < 2) usage();

	if (argc == 2) {
		if (strcmp("--help", argv[1]) == 0 || strcmp("/?", argv[1]) == 0) {
			usage();
		} else if (strcmp("-L", argv[1]) == 0) {
			license();
		} else {
			list(argv[1]);
		}
	} else if (argc == 3) {
		if (strcmp("c", argv[1]) == 0 || strcmp("clean", argv[1]) == 0) {
			clean(argv[2], argv[2]);
		} else {
			usage();
		}
	} else if (strcmp("x", argv[1]) == 0 || strcmp("extract", argv[1]) == 0) {
		extract(argv[2], argv[3]);
	} else if (strcmp("r", argv[1]) == 0 || strcmp("replace", argv[1]) == 0) {
		replace(argv[2], argv[3]);
	} else if (strcmp("c", argv[1]) == 0 || strcmp("clean", argv[1]) == 0) {
		clean(argv[2], argv[3]);
	} else {
		usage();
	}
	return 0;
}
