/*************
	UWXtract - Ultima Underworld Extractor

	UW2 PLAYER.DAT decoder/extractor

	Extracts the raw bytes as a CSV - just exists to assist with creating SAVXtract and will ultimately be removed

	Note:  Hardcoded to SAVE1
*************/
#include "UWXtract.h"
#include <vector>

extern std::string ByteToBitArray(const unsigned char ByteIn);	// Util.cpp

int RAWXtractUW2(
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

// Create export files and set headers
	CreateFolder(OutPath);

	sprintf(TempPath, "%s\\RAW%u_Data.csv", OutPath.c_str(), 1);
	FILE* SaveOut = fopen(TempPath, "w");
	fprintf(SaveOut, "Offset,HexVal,BinVal,DecVal\n");

	sprintf(TempPath, "%s\\RAW%u_Inventory.csv", OutPath.c_str(), 1);
	FILE* InvOut = fopen(TempPath, "w");
	fprintf(InvOut, "ObjectID,x00,x02,x04,x06\n");

	sprintf(TempPath, "%s\\SAVE%u\\PLAYER.DAT", UWPath.c_str(), 1);
	FILE* SAVFile = fopen(TempPath, "rb");


// Create data buffers
	std::vector<unsigned char> SAVData(995);

// UW2 stores the inventory object count in the unencrypted data so we can use that instead of calculating off of file size -- Note:  Count is inventory objects + 1 - I'm assuming it's padding in the player object and so I will too
	fseek(SAVFile, 894, SEEK_SET);
	int ItemCount = fgetc(SAVFile);
	fseek(SAVFile, 0, SEEK_SET);

	std::vector<std::vector<unsigned short int>> ItemData(ItemCount, std::vector<unsigned short int>(4));

// Decrypt data
	{
	// Read in raw data
		std::vector<unsigned char> InBuffer(995);
		for (int i = 0; i < 995; i++) {
			InBuffer[i] = fgetc(SAVFile);
		}

	// Get inventory objects
		for (int i = 1; i < ItemCount; i++) {
			unsigned short int ItemByte[4];
			fread(ItemByte, sizeof(short int), 4, SAVFile);
			for (int b = 0; b < 4; b++) {
				ItemData[i][b] = ItemByte[b];
			}
		}

	// Add in player object
		{
			fseek(SAVFile, 896, SEEK_SET);
			unsigned short int ItemByte[4];
			fread(ItemByte, sizeof(short int), 4, SAVFile);
			for (int b = 0; b < 4; b++) {
				ItemData[0][b] = ItemByte[b];
			}
		}

	// Get "Magic Byte"
		unsigned char MagicByte = InBuffer[0];
		MagicByte += 7;

	// Build "Magic Array"
		std::vector<int> MagicArray(80);
		for (int i = 0; i < 80; i++) {
			MagicByte += 0x06;
			MagicArray[i] = MagicByte;
		}

		for (int i = 0; i < 16; i++) {
			MagicByte += 0x07;
			MagicArray[i * 5] = MagicByte;
		}

		for (int i = 0; i < 4; i++) {
			MagicByte += 0x29;
			MagicArray[i * 12] = MagicByte;
		}

		for (int i = 0; i < 11; i++) {
			MagicByte += 0x49;
			MagicArray[i * 7] = MagicByte;
		}


	// Add the "Magic Byte" to output -- Note:  Original, not modified value for the array
		SAVData[0] = InBuffer[0];

	// Decrypt data -- 0x0001-0x037D
		int CurrentByte = 1;

	// Data is XORed against the Magic Array in a loop of 80 byte blocks excluding the last block which is only 12 bytes
		for (int b = 0; b < 12; b++) {
			[&] {
			// The first byte in each block is only XORed against the Magic Array
				SAVData[CurrentByte] = (InBuffer[CurrentByte] ^ MagicArray[0]);
				CurrentByte++;

				for (int i = 1; i < 0x50; ++i) {
				// Break out of loop once we've hit byte 0x37D (892) -- The remaining data is unencrypted
					if (CurrentByte == 0x037D) {
						return;
					}

				// Remaining bytes in the block are XORed against the previous byte's decrypted data + the previous byte's encrypted data + the matching Magic Array value
					SAVData[CurrentByte] = (InBuffer[CurrentByte] ^ (SAVData[CurrentByte - 1] + InBuffer[CurrentByte - 1] + MagicArray[i])) & 0xFF;
					CurrentByte++;

				}
			}();
		}

	// Now add remaining unencrypted data
		while (CurrentByte < 995) {
			SAVData[CurrentByte] = InBuffer[CurrentByte];
			CurrentByte++;
		}
	}

// Dump
	for (int v = 0; v < 995; v++) {
		fprintf(
			SaveOut,
			"%04X,"	// Offset
			"%02X,"	// HexVal
			"%s,"	// BinVal
			"%u\n",	// DecVal
			v,					// Offset
			SAVData[v],							// HexVal
			ByteToBitArray(SAVData[v]).c_str(),	// BinVal
			SAVData[v]							// DecVal
		);
	}
	fclose(SaveOut);

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
