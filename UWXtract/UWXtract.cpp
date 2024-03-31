/*************
	UWXtract - Ultima Underworld Extractor

	Main function

	Calls the various extract functions specified in the command line

	Todo:
		Add some logic to validate in/out paths
		Add parameter type flag so I'm not doing something dumb like forcing both parameters to be used in order if 1 is (like I am doing)
			i.e.:
				-i <input/game path>
				-o <output path>
		May make/include variation of the convdec tool - the cnvdec "hacking" function output didn't appear useful
*************/
#include "stdio.h"
#include "string.h"
#include <filesystem>
#include <string>

// Extract functions
extern int BYTXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
//extern int CNVXtract();		// Dropping this or replacing
extern int CRITXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int DATXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int GRXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int LEVXtract(bool IsUW2, std::string ExportTarget, const std::string UWPath, const std::string OutPath);
extern int MDLXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int PAKXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int PALXtract(bool IsUW2, const std::string UWPath, const std::string OutPath, bool IsGIMP);
extern int SAVXtract(std::string ExportTarget, const std::string UWPath, const std::string OutPath);
extern int SCDXtract(std::string ExportTarget, const std::string UWPath, const std::string OutPath);
extern int SYSXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int TRXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);

extern int SAVRAWXtract(const std::string UWPath, const std::string OutPath);

int main(
	int argc,
	char* argv[]
) {
// Show help if no or too many parameters provided or common "help" ones passed
	if (argc < 2 || argc > 4 || strcmp("?", argv[1]) == 0 || strcmp("-?", argv[1]) == 0 || _stricmp("-h", argv[1]) == 0 || _stricmp("help", argv[1]) == 0) {
		printf("UWXtract\n"
			"Ultima Underworld data and image extraction tool\n"
			"\n"
			"Usage:  UWXtract [type] [Game path/Optional] [Output path/Optional]\n"
			"Types:\n"
			"  BYT      BYT bitmap images\n"
			//"  CNV      Conversation logic\n"
			"  CRIT     Critter (Monster/NPC) animation images\n"
			"  DAT      Various data type definition files\n"
			"  FONT     Font files (*.SYS)\n"
			"  GR       GR bitmap images (Objects/UI Elements/Etc)\n"
			"  LEV      Level archive\n"
			"  MDL      3D model details\n"
			"  PAK      Game strings (text)\n"
			"  PAL      Bitmap color palettes\n"
			"  SCD      SCD archive (UW2 only)\n"
			"  SAV      Save game (SAVE#\\PLAYER.DAT)\n"
			"  TR       TR bitmap images (Floor/Wall textures)\n"
			"  *        All types\n"
			"\n"
			"UWXtract and supporting files may be placed in and run from the base game folder (where UW.exe/UW2.exe is located)\n"
			"or the game folder and output folder must be specified\n"
			"\n"
			"Note:  For GOG version, the ISO file (game.gog - you can temporarily rename it to iso) must be mounted\n"
			"       and the game folder specified or you can extract it and run from there\n"
		);
		return -1;
	}
   
// Set default paths -- ADD SOME LOGIC HERE!!!!!!!!!!!!!!!!!
	std::string UWPath = ".\\";
	std::string OutPath = ".\\UWXtract";

	if (argc > 2) {
		if (argv[2] != NULL) {
			UWPath = argv[2];
		}
		if (argv[3] != NULL) {
			OutPath = argv[3];
		}
	}

// Capture incoming parameter as string for LEV/SAV/SCD targeting
	std::string ExportType = argv[1];
// And convert to lower case
	std::transform(ExportType.begin(), ExportType.end(), ExportType.begin(), [](unsigned char c) { return std::tolower(c); });

// Identify game type - assuming UW1 if UW2.exe not found
	bool IsUW2 = (std::filesystem::exists(UWPath + "\\UW2.exe"));

// Break if neither UW/UW2 executable found
	if (!IsUW2 && !(std::filesystem::exists(UWPath + "\\UW.exe"))) {
		printf("Ultima Underworld not found.\nSpecify game path (Where UW.exe/UW2.exe is located) or place executable in base game folder and run from there.");
		return -1;
	}

// Call extract function
	if (strcmp("*", argv[1]) == 0) {
		BYTXtract(IsUW2, UWPath, OutPath + "\\BYT");
		//CNVXtract();
		CRITXtract(IsUW2, UWPath, OutPath + "\\CRIT");
		DATXtract(IsUW2, UWPath, OutPath + "\\DAT");
		SYSXtract(IsUW2, UWPath, OutPath + "\\FONT");
		GRXtract(IsUW2, UWPath, OutPath + "\\GR");
		LEVXtract(IsUW2, "*", UWPath, OutPath + "\\LEV");
		MDLXtract(IsUW2, UWPath, OutPath);	// Single text file export, so don't bother creating sub folder
		PAKXtract(IsUW2, UWPath, OutPath + "\\PAK");
		PALXtract(IsUW2, UWPath, OutPath + "\\PAL", false);
	// SAV is UW1 for now -- Don't think it's worth noting it's skipped here, only if specifically called
		if (!IsUW2) {
			SAVXtract("*", UWPath, OutPath + "\\SAV");
		}
	// SCD is UW2 only -- Don't think it's worth noting it's skipped here, only if specifically called
		if (IsUW2) {
			SCDXtract("*", UWPath, OutPath + "\\SCD");
		}
		TRXtract(IsUW2, UWPath, OutPath + "\\TR");
	}
// BYT
	else if (strcmp("byt", argv[1]) == 0) {
		return BYTXtract(IsUW2, UWPath, OutPath);
	}
//// CNV
	//else if (strcmp("cnv", argv[1]) == 0) {
	//	return CNVXtract();
	//}
// CRIT
	else if (strcmp("crit", argv[1]) == 0) {
		return CRITXtract(IsUW2, UWPath, OutPath);
	}
// DAT
	else if (_stricmp("dat", argv[1]) == 0) {
		return DATXtract(IsUW2, UWPath, OutPath);
	}
// FONT
	else if (_stricmp("font", argv[1]) == 0 || _stricmp("sys", argv[1]) == 0) {
		return SYSXtract(IsUW2, UWPath, OutPath);
	}
// GR
	else if (_stricmp("gr", argv[1]) == 0) {
		return GRXtract(IsUW2, UWPath, OutPath);
	}
// LEV
  // DATA or unspecified
	else if (_stricmp("lev", argv[1]) == 0 || _stricmp("levd", argv[1]) == 0) {
		return LEVXtract(IsUW2, "D", UWPath, OutPath);
	}
  // All
	else if (_stricmp("lev*", argv[1]) == 0) {
		return LEVXtract(IsUW2, "*", UWPath, OutPath);
	}
  // SAVE#
	else if (ExportType.substr(0, 3) == "lev" && ExportType.length() == 4) {
		return LEVXtract(IsUW2, ExportType.substr(3, 1), UWPath, OutPath);
	}
// SAV
  // Break if UW2 (ideally will be removed)
	else if (IsUW2 && (_stricmp("save", argv[1]) == 0 || _stricmp("sav", argv[1]) == 0 || _stricmp("savd", argv[1]) == 0 || _stricmp("sav*", argv[1]) == 0 || (ExportType.substr(0, 3) == "sav" && ExportType.length() == 4))) {
		printf("Game save extract currently only for UW1.\n\nI'll get to it.\n");
		return -1;
	}
  // DATA
	else if (_stricmp("savd", argv[1]) == 0) {
		return SAVXtract("D", UWPath, OutPath);
	}
  // All
	else if (_stricmp("sav*", argv[1]) == 0) {
		return SAVXtract("*", UWPath, OutPath);
	}
  // Unspecified (try SAVE1)
	else if (_stricmp("save", argv[1]) == 0 || _stricmp("sav", argv[1]) == 0) {	// Default to SAVE1 if not specified
		return SAVXtract("1", UWPath, OutPath);
	}
  // SAVE#
	else if (ExportType.substr(0, 3) == "sav" && ExportType.length() == 4) {
		return SAVXtract(ExportType.substr(3, 1), UWPath, OutPath);
	}
// MDL
	else if (_stricmp("mdl", argv[1]) == 0 || _stricmp("model", argv[1]) == 0) {
		return MDLXtract(IsUW2, UWPath, OutPath);
	}
// PAK
	else if (_stricmp("pak", argv[1]) == 0) {
		return PAKXtract(IsUW2, UWPath, OutPath);
	}
// PAL
	else if (_stricmp("gimp", argv[1]) == 0) {   // Not sure if this would be more confusing if included, may just be useful for me, so hidden option
		return PALXtract(IsUW2, UWPath, OutPath, true);
	}
	else if (_stricmp("pal", argv[1]) == 0) {
		return PALXtract(IsUW2, UWPath, OutPath, false);
	}
// SCD
  // Break if UW1
	else if (!IsUW2 && (_stricmp("scd", argv[1]) == 0 || _stricmp("scdd", argv[1]) == 0 || _stricmp("scd*", argv[1]) == 0 || (ExportType.substr(0, 3) == "scd" && ExportType.length() == 4))) {
		printf("SCD.ARK extract for UW2 only.\n");
		return -1;
	}
  // DATA or unspecified
	else if (_stricmp("scd", argv[1]) == 0 || _stricmp("scdd", argv[1]) == 0) {
		return SCDXtract("D", UWPath, OutPath);
	}
  // All
	else if (_stricmp("scd*", argv[1]) == 0) {
		return SCDXtract("*", UWPath, OutPath);
	}
  // SAVE#
	else if (ExportType.substr(0, 3) == "scd" && ExportType.length() == 4) {
		return SCDXtract(ExportType.substr(3, 1), UWPath, OutPath);
	}
// TR
	else if (_stricmp("tr", argv[1]) == 0) {
		return TRXtract(IsUW2, UWPath, OutPath);
	}
// Save test - not listed
	else if (!IsUW2 && _stricmp("raw", argv[1]) == 0) {
		return SAVRAWXtract(UWPath, OutPath);
	}

	else {
		printf("Invalid extract type: %s\n", argv[1]);
		return -1;
	}
}
