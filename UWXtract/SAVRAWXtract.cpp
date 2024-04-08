/*************
	UWXtract - Ultima Underworld Extractor

	PLAYER.DAT decoder/extractor

	Extracts the raw bytes as a CSV - just exists to assist with creating SAVXtract and will ultimately be removed

	Note:  Hardcoded to SAVE1
*************/
#include "UWXtract.h"
#include <vector>

extern std::string ByteToBitArray(const unsigned char ByteIn);	// Util.cpp

int SAVRAWXtract(
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

// Create export files and set headers
	CreateFolder(OutPath);

	sprintf(TempPath, "%s\\SAVERAW%u_Data.csv", OutPath.c_str(), 1);
	FILE* SaveOut = fopen(TempPath, "w");
	fprintf(SaveOut, "Offset,HexVal,BinVal,DecVal\n");

	sprintf(TempPath, "%s\\SAVERAW%u_Inventory.csv", OutPath.c_str(), 1);
	FILE* InvOut = fopen(TempPath, "w");
	fprintf(InvOut, "ObjectID,x00,x02,x04,x06\n");

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


// Get number of items
	fseek(SAVFile, 0, SEEK_END);
	int ItemCount = (ftell(SAVFile) - (311 + FileByteOffset)) / 8;
	fseek(SAVFile, 311 + FileByteOffset, SEEK_SET);

	std::vector< std::vector<unsigned short int>> ItemData (ItemCount, std::vector<unsigned short int>(4));

	for (int i = 0; i < ItemCount; i++) {
		unsigned short int ItemByte[4];
		fread(ItemByte, sizeof(short int), 4, SAVFile);
		for (int b = 0; b < 4; b++) {
			ItemData[i][b] = ItemByte[b];
		}
	}

// Dump
	for (int i = 0; i < ItemCount; i++) {
			fprintf(
				InvOut,
				"%u,"		// ObjectID
				"%04x,"		// x00
				"%04x,"		// x02
				"%04x,"		// x04
				"%04x\n",	// x06
				i + 1,			// ObjectID
				ItemData[i][0],	// x00
				ItemData[i][1],	// x02
				ItemData[i][2],	// x04
				ItemData[i][3]	// x06
			);
	}

	fclose(InvOut);
	fclose(SAVFile);

	printf("PLAYER.DAT (raw) extracted to %s\n", OutPath.c_str());
	return 0;
}
