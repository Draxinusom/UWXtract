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

// Returns the specified palette, reformatted for TGA export excluding alpha channel (BGR)
void GetPalette24(
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

// Returns the specified palette, reformatted for TGA export including alpha channel (BGRA)
void GetPalette32(
	const std::string UWPath,
	const unsigned int PaletteIndex,
	char PaletteBuffer[256 * 4]
) {
	FILE* PALS = fopen((UWPath + "\\DATA\\PALS.DAT").c_str(), "rb");
	char TempBuffer[256 * 3];

// Seek to and get target palette
	fseek(PALS, PaletteIndex * (256 * 3), SEEK_SET);
	fread(TempBuffer, 1, 256 * 3, PALS);
	fclose(PALS);

// Left shift color value 2 to convert from 64 to 256 range
	for (int v = 0; v < 256 * 3; v++) {
		TempBuffer[v] <<= 2;
	}

// Then swap palette type from standard RGB to TGA style BGR
	for (int c = 0; c < 256; c++) {
	// First capture the red value
		char RedTemp = TempBuffer[c * 3];
	// Then overwrite with blue
		TempBuffer[c * 3] = TempBuffer[(c * 3) + 2];
	// And finally overwrite blue with red
		TempBuffer[(c * 3) + 2] = RedTemp;
	}

// Now populate return buffer, inserting alpha byte
	for (int c = 0; c < 256; c++) {
		PaletteBuffer[(c * 4) + 0] = TempBuffer[(c * 3) + 0];
		PaletteBuffer[(c * 4) + 1] = TempBuffer[(c * 3) + 1];
		PaletteBuffer[(c * 4) + 2] = TempBuffer[(c * 3) + 2];
	// Add alpha byte -- Only first color should be alpha
		PaletteBuffer[(c * 4) + 3] = (c == 0 ? 0x00 : 0xFF);
	}
}


// Returns all standard palettes, reformatted for export -- Only usage is BYTXtract which is 24-bit only so no alpha byte
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

// Used for current level and Moonstone level location in SAVXtractUW2 & level/target level in SCDXtract
std::string UW2LevelName(
	int LevelID
) {
	std::string LevelName = "";

	switch (LevelID) {
		case  1:	LevelName = "Britannia - Castle of Lord British"; break;
		case  2:	LevelName = "Britannia - Castle Basement"; break;
		case  3:	LevelName = "Britannia - Sewer 1"; break;
		case  4:	LevelName = "Britannia - Sewer 2"; break;
		case  5:	LevelName = "Britannia - Sewer 3"; break;
		case  9:	LevelName = "Prison Tower - Basement"; break;
		case 10:	LevelName = "Prison Tower - First Floor"; break;
		case 11:	LevelName = "Prison Tower - Second Floor"; break;
		case 12:	LevelName = "Prison Tower - Third Floor"; break;
		case 13:	LevelName = "Prison Tower - Fourth Floor"; break;
		case 14:	LevelName = "Prison Tower - Fifth Floor"; break;
		case 15:	LevelName = "Prison Tower - Sixth Floor"; break;
		case 16:	LevelName = "Prison Tower - Seventh Floor"; break;
		case 17:	LevelName = "Killorn Keep - Level 1"; break;
		case 18:	LevelName = "Killorn Keep - Level 2"; break;
		case 25:	LevelName = "Ice Caverns - Level 1"; break;
		case 26:	LevelName = "Ice Caverns - Level 2"; break;
		case 33:	LevelName = "Talorus - Level 1"; break;
		case 34:	LevelName = "Talorus - Level 2"; break;
		case 41:	LevelName = "Scintillus Academy - Level 1"; break;
		case 42:	LevelName = "Scintillus Academy - Level 2"; break;
		case 43:	LevelName = "Scintillus Academy - Level 3"; break;
		case 44:	LevelName = "Scintillus Academy - Level 4"; break;
		case 45:	LevelName = "Scintillus Academy - Level 5"; break;
		case 46:	LevelName = "Scintillus Academy - Level 6"; break;
		case 47:	LevelName = "Scintillus Academy - Level 7"; break;
		case 48:	LevelName = "Scintillus Academy - Level 8"; break;
		case 49:	LevelName = "Tomb of Praecor Loth - Level 1"; break;
		case 50:	LevelName = "Tomb of Praecor Loth - Level 2"; break;
		case 51:	LevelName = "Tomb of Praecor Loth - Level 3"; break;
		case 52:	LevelName = "Tomb of Praecor Loth - Level 4"; break;
		case 57:	LevelName = "Pits of Carnage - Prison"; break;
		case 58:	LevelName = "Pits of Carnage - Upper Dungeons"; break;
		case 59:	LevelName = "Pits of Carnage - Lower Dungeons"; break;
		case 65:	LevelName = "Ethereal Void - Color Zone"; break;
		case 66:	LevelName = "Ethereal Void - Purple Zone"; break;
		case 67:	LevelName = "Scintillus Academy - Secure Vault"; break;
		case 68:	LevelName = "Ethereal Void - Yellow Zone"; break;
		case 69:	LevelName = "Ethereal Void"; break;
		case 71:	LevelName = "Ethereal Void - Red Zone"; break;
		case 72:	LevelName = "Killorn Deathtrap"; break;
		case 73:	LevelName = "Tomb of Praecor Loth - Level 3-Alt"; break;
		case 246:	LevelName = "Britannia"; break;
		case 247:	LevelName = "Prison Tower"; break;
		case 248:	LevelName = "Killorn Keep"; break;
		case 249:	LevelName = "Ice Caverns"; break;
		case 250:	LevelName = "Talorus"; break;
		case 251:	LevelName = "Scintillus Academy"; break;
		case 252:	LevelName = "Tomb of Praecor Loth"; break;
		case 253:	LevelName = "Pits of Carnage"; break;
		case 254:	LevelName = "Ethereal Void"; break;
		case 255:	LevelName = "Any/All"; break;
		default:	LevelName = "Unknown"; break;
	};

	return LevelName;
}
