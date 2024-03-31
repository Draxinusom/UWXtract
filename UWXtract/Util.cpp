/*************
	UWXtract - Ultima Underworld Extractor

	Miscellaneous utility functions
*************/
#include <string>
#include <filesystem>

std::string StringToLowerCase(
	const std::string InString
) {
	std::string OutString = InString;
	std::transform(OutString.begin(), OutString.end(), OutString.begin(), ::tolower);

	return OutString;
}

std::string StringToUpperCase(
	const std::string InString
) {
	std::string OutString = InString;
	std::transform(OutString.begin(), OutString.end(), OutString.begin(), ::toupper);

	return OutString;
}

// Replaces all instances of the specified text with a new text value
void StringReplace(
	std::string& InString,
	const std::string& OldText,
	const std::string& NewText
) {
	if (InString.empty()) {
		return;
	}

// Iterate and replace each instance of OldText
	size_t CurrentPos = 0;
	while ((CurrentPos = InString.find(OldText, CurrentPos)) != std::string::npos) {
		InString.replace(CurrentPos, OldText.length(), NewText);
		CurrentPos += NewText.length(); // Skip past replaced text to ensure don't double replace or get into loop
	}
}

// Converts a single byte into a bit array string
std::string ByteToBitArray(
	const unsigned char ByteIn
) {
	std::string BitArray = "";
	for (int i = 7; i >= 0; i--) {
		BitArray += (ByteIn & (0x01 << i)) == (0x01 << i) ? "1" : "0";
	}
	return BitArray;
}


// Returns the specified palette, reformatted for export (primarily used just for 0)
void GetPalette(
	const std::string UWPath,
	const unsigned int PaletteIndex,
	char PaletteBuffer[256 * 3]
) {
	FILE* PALS = fopen((UWPath + "\\DATA\\PALS.DAT").c_str(), "rb");

// Seek to and get target palette
	fseek(PALS, PaletteIndex * (256 * 3), SEEK_SET);
	fread(PaletteBuffer, 1, 256 * 3, PALS);
	fclose(PALS);

// Left shift color value 2 to convert from 64 to 256 range
	for (int v = 0; v < 256 * 3; v++) {
		PaletteBuffer[v] <<= 2;
	}

// Then swap palette type from standard RGB to TGA style BGR
	for (int c = 0; c < 256; c++) {
	// First capture the red value
		char RedTemp = PaletteBuffer[c * 3];
	// Then overwrite with blue
		PaletteBuffer[c * 3] = PaletteBuffer[(c * 3) + 2];
	// And finally overwrite blue with red
		PaletteBuffer[(c * 3) + 2] = RedTemp;
	}
}

// Returns all standard palettes, reformatted for export
void GetPaletteAll(
	const std::string UWPath,
	const bool IsUW2,
	char* PaletteBuffer
) {
	FILE* PALS = fopen((UWPath + "\\DATA\\PALS.DAT").c_str(), "rb");

// Read in all palettes
	unsigned int PaletteCount = !IsUW2 ? 8 : 11;
	fread(PaletteBuffer, 1, PaletteCount * (256 * 3), PALS);
	fclose(PALS);

// Loop and adjust each palette
	for (unsigned int PalIndex = 0; PalIndex < PaletteCount; PalIndex++) {
		char* Palette = &PaletteBuffer[PalIndex * (256 * 3)];

	// Left shift color value 2 to convert from 64 to 256 range
		for (int v = 0; v < 256 * 3; v++) {
			Palette[v] <<= 2;
		}

	// Then swap palette type from standard RGB to TGA style BGR
		for (int c = 0; c < 256; c++) {
		// First capture the red value
			char RedTemp = Palette[c * 3];
		// Then overwrite with blue
			Palette[c * 3] = Palette[(c * 3) + 2];
		// And finally overwrite blue with red
			Palette[(c * 3) + 2] = RedTemp;
		}
	}
}

// Checks if a game save exists in the specified slot
bool AvailSaveGame(
	std::string UWPath,
	int SaveID
) {
// Just return false if invalid save -- Note:  SAVE0 is temp storage while playing - Will only export if explicitely called
	if (SaveID > 4) {
		return false;
	}

	bool IsAvail = (std::filesystem::exists(UWPath + "\\SAVE" + std::to_string(SaveID) + "\\PLAYER.DAT"));	// Check file actually exists, not just folder - technically could cause issues if someone deletes shit I guess...
	return IsAvail;
}
