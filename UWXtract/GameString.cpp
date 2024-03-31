/*************
	UWXtract - Ultima Underworld Extractor

	Game string loading

	Lightly reformated but otherwise unchanged version of the "hacking" gamestrings function by the
	Underworld Adventures Team used by most functions

	Todo:
		Either include additional functionality from GameStrings/GameStringsImporter.hpp used by LEV/PAKXtract
		and remove those or switch all other functions to use those
*************/
#include "UWXtract.h"

// Huffman node structure
typedef struct ua_huff_node {
	int symbol;
	int parent;
	int left;
	int right;
} ua_huff_node;

typedef struct ua_block_info {
	Uint16 block_id;
	Uint32 offset;
} ua_block_info;

// Reads a 32-bit int
Uint32 fread32(FILE* fd) {
	Uint32 data;
	fread(&data, 1, 4, fd);
	return data;
}

// Reads a 16-bit int
inline Uint16 fread16(FILE* fd) {
	Uint16 data;
	fread(&data, 1, 2, fd);
	return data;
}

void ua_gamestrings::load(
	const char *filename
) {
	FILE* fd = fopen(filename, "rb");
	if (fd == NULL) {
		printf("Could not open file STRINGS.PAK");
		return;
	}

	Uint16 nodenum = fread16(fd);

// Read in node list
	std::vector<ua_huff_node> allnodes;
	allnodes.resize(nodenum);
	for (Uint16 k = 0; k < nodenum; k++) {
		allnodes[k].symbol = fgetc(fd);
		allnodes[k].parent = fgetc(fd);
		allnodes[k].left = fgetc(fd);
		allnodes[k].right = fgetc(fd);
	}

// Number of string blocks
	Uint16 sblocks = fread16(fd);

// Read in all block infos
	std::vector<ua_block_info> allblocks;
	allblocks.resize(sblocks);
	for (int z = 0; z < sblocks; z++) {
		allblocks[z].block_id = fread16(fd);
		allblocks[z].offset = fread32(fd);
	}

	for (Uint16 i = 0; i < sblocks; i++) {
		std::vector<std::string> allblockstrings;

		fseek(fd, allblocks[i].offset, SEEK_SET);

	// Number of strings
		Uint16 numstrings = fread16(fd);

	// All string offsets
		Uint16 *stroffsets = new Uint16[numstrings];
		for (int j = 0; j < numstrings; j++) {
			stroffsets[j] = fread16(fd);
		}

		Uint32 curoffset = allblocks[i].offset + (numstrings + 1) * sizeof(Uint16);

		for (Uint16 n = 0; n < numstrings; n++) {
			fseek(fd, curoffset + stroffsets[n], SEEK_SET);

			char c;
			std::string str;

			int bit = 0;
			int raw = 0;

			do {
			// Starting node
				int node = nodenum - 1;

			// Huffman tree decode loop
				while (char(allnodes[node].left) != -1 && char(allnodes[node].left) != -1) {
					if (bit == 0) {
						bit = 8;
						raw = fgetc(fd);
						if (feof(fd)) {
						// Premature end of file, should not happen
							n = numstrings;
							i = sblocks;
							break;
						}
					}

				// Decide which node is next
					node = raw & 0x80 ? short(allnodes[node].right) : short(allnodes[node].left);
					raw <<= 1;
					bit--;
				}

				if (feof(fd)) {
					break;
				}

			// Have a new symbol
				c = allnodes[node].symbol;
				if (c != '|') {
					str.append(1, c);
				}
			} while (c != '|');

			allblockstrings.push_back(str);
			str.erase();
		}
		delete[] stroffsets;

	// Insert string block
		allstrings[allblocks[i].block_id] = allblockstrings;
	}
	fclose(fd);
}

std::string ua_gamestrings::get_string(
	unsigned int block,
	unsigned int string_nr
) {
	std::string res;

// Try to find string block
	std::map<int, std::vector<std::string> >::iterator iter = allstrings.find(block);

	if (iter != allstrings.end()) {
	// Try to find string in vector
		std::vector<std::string> &stringlist = iter->second;
		if (string_nr < stringlist.size()) {
			res = stringlist[string_nr];
		}
	}
	return res;
}
