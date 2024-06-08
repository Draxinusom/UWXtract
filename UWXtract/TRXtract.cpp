/*************
	UWXtract - Ultima Underworld Extractor

	TR (Terrain) image extractor

	Heavily modified version of the "hacking" trdecode function by the Underworld Adventures Team
	to extract images from TR files

	Note:  Doors are (sorta) treated as terrain in LEV.ARK but those are contained in GR file and handled by GRXtract
*************/
#include "UWXtract.h"

extern void GetPalette24(const std::string UWPath, const unsigned int PaletteIndex, char PaletteBuffer[256 * 3]);	// Util.cpp
extern void StringReplace(std::string& InString, const std::string& OldText, const std::string& NewText);		// Util.cpp

int TRXtract(
	const bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

	CreateFolder(OutPath);

// Load game strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Create CSV export file and header -- Note:  Attempting to match GR log file format
	sprintf(TempPath, "%s\\TR.csv", OutPath.c_str());
	FILE* log = fopen(TempPath, "w");
	if (!IsUW2) {
		fprintf(log, "FileName,Index,Width,Height,Type,Palette,Description\n");
	}
// UW2 shares textures so use separate column to return both types - not all have both and some are obviously wrong since they're never used for that type but I'm not going to bother filtering those out
	else {
		fprintf(log, "FileName,Index,Width,Height,Type,Palette,FloorDescription,WallDescription\n");
	}

// Get palette 0
	char palette[256 * 3];
	GetPalette24(UWPath, 0, palette);

	_finddata_t find;
	intptr_t hnd = _findfirst((UWPath + "\\DATA\\*.TR").c_str(), &find);

	if (hnd == -1) {
		printf("No TR files found!\n");
		return -1;
	}

	do {
	// Get basename
		char basename[64];
		strcpy(basename, find.name);

		char *pos = strrchr(basename, '.');
		if (pos != NULL) {
			*pos = 0;
		}


	// Get full TR file path/name
		sprintf(TempPath, "%s\\DATA\\%s.TR", UWPath.c_str(), basename);

		FILE* fd = fopen(TempPath, "rb");
		if (fd == NULL) {
			printf("Could not open file\n");
			return -1;
		}

		fseek(fd, 0, SEEK_END);
		unsigned long flen = ftell(fd);
		fseek(fd, 1, SEEK_SET);	// Skip file format byte (2=TR)

		int xyres = fgetc(fd);	// Width/Height -- All images in a file are the same size and square

		unsigned short entries;
		fread(&entries, 1, 2, fd);

	// Create file output folder (UW1 only/UW2 is all in 1 file so not necessary)
		if (!IsUW2) {
			sprintf(TempPath, "%s\\%s", OutPath.c_str(), basename);
			CreateFolder(TempPath);
		}

		unsigned int *offsets = new unsigned int[entries];
		fread(offsets, sizeof(unsigned int), entries, fd);

		for (int i = 0; i < entries; i++) {
			if (offsets[i] >= flen) {
				continue;
			}

			fseek(fd, offsets[i], SEEK_SET);

			char tganame[256];

		// Create output file
			if(!IsUW2) {
				sprintf(tganame, "%s\\%s\\%03u.tga", OutPath.c_str(), basename, i);
			}
			else {
				sprintf(tganame, "%s\\%03u.tga", OutPath.c_str(), i);
			}
			FILE* tga = fopen(tganame, "wb");

		// Write header
			TGAWriteHeader(tga, xyres, xyres, 24);
		// Write palette
			fwrite(palette, 1, 256 * 3, tga);

		// Write data
			int datalen = xyres * xyres;
			char buffer[1024];

			while (datalen > 0) {
				int size = datalen > 1024 ? 1024 : datalen;
				fread(buffer, 1, size, fd);
				fwrite(buffer, 1, size, tga);
				datalen -= size;
			}
			fclose(tga);

		// Log results - Note:  Terrain descriptions don't really work well for file names so only setting here
			if (!IsUW2) {
				std::string TerrainDescription = CleanDisplayName(gs.get_string(10, _stricmp("f16", basename) == 0 || _stricmp("f32", basename) == 0 ? 510 - i : i), true, false);

			// Text qualify strings containing commas
				if (TerrainDescription.find(',') != std::string::npos) {
					StringReplace(TerrainDescription, "\"", "\"\"");
					TerrainDescription = "\"" + TerrainDescription + "\"";
				}

				fprintf(
					log,
					"%s,"	// FileName
					"%u,"	// Index
					"%u,"	// Width
					"%u,"	// Height
					"8-bit uncompressed,"	// Type -- Images don't have a header in TR files to specify but they're handled identically
					"0,"	// Palette -- All terrain use 0 so just hardcode
					"%s\n",	// Description
					basename,					// FileName
					i,							// Index
					xyres,						// Width
					xyres,						// Height
												// Type -- hardcoded
												// Palette -- hardcoded
					TerrainDescription.c_str()	// Description
				);
			}
			else {
				std::string FloorDescription = CleanDisplayName(gs.get_string(10, 510 - i), true, false);
				std::string WallDescription = CleanDisplayName(gs.get_string(10, i), true, false);

			// Text qualify strings containing commas
				if (FloorDescription.find(',') != std::string::npos) {
					StringReplace(FloorDescription, "\"", "\"\"");
					FloorDescription = "\"" + FloorDescription + "\"";
				}
				if (WallDescription.find(',') != std::string::npos) {
					StringReplace(WallDescription, "\"", "\"\"");
					WallDescription = "\"" + WallDescription + "\"";
				}

				fprintf(
					log,
					"%s,"	// FileName
					"%u,"	// Index
					"%u,"	// Width
					"%u,"	// Height
					"8-bit uncompressed,"	// Type -- Images don't have a header in TR files to specify but they're handled identically
					"0,"	// Palette -- All terrain use 0 so just hardcode
					"%s,"	// FloorDescription
					"%s\n",	// WallDescription
					basename,					// FileName
					i,							// Index
					xyres,						// Width
					xyres,						// Height
												// Type -- hardcoded
												// Palette -- hardcoded
					FloorDescription.c_str(),	// FloorDescription
					WallDescription.c_str()		// WallDescription
				);
			}
		}
		delete[] offsets;
		fclose(fd);
	} while (0 == _findnext(hnd, &find));
	_findclose(hnd);
	fclose(log);

	printf("TR image files extracted to %s\n", OutPath.c_str());
	return 0;
}
