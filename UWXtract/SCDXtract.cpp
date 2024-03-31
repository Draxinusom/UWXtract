/*************
	UWXtract - Ultima Underworld Extractor

	SCD.ARK decoder/extractor

	Not sure "decoder" is right as I haven't the slightest what any of the shit in this file means
	It does most certainly extracts it though :)

	Todo:
		Figure out what this shit means
*************/
#include "UWXtract.h"

extern bool AvailSaveGame(std::string UWPath, int SaveID);	// Util.cpp
extern std::string ByteToBitArray(const unsigned char ByteIn);	// Util.cpp

int ProcessSCD(
	char* SourcePath,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[255];

	sprintf(TempPath, "%s\\%s_Header.csv", OutPath.c_str(), SourcePath);
	FILE* SCDHeaderOut = fopen(TempPath, "w");
	fprintf(SCDHeaderOut, "Block,Offset,DataSize,TotalSize,CanCompress,IsCompressed,IsPadded,ActiveRecords,UnusedRecords\n");

/***
	Since no one seems to really know what this file is/does, the data is really only useful for trying to reverse engineer

	With that in mind, I think it'll probably be handy/helpful to provide the data as both the raw bytes and as a bit array
	Since doubling up the columns will make it bit unwieldy to work with, I'll split into two files
***/
	sprintf(TempPath, "%s\\%s_Byte.csv", OutPath.c_str(), SourcePath);
	FILE* SCDByteOut = fopen(TempPath, "w");
	fprintf(SCDByteOut, "Block,ID,IsActive,x00,x01,x02,x03,x04,x05,x06,x07,x08,x09,x0A,x0B,x0C,x0D,x0E,x0F\n");

	sprintf(TempPath, "%s\\%s_Binary.csv", OutPath.c_str(), SourcePath);
	FILE* SCDBinOut = fopen(TempPath, "w");
	fprintf(SCDBinOut, "Block,ID,IsActive,x00,x01,x02,x03,x04,x05,x06,x07,x08,x09,x0A,x0B,x0C,x0D,x0E,x0F\n");

	sprintf(TempPath, "%s\\%s\\SCD.ARK", UWPath.c_str(), SourcePath);
	FILE* SCDARK = fopen(TempPath, "rb");

// First two bytes is the number of blocks in the file
	unsigned short ActiveBlock;
	fread(&ActiveBlock, sizeof(unsigned short), 1, SCDARK);

// Skip over blank block (maybe a self reference to the header as a block?)
	fseek(SCDARK, 6, SEEK_SET);

/***
	Storing all header data in 1 array
	0:	Offset to Block
	1:	Flags			-- Don't believe this file ever compresses so always 00, including just in case
	2:	DataSize		-- Record count it this minus 4 (ActiveRecord int at the beginning) divided by 16
	3:	AvailableSize	-- Since it doesn't compress this should always equal DataSize
	4:	ActiveRecords	-- Technically not part of header but makes sense to report here

	ActiveRecords:
	I'm assuming that's what it is as it matches up row/record count wise and similar logic is applied in other ARK files
	All blocks (including empty ones) start with 20 unused records -- Wrong, now believe it is either not that many records and first (at least) is a full header or is a separate sub block
	Similar to objects in LEV.ARK, records fill from the bottom up
***/
	unsigned int HeaderData[16][5];

// Get standard header data
	for (int v = 0; v < 4; v++) {
		for (int b = 0; b < ActiveBlock; b++) {
			unsigned int HeaderBuffer;
			fread(&HeaderBuffer, sizeof(unsigned int), 1, SCDARK);

			HeaderData[b][v] = HeaderBuffer;
		}
	}
// Get Active... whatever?
	for (int b = 0; b < ActiveBlock; b++) {
		fseek(SCDARK, HeaderData[b][0], SEEK_SET);
		unsigned int HeaderBuffer;
		fread(&HeaderBuffer, sizeof(short int), 1, SCDARK);	// Note, this is a u16 value, not int
		HeaderData[b][4] = HeaderBuffer;
	}

// Dump header details to CSV
	for (int b = 0; b < ActiveBlock; b++) {
		fprintf(
			SCDHeaderOut,
			"%u,"	// Block
			"%04x,"	// Offset
			"%u,"	// DataSize
			"%u,"	// TotalSize
			"%u,"	// CanCompress
			"%u,"	// IsCompressed
			"%u,"	// IsPadded
			"%u,"	// ActiveRecords
			"%u\n",	// UnusedRecords
			b,								// Block
			HeaderData[b][0],				// Offset
			HeaderData[b][2],				// DataSize
			HeaderData[b][3],				// TotalSize
			HeaderData[b][1] & 0x01,		// CanCompress
			(HeaderData[b][1] & 0x02) >> 1,	// IsCompressed
			(HeaderData[b][1] & 0x04) >> 2,	// IsPadded
			HeaderData[b][4],				// ActiveRecords
			((HeaderData[b][2] - 4) / 16) - HeaderData[b][4]	// UnusedRecords -- Not sure anymore what this is
		);
	}
// Done with the header output
	fclose(SCDHeaderOut);

// Loop and dump block data for both CSV outputs
	for (int b = 0; b < ActiveBlock; b++) {
		fseek(SCDARK, (HeaderData[b][0] + 4), SEEK_SET);	// Note - Skip 4 byte active record block header - This is not right but leaving as is for now

		int TotalRecords = ((HeaderData[b][2] - 4) / 16);

	// Loop each record
		for (int r = 0; r < TotalRecords; r++) {
		// Add block/record ID & active flag for both output files
		// Byte
			fprintf(
				SCDByteOut,
				"%u,"	// Block
				"%u,"	// ID
				"%u,",	// IsActive
				b,
				r,
				r >= TotalRecords - HeaderData[b][4] ? 1 : 0
			);
		// Bin
			fprintf(
				SCDBinOut,
				"%u,"	// Block
				"%u,"	// ID
				"%u,",	// IsActive
				b,
				r,
				r >= TotalRecords - HeaderData[b][4] ? 1 : 0
			);

		// No idea where, if any, of them are combined as 16/32 (though I suspect 00/01 is a 16)
		// So dump them all as single byte columns by looping through each byte
			for (int d = 0; d < 16; d++) {
			// Capture the byte as we need to export it twice
				unsigned char RecordByte = fgetc(SCDARK);

			// Byte
				fprintf(
					SCDByteOut,
					"%02X%s",	// x0?
					RecordByte,
					d == 15 ? "\n" : ","	// Switch to new line on last byte
				);
			// Bin
				fprintf(
					SCDBinOut,
					"%s%s",	// x0?
					ByteToBitArray(RecordByte).c_str(),
					d == 15 ? "\n" : ","	// Switch to new line on last byte
				);
			}
		}

	}
// Finally close out
	fclose(SCDByteOut);
	fclose(SCDBinOut);
	fclose(SCDARK);

	return 0;
}

int SCDXtract(
	std::string ExportTarget,
	const std::string UWPath,
	const std::string OutPath
) {
// Using this to get isdigit to behave for save check (I'm certainly doing something wrong but this works and tired of messing with it)
	char SaveID[1];
	sprintf(SaveID, "%s", ExportTarget.c_str());

// Export all
	if (ExportTarget == "*") {
	// Create output folder (done here in case invalid parameter passed)
		CreateFolder(OutPath);

		ProcessSCD("DATA", UWPath, OutPath);

	// Loop through and export any saves that exist -- Note:  Skipping temp SAVE0 when exporting all (not really all I guess :P)
		for (int s = 1; s < 5; s++) {
			if (AvailSaveGame(UWPath, s)) {
				char SavePath[5];
				sprintf(SavePath, "SAVE%u", s);

				ProcessSCD(SavePath, UWPath, OutPath);
			}
		}
	}
// Data only
	else if (ExportTarget == "d" || ExportTarget == "D") {
	// Create output folder (done here in case invalid parameter passed)
		CreateFolder(OutPath);
		ProcessSCD("DATA", UWPath, OutPath);
	}
// Error if non numeric value passed
	else if (isdigit(SaveID[0]) == 0) {
		printf("Invalid SAVE specified.  Valid values are 1-4.\n");
		return -1;
	}
// Error if out of range
	else if (stoi(ExportTarget) > 4) {
		printf("Invalid SAVE specified.  Valid values are 1-4.\n");
		return -1;
	}
// Error if temp SAVE0 specified and not found -- Point out game/save needs run/loaded
	else if (stoi(ExportTarget) == 0 && !AvailSaveGame(UWPath, stoi(ExportTarget))) {
		printf("No data for SAVE0 found.  Make sure game is currently running with a save loaded.\n");
		return -1;
	}
// Error if specified save does not exist
	else if (!AvailSaveGame(UWPath, stoi(ExportTarget))) {
		printf("No data for SAVE%u found.\n", stoi(ExportTarget));
		return -1;
	}
// Export specified save
	else {
	// Create output folder (done here in case invalid parameter passed)
		CreateFolder(OutPath);

		char SavePath[255];
		sprintf(SavePath, "SAVE%u", stoi(ExportTarget));

		ProcessSCD(SavePath, UWPath, OutPath);
	}

	printf("SCD.ARK extracted to %s\n", OutPath.c_str());
	return 0;
}
