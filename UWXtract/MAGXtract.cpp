/*************
	UWXtract - Ultima Underworld Extractor

	Magic extractor

	Extracts the embedded magic spell data from the UW/UW2 executable

	Note:	Contains all player castable spells plus a few extra -- Need to check but guessing they're either scrolls or wands the player can use

	Todo:
		There's 4 bits I'm not sure what they mean/do but have pretty obvious
		spell type groupings where set so may attempt to sort those out
*************/
#include "UWXtract.h"

// Easier to hardcode vs lookup and strip Stone from the name and handle unused ones (24)
static std::map<unsigned int, std::string> RuneStoneMap = {
	{0, "An"},
	{1, "Bet"},
	{2, "Corp"},
	{3, "Des"},
	{4, "Ex"},
	{5, "Flam"},
	{6, "Grav"},
	{7, "Hur"},
	{8, "In"},
	{9, "Jux"},
	{10, "Kal"},
	{11, "Lor"},
	{12, "Mani"},
	{13, "Nox"},
	{14, "Ort"},
	{15, "Por"},
	{16, "Quas"},
	{17, "Rel"},
	{18, "Sanct"},
	{19, "Tym"},
	{20, "Uus"},
	{21, "Vas"},
	{22, "Wis"},
	{23, "Ylem"},
	{24, ""}
};

int MAGXtract(
	const bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

	CreateFolder(OutPath);

// Get strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Create CSV export file and header
	sprintf(TempPath, "%s\\MAG.csv", OutPath.c_str());
	FILE* MagOut = fopen(TempPath, "w");
	fprintf(MagOut, "SpellID,Circle,SpellName,Rune1,Rune2,Rune3,Class,SubClass,b31,b29,b01,b00\n");

// Magic table is embedded in executable
	sprintf(TempPath, "%s\\UW%s.EXE", UWPath.c_str(), !IsUW2 ? "" : "2");
	FILE* UWEXE = fopen(TempPath, "rb");

/***
	Very possible this is similar to the embedded model data where it is stored in different locations on different versions
	of the executable and will fail (or likely just be completely wrong) if used on a non-GOG version
***/
	unsigned int RecordCount = !IsUW2 ? 0x35 : 0x45;
	unsigned int MTableOffset = !IsUW2 ? 0x059EF0 : 0x066490;
	fseek(UWEXE, MTableOffset, SEEK_SET);


	for (unsigned int s = 0; s < RecordCount; s++) {
		unsigned char SpellData[4];
		fread(SpellData, 1, 4, UWEXE);

		unsigned int SpellVal = (SpellData[3] << 24) | (SpellData[2] << 16) | (SpellData[1] << 8) | SpellData[0];

	// I assume there's a less dumb way to do this
		std::string Circle = "";
		if ((!IsUW2 && s < 6) || (IsUW2 && s < 8)) {
			Circle = "1";
		}
		else if ((!IsUW2 && s < 12) || (IsUW2 && s < 16)) {
			Circle = "2";
		}
		else if ((!IsUW2 && s < 18) || (IsUW2 && s < 24)) {
			Circle = "3";
		}
		else if ((!IsUW2 && s < 24) || (IsUW2 && s < 32)) {
			Circle = "4";
		}
		else if ((!IsUW2 && s < 30) || (IsUW2 && s < 40)) {
			Circle = "5";
		}
		else if ((!IsUW2 && s < 36) || (IsUW2 && s < 48)) {
			Circle = "6";
		}
		else if ((!IsUW2 && s < 42) || (IsUW2 && s < 56)) {
			Circle = "7";
		}
		else if ((!IsUW2 && s < 48) || (IsUW2 && s < 64)) {
			Circle = "8";
		}

	// Export to CSV -- Note:  bits 29, 23, & 02 are unused and always 0 so excluding
		fprintf(
			MagOut,
			"%u,"	// SpellID
			"%s,"	// Circle
			"%s,"	// SpellName
			"%s,"	// Rune1
			"%s,"	// Rune2
			"%s,"	// Rune3
			"%u,"	// Class
			"%u,"	// SubClass
			"%u,"	// b31
			"%u,"	// b30
			"%u,"	// b02
			"%u\n",	// b01
			s,												// SpellID -- Maybe should add 256 to match to string ID but going to leave as is since it's from the EXE's table
			Circle.c_str(),									// Circle
			gs.get_string(6, s + 256).c_str(),				// Spell
			RuneStoneMap[(SpellVal >> 18) & 0x1F].c_str(),	// Rune1
			RuneStoneMap[(SpellVal >> 13) & 0x1F].c_str(),	// Rune2
			RuneStoneMap[(SpellVal >> 8) & 0x1F].c_str(),	// Rune3
			(SpellVal >> 3) & 0x1F,							// Class
			(SpellVal >> 24) & 0x1F,						// SubClass
			SpellVal >> 31,									// b31
			(SpellVal >> 30) & 0x01,						// b30
			(SpellVal >> 1) & 0x01,							// b01
			SpellVal & 0x01									// b00
		);
	}

	fclose(MagOut);
	fclose(UWEXE);

	printf("Magic spell data extracted to %s\n", OutPath.c_str());
	return 0;
}
