/*************
	UWXtract - Ultima Underworld Extractor

	BYT image extractor

	Heavily modified version of the "hacking" bytdecode function by the
	Underworld Adventures Team to extract images from BYT files/archives

	Todo:
		Add image description to log
*************/
#include "UWXtract.h"
#include <algorithm>

extern std::string StringToLowerCase(const std::string InString);							// Util.cpp
extern void GetPaletteAll(const std::string UWPath, const bool IsUW2, char* PaletteBuffer);	// Util.cpp

// All BYT images are fullscreen 320x200
const unsigned int Width = 320;
const unsigned int Height = 200;

static std::map<std::string, unsigned int> BYTFilePaletteMap = {
// UW1 Demo -- Currently (and likely permanently) unsupported
	//{"dmain.byt", 0},
	//{"presd.byt", 5},
// UW1
	{"blnkmap.byt", 1},	// Automap background
	{"chargen.byt", 3},	// Create Character background
	{"conv.byt", 0},	// Conversation screenshot -- Not used in game (or anywhere else I'd imagine)
	{"main.byt", 0},	// In game HUD background
	{"opscr.byt", 2},	// Opening screen background
	{"pres1.byt", 5},	// Startup "ORIGIN Presents" screen
	{"pres2.byt", 5},	// Startup "A BLUE SKY PRODUCTIONS Game" screen
	{"win1.byt", 7},	// Victory "Congratulations" message
	{"win2.byt", 7},	// Victory character stats/info screen background
// UW2 LBACK00X.BYT -- Palette comes from the cutscene file
	{"lback000.byt", 0},	// CS000.N02/4	-- Opening -- Left Britain "The Festival of Rebuilding" scene -- Combined with LBACK001 and panned right to left
	{"lback001.byt", 0},	// CS000.N02/4	-- Opening -- Right Britain "The Festival of Rebuilding" scene -- Combined with LBACK000 and panned right to left
	{"lback002.byt", 1},	// CS000.N05/7	-- Opening -- Upper castle courtyard scene -- Combined with LBACK003 and panned top to bottom
	{"lback003.byt", 1},	// CS000.N05/7	-- Opening -- Lower castle courtyard scene -- Combined with LBACK002 and panned top to bottom
	{"lback004.byt", 2},	// CS002.N01/3	-- Ending -- Throne room scene -- Combined with LBACK005 and panned bottom to top
	{"lback005.byt", 2},	// CS002.N01/3	-- Ending -- Throne room scene -- Combined with LBACK004 and panned bottom to top
	{"lback006.byt", 3},	// CS002.N06/10	-- Ending -- Castle balcony scene -- Combined with LBACK007 and panned left to right
	{"lback007.byt", 3}		// CS002.N06/10	-- Ending -- Castle balcony scene -- Combined with LBACK006 and panned left to right
};

// UW2 BYT.ARK
static std::map<unsigned short, unsigned int> BYTARKPaletteMap = {
	{ 0, 1},	// Automap background
	{ 1, 0},	// Create Character background
	{ 2, 0},	// Bartering screen background
	{ 3, 0},	// Unused/Skipped
	{ 4, 0},	// In game HUD background
	{ 5, 0},	// Opening screen background
	{ 6, 5},	// Startup "ORIGIN An Electronic Arts Company Presents" screen
	{ 7, 5},	// Startup "A LookingGlass Technologies Game"
	{ 8, 0},	// Victory "Congratulations" message
	{ 9, 0},	// Victory character stats/info screen background
	{10, 0}		// Unused/Skipped
};

void UncompressUW2BYT(
	const unsigned char* compressedData,
	size_t compressedSize,
	unsigned char* uncompressedData,
	size_t uncompressedSize
) {
	Uint32 destCount = 0;

	if (uncompressedData == NULL) {
		uncompressedSize = 1; // when not passing a buffer, just set dest size to 1 to keep loop running
	}

// compressed data
	Uint8* ubuf = uncompressedData;
	const Uint8* cp = compressedData + 4; // for some reason the first 4 bytes are not used
	const Uint8* ce = compressedData + compressedSize;
	Uint8* up = ubuf;
	Uint8* ue = ubuf + uncompressedSize;

	while (up < ue && cp < ce) {
		unsigned char bits = *cp++;

		for (int i = 0; i < 8; i++, bits >>= 1) {
			if (bits & 1) {
				if (up != NULL) {
					*up++ = *cp++;
				}
				else {
					cp++;
				}

				destCount++;
			}
			else {
				signed int m1 = *cp++; // m1: pos
				signed int m2 = *cp++; // m2: run

				m1 |= (m2 & 0xF0) << 4;

			// correct for sign bit
				if (m1 & 0x800) {
					m1 |= 0xFFFFF000;
				}

			// add offsets
				m2 = (m2 & 0x0F) + 3;
				m1 += 18;

				destCount += m2; // add count

				if (up != NULL) {
					if (m1 > up - ubuf) {
						return; // should not happen
					}

				// adjust pos to current 4k segment
					while (m1 < (up - ubuf - 0x1000)) {
						m1 += 0x1000;
					}

					while (m2-- && up < ue) {
						*up++ = ubuf[m1++];
					}
				}
			}

		// stop if source or destination buffer pointer overrun
			if (up >= ue || cp >= ce) {
				break;
			}
		}
	}
}

void SaveImage(
	const char* BYTFile,
	const std::string OutPath,
	const char* filename,
	char* allPalettes
) {
	FILE* fd = fopen(BYTFile, "rb");

	if (fd == NULL) {
		printf("Could not open file\n");
		return;
	}

	fseek(fd, 0, SEEK_END);
	unsigned long flen = ftell(fd);

	if (flen != 64000) {
		printf("%s: invalid size; %u\n", filename, flen);
		fclose(fd);
		return;
	}

	fseek(fd, 0, SEEK_SET);

	std::vector<unsigned char> bitmap;
	bitmap.resize(flen);
	fread(bitmap.data(), sizeof(unsigned char), flen, fd);

	fclose(fd);

// Get base filename (without extension)
	std::string basename{ filename };
	basename = basename.substr(0, basename.find_last_of("."));

// Get palette index
	unsigned int paletteIndex = BYTFilePaletteMap[StringToLowerCase(filename)];

	char tga_fname[256];
	sprintf(tga_fname, "%s\\%s.tga", OutPath.c_str(), basename.c_str());

	FILE* tga = fopen(tga_fname, "wb");

	TGAWriteHeader(tga, Width, Height, 1, 1);

// Write palette
	fwrite(allPalettes + paletteIndex * 256 * 3, 1, 256 * 3, tga);

	fwrite(bitmap.data(), 1, Width * Height, tga);

	fclose(tga);
}

int BYTXtract(
	const bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

	CreateFolder(OutPath);

// Create log
	sprintf(TempPath, "%s\\BYT.csv", OutPath.c_str());
	FILE* log = fopen(TempPath, "w");
	fprintf(log, "FileName,Index,Width,Height,Type,Palette\n");

	if (!IsUW2) {
	// Get all color palettes -- Only need a few but easier to just grab all
		char AllPalette[8][256 * 3];
		GetPaletteAll(UWPath, IsUW2, &AllPalette[0][0]);

		_finddata_t find;
		intptr_t hnd = _findfirst((UWPath + "\\DATA\\*.BYT").c_str(), &find);

		if (hnd == -1) {
			printf("No BYT files found!\n");
			return -1;
		}

		do {
		// Save image
			sprintf(TempPath, "%s\\DATA\\%s", UWPath.c_str(), find.name);
			SaveImage(TempPath, OutPath, find.name, &AllPalette[0][0]);

		// Get palette for logging
			unsigned int PaletteIndex = BYTFilePaletteMap[StringToLowerCase(find.name)];

		// Log results
			fprintf(
				log,
				"%s,"					// FileName
				"0,"					// Index -- 1 image per file in UW1
				"320,"					// Width
				"200,"					// Height
				"8-bit uncompressed,"	// Type -- No real type but basically what these are
				"%u\n",					// Palette
				find.name,		// FileName
				PaletteIndex	// Palette
			);

		} while (0 == _findnext(hnd, &find));

		_findclose(hnd);
	}
	else {
	// BYT.ARK
		{
		// Get all color palettes -- Only need a few but easier to just grab all
			char AllPalette[11][256 * 3];
			GetPaletteAll(UWPath, IsUW2, &AllPalette[0][0]);

			sprintf(TempPath, "%s\\DATA\\BYT.ARK", UWPath.c_str());
			FILE* BYTARK = fopen(TempPath, "rb");

			if (BYTARK == NULL) {
				printf("Could not open BYT.ARK\n");
				return -1;
			}

		// Get file size
			fseek(BYTARK, 0, SEEK_END);
			unsigned int FileLength = ftell(BYTARK);
			fseek(BYTARK, 0, SEEK_SET);

		// Get image count
			unsigned short entries;
			fread(&entries, sizeof(short), 1, BYTARK);
			fseek(BYTARK, 4, SEEK_CUR); // Skip blank int

		// Get image offsets
			std::vector<unsigned int> offsets;
			offsets.resize(entries);
			fread(offsets.data(), sizeof(unsigned int), entries, BYTARK);

			unsigned char* entryFlags = new unsigned char[entries];
			fread(entryFlags, sizeof(unsigned char), entries, BYTARK);

			std::vector<unsigned int> dataSizes;
			dataSizes.resize(entries);
			fread(dataSizes.data(), sizeof(unsigned int), entries, BYTARK);

			for (unsigned short offsetIndex = 0; offsetIndex < entries; offsetIndex++) {
				if (offsets[offsetIndex] == 0) {
					printf("Skipping image at index %i", offsetIndex);
					continue;
				}

			// Log and skip 2 unused files
				if (offsetIndex == 3 || offsetIndex == 10) {
					fprintf(
						log,
						"BYT.ARK,"	// FileName -- Just hardcode for UW2
						"%u,"		// Index
						","			// Width
						","			// Height
						"Unused,"	// Type
						"\n",		// Palette
						offsetIndex		// Index
					);
					continue;
				}

			// Seek to image data
				fseek(BYTARK, offsets[offsetIndex], SEEK_SET);

				unsigned int compressedSize = offsetIndex < entries - 1
					? offsets[offsetIndex + 1] - offsets[offsetIndex]
					: FileLength - offsets[offsetIndex];

				std::vector<unsigned char> compressedData;
				compressedData.resize(compressedSize);
				fread(compressedData.data(), sizeof(unsigned char), compressedSize, BYTARK);

				std::vector<unsigned char> bitmap;
				bitmap.resize(Width * Height);

				UncompressUW2BYT(compressedData.data(), compressedData.size(), bitmap.data(), bitmap.size());

				unsigned int PaletteIndex = BYTARKPaletteMap[offsetIndex];

				sprintf(TempPath, "%s\\BYTARK_%02u.tga", OutPath.c_str(), offsetIndex);

				FILE* tga = fopen(TempPath, "wb");

				TGAWriteHeader(tga, Width, Height, 1, 1);

				// write palette
				fwrite(AllPalette[PaletteIndex], 1, 256 * 3, tga);

				fwrite(bitmap.data(), 1, Width * Height, tga);

				fclose(tga);

				fprintf(
					log,
					"BYT.ARK,"				// FileName -- Just hardcode
					"%u,"					// Index
					"320,"					// Width
					"200,"					// Height
					"8-bit uncompressed,"	// Type -- No real type but basically what these are
					"%u\n",					// Palette
					offsetIndex,	// Index
					PaletteIndex	// Palette
				);
			} // end for

			fclose(BYTARK);
		}
	// LBACK00X
		{
		 // Get raw palette from cutscene files
			unsigned char CSPalTemp[4][1024];
		// LBACK000/1
			sprintf(TempPath, "%s\\CUTS\\CS000.N02", UWPath.c_str());
			FILE* CSFile = fopen(TempPath, "rb");
			fseek(CSFile, 0x100, SEEK_SET);
			fread(CSPalTemp[0], 1, 1024, CSFile);
			fclose(CSFile);

		// LBACK002/3
			sprintf(TempPath, "%s\\CUTS\\CS000.N05", UWPath.c_str());
			CSFile = fopen(TempPath, "rb");
			fseek(CSFile, 0x100, SEEK_SET);
			fread(CSPalTemp[1], 1, 1024, CSFile);
			fclose(CSFile);

		// LBACK004/5
			sprintf(TempPath, "%s\\CUTS\\CS002.N01", UWPath.c_str());
			CSFile = fopen(TempPath, "rb");
			fseek(CSFile, 0x100, SEEK_SET);
			fread(CSPalTemp[2], 1, 1024, CSFile);
			fclose(CSFile);

		// LBACK006/7
			sprintf(TempPath, "%s\\CUTS\\CS002.N06", UWPath.c_str());
			CSFile = fopen(TempPath, "rb");
			fseek(CSFile, 0x100, SEEK_SET);
			fread(CSPalTemp[3], 1, 1024, CSFile);
			fclose(CSFile);

		// Strip 00 padding
			char CSPalette[4][768];
			for (int p = 0; p < 4; p++) {
				for (int i = 0; i < 256; i++) {
					CSPalette[p][i * 3] = CSPalTemp[p][i * 4];
					CSPalette[p][i * 3 + 1] = CSPalTemp[p][(i * 4) + 1];
					CSPalette[p][i * 3 + 2] = CSPalTemp[p][(i * 4) + 2];

				}
			}

			_finddata_t find;
			intptr_t hnd = _findfirst((UWPath + "\\CUTS\\LBACK00?.BYT").c_str(), &find);

			if (hnd == -1) {
				printf("No LBACK files found!\n");
				return -1;
			}

			do {
			// Save image
				sprintf(TempPath, "%s\\CUTS\\%s", UWPath.c_str(), find.name);
				SaveImage(TempPath, OutPath, find.name, &CSPalette[0][0]);

			// Get palette for logging
				std::string LBPalette = "";
				if (_stricmp("lback000.byt", find.name) == 0 || _stricmp("lback001.byt", find.name) == 0) {
					LBPalette = "CS000.N02";
				}
				else if (_stricmp("lback002.byt", find.name) == 0 || _stricmp("lback003.byt", find.name) == 0) {
					LBPalette = "CS000.N05";
				}
				else if (_stricmp("lback004.byt", find.name) == 0 || _stricmp("lback005.byt", find.name) == 0) {
					LBPalette = "CS002.N01";
				}
				else if (_stricmp("lback006.byt", find.name) == 0 || _stricmp("lback007.byt", find.name) == 0) {
					LBPalette = "CS002.N06";
				}

			// Log results
				fprintf(
					log,
					"%s,"					// FileName
					"0,"					// Index -- 1 image per LBACK file
					"320,"					// Width
					"200,"					// Height
					"8-bit uncompressed,"	// Type -- No real type but basically what these are
					"%s\n",					// Palette
					find.name,			// FileName
					LBPalette.c_str()	// Palette
				);

			} while (0 == _findnext(hnd, &find));

			_findclose(hnd);
		}
	}
	fclose(log);

	printf("BYT images extracted to %s\n", OutPath.c_str());
	return 0;
}
