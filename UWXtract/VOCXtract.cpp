/*************
	UWXtract - Ultima Underworld Extractor

	VOC extractor

	Converts VOC audio files to WAV

	Note:  The VOC format can (from the looks of it at least) be fairly complex with multiple audio blocks in a single
	file with loop points, video sync markers, etc.  This is intended for the VOC files in UW1/2 only which are all
	the older 1.10 format single 8bit PCM files (except for SP18.VOC in UW2 for some reason - runs at a higher frequency)
	and will not work on files that are using anything more advanced than that.	
*************/
#include "UWXtract.h"
#include <vector>
#include <fstream>

int VOCXtract(
	const std::string UWPath,
	const std::string OutPath
) {
// Get all VOC files
	_finddata_t find;
	intptr_t VOCList = _findfirst((UWPath + "\\SOUND\\*.VOC").c_str(), &find);

	if (VOCList == -1) {
		printf("No VOC files found!\n");
		return -1;
	}

// Reusable variable for setting target file names
	char TempPath[256];

	CreateFolder(OutPath);

// Create generic structure for WAV header
	struct WAVHeader {
		char ChunkID[4];
		unsigned int ChunkSize;
		char Format[4];
		char SubChunk1ID[4];
		unsigned int SubChunk1Size;
		unsigned short AudioFormat;
		unsigned short NumberOfChannels;
		unsigned int SampleRate;
		unsigned int ByteRate;
		unsigned short BlockAlign;
		unsigned short BitsPerSample;
		char SubChunk2ID[4];
		unsigned int SubChunk2Size;
	};

// Initialize and set standard values
	struct WAVHeader OutHeader = {};

	strcpy(OutHeader.ChunkID, "RIFF");
	strcpy(OutHeader.Format, "WAVE");
	strcpy(OutHeader.SubChunk1ID, "fmt ");
	OutHeader.SubChunk1Size = 16;
	OutHeader.AudioFormat = 1;	// 1=PCM
	strcpy(OutHeader.SubChunk2ID, "data");

	do {
	// Get VOC file name
		char VOCName[64];
		strcpy(VOCName, find.name);

	// Strip extension
		char *pos = strrchr(VOCName, '.');
		if (pos != NULL) {
			*pos = 0;
		}

	// Open target VOC
		sprintf(TempPath, "%s\\SOUND\\%s.VOC", UWPath.c_str(), VOCName);

	// Read in VOC file and skip to end
		std::ifstream VOCFile(TempPath, std::ios::in | std::ifstream::binary | std::ios::ate);

	// Create and size vector buffer for file and read it in
		std::vector<unsigned char>VOCData(VOCFile.tellg(), 0);
		VOCFile.seekg(0);
		VOCFile.read(reinterpret_cast<char*>(&VOCData[0]), VOCData.size());

	// Create output WAV
		sprintf(TempPath, "%s\\%s.wav", OutPath.c_str(), VOCName);
		std::ofstream WAVFile;
		WAVFile.open(TempPath, std::ios::out | std::ifstream::binary);

	// Assign header values and write
	  // Check for info block, will need to adjust some offsets for data if so
		unsigned int HeaderSize = VOCData[26] == 0x08 ? 40 : 32;

		unsigned int DataSize;
		unsigned int BitDepth;
		unsigned short Channels;
		unsigned int SampleRate;

		if (HeaderSize == 40) {
			DataSize = VOCData[35] + (VOCData[36] << 8) + (VOCData[37] << 16);
			BitDepth = VOCData[32] == 0x04 ? 16 : 8;
			Channels = VOCData[33] + 1;	// 0=Mono/1=Stereo
			SampleRate = 256000000 / (Channels * (65536 - (VOCData[30] + (VOCData[31] << 8))));
		}
		else {
			DataSize = VOCData[27] + (VOCData[28] << 8) + (VOCData[29] << 16);
			BitDepth = VOCData[31] == 0x04 ? 16 : 8;
			Channels = 1;			
			SampleRate = 1000000 / (256 - VOCData[30]);
		}

		OutHeader.ChunkSize = DataSize + 35;
		OutHeader.NumberOfChannels = Channels;
		OutHeader.SampleRate = SampleRate;
		OutHeader.ByteRate = SampleRate * Channels;
		OutHeader.BlockAlign = Channels * (BitDepth/8);
		OutHeader.BitsPerSample = BitDepth;
		OutHeader.SubChunk2Size = DataSize -2;

		WAVFile.write(reinterpret_cast<char*>(&OutHeader), sizeof(OutHeader));

	// Now strip the VOC header from the data buffer and write the remaining PCM data out
		VOCData.erase(VOCData.begin(), VOCData.begin() + HeaderSize);
		WAVFile.write(reinterpret_cast<char*>(&VOCData[0]), VOCData.size());

	// Close out in/out files
		VOCFile.close();
		WAVFile.close();

	} while (0 == _findnext(VOCList, &find));
	_findclose(VOCList);
	
	printf("VOC audio files converted to %s\n", OutPath.c_str());
	return 0;
}
