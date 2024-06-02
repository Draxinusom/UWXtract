/*************
	UWXtract - Ultima Underworld Extractor

	Standard includes/functions

	Heavily modified version of the "hacking" hacking.h header by the Underworld Adventures Team
*************/
#pragma once

#include <stdio.h>
#include <string.h>
#include <io.h>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <SDL2/SDL.h>

// Game string container class
class ua_gamestrings {
	public:
	// ctor
		ua_gamestrings() {}

	// loads all game strings from a file
		void load(const char *filename);

	// returns a game string
		std::string get_string(unsigned int block, unsigned int string_nr);

	protected:
	// game string container
		std::map<int, std::vector<std::string>> allstrings;
};

// Write TGA file header
inline void TGAWriteHeader(
	FILE* OutFile,
	unsigned short Width,
	unsigned short Height,
	unsigned char Depth = 32,	// BYTXtract is 24 - CRIT/GR/SYS/TR are 32
// These values are used for all images extracted
	unsigned char Type = 1,
	unsigned char HasColorMap = 1,
	unsigned char ImageDataOrder = 0x20	// TopLeft
) {
#pragma pack(push,1)

// TGA header struct
	struct TGAHeader {
		unsigned char IDLength;     // ImageID field bytes after header 0-255 (looks like metadata)
		unsigned char ColorMapType; // Boolean flag if color map included (indexed)
		unsigned char TGAType;
			/***
				0:	No image data (no idea point of this)
				1:	Uncompressed - Color-mapped	(This is only type used in UWXtract)
				2:	Uncompressed - True-color
				3:	Uncompressed - Black/White
				9:	Run-Length - Color-mapped
				10:	Run-Length - True-color
				11:	Run-Length - Black/White
			***/

	// ColorMap -- Should be all 0s if not used but ignored if TGAType set to non-colormap type
		unsigned short ColorMapOffset;	// Offset to first color in map to use -- Should pretty much always be 0
		unsigned short ColorMapLength;	// Number of colors in map (colors, not bytes)
		unsigned char ColorMapDepth;	// Number of bits used for each color (3 * bits + Alpha)

	// X/YOrigin
		unsigned short XOrigin, YOrigin;	// Starting offset of image data based on bits 4-5 of ImageDescriptor byte
	// ImageWidth/Height
		unsigned short Width, Height;		// Image dimensions in pixels

		unsigned char BitsPerPixel;			// Number of bits used per pixel -- Generally 24 or 32 for non-colormap, though 8/16 are standards -- Can be 1-8 for colormap types, depending on size of colormap table
		unsigned char ImageDescriptor;
			/***
				Bits 0-3:	Number of bits per pixel to use for alpha -- Can also mean number of overlay bits but not relevant here
				Bit 4:		Image data order -- 0=Left>Right - 1=Right>Left
				Bit 5:		Image data order -- 0=Bottom>Top - 1=Top>Bottom
				Bits 6-7:	Unused
			***/
	}
	TGAHeader = {
	// IDLength
		0,
	// ColorMapType
		HasColorMap,
	// TGAType
		Type,
	// ColorMapOffset
		0,
	// ColorMapLength
		(unsigned short)(HasColorMap == 1 ? 256 : 0),
	// ColorMapDepth
		(unsigned char)(HasColorMap == 1 ? Depth : 0),
	// XOrigin
		0,
	// YOrigin
		0,
	// ImageWidth
		Width,
	// ImageHeight
		Height,
	// BitsPerPixel
		(unsigned char)(HasColorMap == 1 ? 8 : Depth),
	// ImageDescriptor
		(unsigned char)(ImageDataOrder | (Depth == 32 ? 0x28 : 0x20))
	};
#pragma pack(pop)

	fwrite(&TGAHeader, 1, 18, OutFile);
}

inline std::string TitleCase(
	const std::string InString
) {
	std::string OutString = "";

	unsigned int DelimPos = 0;
	unsigned int CurrentPos = 0;

// Find next delim (space)
	DelimPos = InString.find(' ', CurrentPos);

// Loop through string to break into words to capitalize
	while (DelimPos != std::string::npos) {
		std::string Word = "";

	// Capture next word
		if (CurrentPos != DelimPos) {
			Word = InString.substr(CurrentPos, (DelimPos - CurrentPos));
		}
	// Handle double (or more) spaces
		else {
			Word = InString.substr(CurrentPos, 1);
		}

	// Capitalize Word first letter
		Word[0] = toupper(Word[0]);
	// And append Word + space
		OutString += Word + InString[DelimPos];

	// If not at end set CurrentPos past DelimPos
		if (DelimPos < (InString.length() - 1)) {
			CurrentPos = (DelimPos + 1);
		}
	// Otherwise set CurrentPos to DelimPos and exit loop
		else {
			CurrentPos = DelimPos;
			break;
		}

	// Find next delim
		DelimPos = InString.find(' ', CurrentPos);

	}

// Capture last word (or full string if no spaces)
	std::string Word = InString.substr(CurrentPos, std::string::npos);
	Word[0] = toupper(Word[0]);
	OutString += Word;
	return OutString;
}

inline std::string CleanDisplayName(
	const std::string InName,
	bool IsTitleCase,
	bool RemoveSpace
) {
	std::string CleanName = InName;

// Return if no value
	if (CleanName == "" || CleanName.find_first_not_of(" ") == std::string::npos) {
		return CleanName;
	}

// First trim surrounding space
	CleanName = CleanName.substr(CleanName.find_first_not_of(" "), CleanName.find_last_not_of(" ") - CleanName.find_first_not_of(" ") + 1);

// Strip plural name if exists and no leading article (a/an)
	if(CleanName.find('_') == std::string::npos) {
		CleanName = CleanName.substr(0, (CleanName.find_first_of("&") - 1));
	}
// Strip leading article (a/an) and plural name if exists
	else {
		CleanName = CleanName.substr(CleanName.find_first_of("__") + 1, (CleanName.find_first_of("&") - 1) - CleanName.find_first_of("__"));
	}

// Strip leading "a " if exists
	if (CleanName.substr(0, 2) == "a " || CleanName.substr(0, 2) == "A ") {
		CleanName = CleanName.substr(2, CleanName.length() -2);
	}
   // Strip leading "an " if exists - Except "An Stone"
	if (CleanName != "An stone" && (CleanName.substr(0, 3) == "an " || CleanName.substr(0, 3) == "An ")) {
		CleanName = CleanName.substr(3, CleanName.length() - 3);
	}

// Apply title case if option set
	if(IsTitleCase) {
		CleanName = TitleCase(CleanName);

	// Remove title case on joining words if RemoveSpace is false
		if(!RemoveSpace) {
		// " Of " just looks dumb :P
			if(CleanName.find(" Of ") != std::string::npos) {
				CleanName.replace(CleanName.find(" Of "), 4, " of ");
			}
		// " The " does too
			if(CleanName.find(" The ") != std::string::npos) {
				CleanName.replace(CleanName.find(" The "), 5, " the ");
			}
		}
	}

// Strip all spaces if option set
	if (RemoveSpace) {
		CleanName.erase(std::remove_if(CleanName.begin(), CleanName.end(), [](unsigned char x) { return std::isspace(x); }), CleanName.end());
	}

	return CleanName;
}

inline void CreateFolder(
	const std::string FolderName
) {
	if (!(std::filesystem::exists(FolderName))) {
		std::filesystem::create_directories(FolderName);
	}
}
