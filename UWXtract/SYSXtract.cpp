/*************
	UWXtract - Ultima Underworld Extractor

	Font extractor

	Extracts font (SYS) files to images (1 image per character)
*************/
#include "UWXtract.h"

extern std::string ByteToBitArray(unsigned char ByteIn);	// Util.cpp
extern void GetPalette(const std::string UWPath, const unsigned int PaletteIndex, char PaletteBuffer[256 * 3]);	// Util.cpp

int SYSXtract(
	bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
// Get all font files
	_finddata_t find;
	intptr_t FontList = _findfirst((UWPath + "\\DATA\\FONT*.SYS").c_str(), &find);

	if (FontList == -1) {
		printf("No font files found!\n");
		return -1;
	}

// Reusable variable for setting target file names
	char TempPath[256];

// Get palette 0 -- A bit variable where they're used with some fonts so not going to try matching it to the correct one as there may be multiple, just picking the most common
	char Palette[256 * 3];
	GetPalette(UWPath, 0, Palette);

// Just using white (or closest to) for the character's color -- Note: white is a different index between games in palette 0
	char PalIndex = !IsUW2 ? 0x0B : 0x02;

// Create CSV export logs and header row
	CreateFolder(OutPath);
	sprintf(TempPath, "%s\\FontSummary.csv", OutPath.c_str());
	FILE* SummaryOut = fopen(TempPath, "w");
	fprintf(SummaryOut, "FontName,ByteCount,WidthOfSpace,Height,RowWidth,MaxWidth,TotalChar,UsedChar\n");

	sprintf(TempPath, "%s\\FontCharacter.csv", OutPath.c_str());
	FILE* DetailOut = fopen(TempPath, "w");
	fprintf(DetailOut, "FontName,ID,Character,Width,Height\n");

	do {
	// Set counter for tracking number of used/active characters
		int UsedChar = 0;

	// Get font file name
		char FontName[64];
		strcpy(FontName, find.name);

	// Strip extension
		char *pos = strrchr(FontName, '.');
		if (pos != NULL) {
			*pos = 0;
		}

	// Open target font
		sprintf(TempPath, "%s\\DATA\\%s.SYS", UWPath.c_str(), FontName);
		FILE* FontFile = fopen(TempPath, "rb");

		if (FontFile == NULL) {
			printf("Could not open file\n");
			return 0;
		}

	// Get file length
		fseek(FontFile, 0, SEEK_END);
		unsigned long FileLength = ftell(FontFile);
		fseek(FontFile, 2, SEEK_SET);

	/***
		Header
		0:	ByteCount		-- Number of bytes for each character
		1:	WidthOfSpace	-- Spaces I checked had a width that matches this so not sure the point?
		2:	Height			-- Height for all characters
		3:	RowWidth		-- Character width is included for each character and overrides this so also not sure why this exists
		4:	MaxWidth		-- Wrong for some fonts so use is also questionable
	***/
		unsigned short int FontHeader[5];
		fread(&FontHeader, sizeof(short), 5, FontFile);


		int TotalChar = (FileLength - 12 /* Header */) / (FontHeader[0] + 1 /* CharWidth byte */);
		fprintf(
			SummaryOut,
			"%s,"	// FontName
			"%u,"	// ByteCount
			"%u,"	// WidthOfSpace
			"%u,"	// Height
			"%u,"	// RowWidth
			"%u,"	// MaxWidth
			"%u,",	// TotalChar -- Note: UsedChar and newline added after character loop
			FontName,		// FontName
			FontHeader[0],	// ByteCount
			FontHeader[1],	// WidthOfSpace
			FontHeader[2],	// Height
			FontHeader[3],	// RowWidth
			FontHeader[4],	// MaxWidth
			TotalChar		// TotalChar
		);

	// Create font output folder
		sprintf(TempPath, "%s\\%s", OutPath.c_str(), FontName);
		CreateFolder(TempPath);

	// Loop through each character
		for (int i = 0; i < TotalChar; i++) {
		// Get current position - start of character
			int CurrPos = ftell(FontFile);
		// Skip to and get character width (at the end for some f'ed up reason)
			fseek(FontFile, FontHeader[0], SEEK_CUR);
			short int CharWidth = fgetc(FontFile);

		// Override width with header value for spaces (first couple I checked was the same size but just to be sure)
			if (i == 32) {
				CharWidth = FontHeader[1];
			}

		// Log and break if character is 0 width
			if (CharWidth == 0) {
				fprintf(
					DetailOut,
					"%s,"		// FontName
					"%u,"		// Index
					"Invalid"	// Character
					","			// Width
					"\n",		// Height
					FontName,		// FontName
					i				// Index
				);
				continue;
			}

		// Rewind back to character data
			fseek(FontFile, CurrPos, SEEK_SET);

			char TGAOutBuffer[255];

		// Loop character bytes to built image
			for (int h = 0; h < FontHeader[2]; h++) {
			// Convert byte into a binary string - Probably not the best way to do this but actually works really well
				std::string CurrByte = ByteToBitArray(fgetc(FontFile));
			// Add in the second byte if 2 wide
				if (FontHeader[3] == 2) {
					CurrByte += ByteToBitArray(fgetc(FontFile));
				}

			// Now iterate the binary string, if 1 then use PalIndex if 0 then alpha (0)
				for (int w = 0; w < CharWidth; w++) {
					TGAOutBuffer[(h * CharWidth) + w] = CurrByte.substr(w, 1) == "1" ? PalIndex : 0x00;
				}
			}

		// And then skip past the width char so ready for next character
			fseek(FontFile, 1, SEEK_CUR);

		// Some unused characters _do_ have a width value but entire character is alpha so check if there is any non-alpha pixels (excluding spaces here)
			bool HasData = i == 32 ? true : false; // Set space to true as its check is skipped

			if (i != 32) {
				int CharBytes = FontHeader[2] * CharWidth;

				for (int b = 0; b < CharBytes; b++) {
					if (TGAOutBuffer[b] != 0x00) {
						HasData = true;
						break ;
					}
				}
			}

		// Log and break if character if unused
			if (!HasData) {
				fprintf(
					DetailOut,
					"%s,"		// FontName
					"%u,"		// Index
					"Unused"	// Character
					","			// Width
					"\n",		// Height
					FontName,		// FontName
					i				// Index
				);
				continue;
			}

		// Create character file
			sprintf(TempPath, "%s\\%s\\%03u.tga", OutPath.c_str(), FontName, i);
			FILE* TGAOutFile = fopen(TempPath, "wb");

		// Write header
			TGAWriteHeader(TGAOutFile, CharWidth, FontHeader[2], 1, 1);
		// Write palette
			fwrite(Palette, 1, 256 * 3, TGAOutFile);
		// Write data
			fwrite(TGAOutBuffer, 1, FontHeader[2] * CharWidth, TGAOutFile);
		// And close
			fclose(TGAOutFile);

		// Get the actual character
			char Character[6];
			sprintf(Character, "%c", char(i) + 0);

		// Fix Space/DoubleQuote/Comma
			if (i == 32) {
				sprintf(Character, "<SPC>");
			}
			else if (i == 34) {
				sprintf(Character, "\"\"\"\"");
			}
			else if (i == 44) {
				sprintf(Character, "\",\"");
			}

		// Log results
			fprintf(
				DetailOut,
				"%s,"	// FontName
				"%u,"	// Index
				"%s,"	// Character
				"%u,"	// Width
				"%u\n",	// Height
				FontName,		// FontName
				i,				// Index
				Character,		// Character
				CharWidth,		// Width
				FontHeader[2]	// Height
			);

		// Increment used character tracker
			UsedChar += 1;
		}

	// Add UsedChar to summary log
		fprintf(
			SummaryOut,
			"%u\n",	// UsedChar
			UsedChar
		);

		fclose(FontFile);
	} while (0 == _findnext(FontList, &find));
	_findclose(FontList);

	fclose(SummaryOut);
	fclose(DetailOut);

	printf("Font character images extracted to %s\n", OutPath.c_str());
	return 0;
}
