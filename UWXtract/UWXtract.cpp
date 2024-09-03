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
extern int MAGXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int MDLXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int PAKXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int PALXtract(bool IsUW2, const std::string UWPath, const std::string OutPath, bool IsGIMP);
extern int SAVXtract(bool IsUW2, std::string ExportTarget, const std::string UWPath, const std::string OutPath);
extern int SCDXtract(std::string ExportTarget, const std::string UWPath, const std::string OutPath);
extern int SYSXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int TRXtract(bool IsUW2, const std::string UWPath, const std::string OutPath);
extern int VOCXtract(const std::string UWPath, const std::string OutPath);

// Test functions
extern int RAWXtractUW1(const std::string UWPath, const std::string OutPath);
extern int RAWXtractUW2(const std::string UWPath, const std::string OutPath);

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
			"  MAG      Magic spell data\n"
			"  MDL      3D model details\n"
			"  PAK      Game strings (text)\n"
			"  PAL      Bitmap color palettes\n"
			"  SCD      SCD archive (UW2 only)\n"
			"  SAV      Save game (SAVE#\\PLAYER.DAT)\n"
			"  TR       TR bitmap images (Floor/Wall textures)\n"
			"  VOC      Cutscene and sound effects\n"
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

// Get LEV/SAV/SCD target
	std::string ExportTarget = "";
	if (ExportType.substr(0, 3) == "lev" || ExportType.substr(0, 3) == "sav"  || ExportType.substr(0, 3) == "scd" ) {
	// Different default for SAV vs LEV/SCD if target not specified
		if (ExportType.length() == 3) {
			if (ExportType == "sav") {
				ExportTarget = "1";
			}
			else {
				ExportTarget = "D";
			}
		}
	// Break if invalid length passed
		else if (ExportType.length() > 4) {
			printf("Invalid extract target: %s\n", ExportType.substr(3, ExportType.length() - 3).c_str());
			return -1;
		}
	// Take 4th character as target, further validation is done in the extract function
		else {
			ExportTarget = ExportType.substr(3, 1);
		}

	// Strip target character from export type
		ExportType = ExportType.substr(0, 3);
	}

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
		MAGXtract(IsUW2, UWPath, OutPath);	// Single text file export, so don't bother creating sub folder
		MDLXtract(IsUW2, UWPath, OutPath);	// Single text file export, so don't bother creating sub folder
		PAKXtract(IsUW2, UWPath, OutPath + "\\PAK");
		PALXtract(IsUW2, UWPath, OutPath + "\\PAL", false);
		SAVXtract(IsUW2, "*", UWPath, OutPath + "\\SAV");
	// SCD is UW2 only -- Don't think it's worth noting it's skipped here, only if specifically called
		if (IsUW2) {
			SCDXtract("*", UWPath, OutPath + "\\SCD");
		}
		TRXtract(IsUW2, UWPath, OutPath + "\\TR");
		VOCXtract(UWPath, OutPath + "\\VOC");
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
	else if (ExportType == "lev") {
		return LEVXtract(IsUW2, ExportTarget, UWPath, OutPath);
	}
// MAG
	else if (_stricmp("mag", argv[1]) == 0) {
		return MAGXtract(IsUW2, UWPath, OutPath);
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
// SAV
	else if (ExportType == "sav") {
		return SAVXtract(IsUW2, ExportTarget, UWPath, OutPath);
	}
// SCD
	else if (ExportType == "scd") {
	// Break if UW1
		if (!IsUW2) {
			printf("SCD.ARK extract for UW2 only.\n");
			return -1;
		}
		else {
			return SCDXtract(ExportTarget, UWPath, OutPath);
		}
	}
// TR
	else if (_stricmp("tr", argv[1]) == 0) {
		return TRXtract(IsUW2, UWPath, OutPath);
	}
// VOC
	else if (_stricmp("voc", argv[1]) == 0) {
		return VOCXtract(UWPath, OutPath);
	}
// Save test - not listed
	else if (!IsUW2 && _stricmp("raw", argv[1]) == 0) {
		return RAWXtractUW1(UWPath, OutPath);
	}
	else if (IsUW2 && _stricmp("raw", argv[1]) == 0) {
		return RAWXtractUW2(UWPath, OutPath);
	}
	else {
		printf("Invalid extract type: %s\n", argv[1]);
		return -1;
	}
}
