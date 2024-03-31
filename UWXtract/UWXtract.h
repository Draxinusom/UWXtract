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
	FILE* fd,
	unsigned short width,
	unsigned short height,
	unsigned char type = 2,
	unsigned char colmap = 0,
	bool bottomup = false
) {
#pragma pack(push,1)

// TGA header struct
	struct tgaheader {
		unsigned char idlength;     // length of id field after header
		unsigned char colormaptype; // 1 if a color map is present
		unsigned char tgatype;      // tga type

	// Colormap not used
		unsigned short colormaporigin;
		unsigned short colormaplength;
		unsigned char colormapdepth;

	// X and Y origin
		unsigned short xorigin, yorigin;
	// Width and height
		unsigned short width, height;

	// Bits per pixel, either 16, 24 or 32
		unsigned char bitsperpixel;
		unsigned char imagedescriptor;
	}
	tgaheader = {
		0,
		colmap,
		type,

		0,
		(unsigned short)(colmap == 1 ? 256 : 0),
		(unsigned char)(colmap == 1 ? 24 : 0),

		0, 0,
		width, height,

		(unsigned char)(colmap == 1 ? 8 : 32),
		(unsigned char)(bottomup ? 0x00 : 0x20)
	};
#pragma pack(pop)

	fwrite(&tgaheader, 1, 18, fd);
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
