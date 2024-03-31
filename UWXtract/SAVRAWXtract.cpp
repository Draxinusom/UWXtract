/*************
	UWXtract - Ultima Underworld Extractor

	PLAYER.DAT decoder/extractor

	Extracts the raw bytes as a CSV - just exists to assist with creating SAVXtract and will ultimately be removed

	Note:  Hardcoded to SAVE1
*************/
#include "UWXtract.h"

extern std::string ByteToBitArray(const unsigned char ByteIn);	// Util.cpp

int SAVRAWXtract(
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

// Create export files and set headers
	CreateFolder(OutPath);

	sprintf(TempPath, "%s\\SAVERAW%u.csv", OutPath.c_str(), 1);
	FILE* SaveOut = fopen(TempPath, "w");
	fprintf(SaveOut, "Offset,HexVal,BinVal,DecVal\n");

	sprintf(TempPath, "%s\\SAVE%u\\PLAYER.DAT", UWPath.c_str(), 1);
	FILE* SAVFile = fopen(TempPath, "rb");

// Check if encrypted (only initial DATA file is not
	bool IsEncrypted = strstr(TempPath, "DATA") == NULL;
	unsigned char xorvalue = IsEncrypted ? fgetc(SAVFile) : 0;

// Add this to the byte offset to align exported value with the file (Encrypted has leading XOR value byte)
	int FileByteOffset = IsEncrypted ? 1 : 0;

// Read in SAVE data (excluding items)
	unsigned char SAVData[311];
	fread(SAVData, 1, 311, SAVFile);

// Decrypt
	if (IsEncrypted) {
		unsigned char incrnum = 3;

		for (int i = 0; i <= 219; i++) {
			if (i == 80 || i == 160) {
				incrnum = 3;
			}

			SAVData[i] ^= (xorvalue + incrnum);
			incrnum += 3;
		}
	}

// Dump
	for (int v = 0; v < 311; v++) {
		fprintf(
			SaveOut,
			"%04X,"	// Offset
			"%02X,"	// HexVal
			"%s,"	// BinVal
			"%u\n",	// DecVal
			v + FileByteOffset,					// Offset
			SAVData[v],							// HexVal
			ByteToBitArray(SAVData[v]).c_str(),	// BinVal
			SAVData[v]							// DecVal
		);
	}
	fclose(SaveOut);
	fclose(SAVFile);

	printf("PLAYER.DAT (raw) extracted to %s\n", OutPath.c_str());
	return 0;
}
