/*************
	UWXtract - Ultima Underworld Extractor

	STRINGS.PAK decoder/extractor

	Heavily modified version of the strpak utility by the
	Underworld Adventures Team to extract game strings
*************/
#include <SDL2/SDL_types.h>
#include "GameStrings.hpp"
#include "GameStringsImporter.hpp"
#include <sstream>
#include <iomanip>
#include <filesystem>

extern std::string CleanDisplayName(const std::string InName, bool IsTitleCase, bool RemoveSpace);			// UWXtract.h -- Standard header conflicts with some of the other includes here so just including the function
extern void CreateFolder(const std::string FolderName);														// UWXtract.h -- Standard header conflicts with some of the other includes here so just including the function
extern void StringReplace(std::string& InString, const std::string& OldText, const std::string& NewText);	// Util.cpp

int PAKXtract(
	const bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

// Create export files and set headers
	CreateFolder(OutPath);

	sprintf(TempPath, "%s\\PAK.csv", OutPath.c_str());
	FILE* StringTOC = fopen(TempPath, "w");
	fprintf(StringTOC, "Block,Description,Count,Total\n");

	sprintf(TempPath, "%s\\STRINGS.csv", OutPath.c_str());
	FILE* StringOut = fopen(TempPath, "w");
	fprintf(StringOut, "Block,StringID,StringText\n");

// Read in strings
	GameStrings gs;
	Import::GameStringsImporter importer(gs);
	importer.LoadStringsPakFile((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Get all blocks to export
	const GameStrings& cgs = gs;
	const std::set<Uint16>& stringBlocks = cgs.GetStringBlockSet();

	std::set<Uint16>::iterator iter, stop;
	iter = stringBlocks.begin();
	stop = stringBlocks.end();

// Loop through each string block
	for (; iter != stop; iter++) {
		Uint16 BlockID = *iter;
		std::vector<std::string> stringList = gs.GetStringBlock(BlockID);

	// Hardcode generic string block descriptions
		std::string BlockDesc = "";
		switch (BlockID) {
			case 0x0001:	BlockDesc = "General"; break;
			case 0x0002:	BlockDesc = "CharCreation_Mantra"; break;
			case 0x0003:	BlockDesc = "Text_BookScroll"; break;
			case 0x0004:	BlockDesc = "Item_Name"; break;
			case 0x0005:	BlockDesc = "Item_Description"; break;
			case 0x0006:	BlockDesc = "Spell_Name"; break;
			case 0x0007:	BlockDesc = "Barter_NPCName"; break;
			case 0x0008:	BlockDesc = "Text_SignWall"; break;
			case 0x0009:	BlockDesc = "Text_Trap"; break;
			case 0x000A:	BlockDesc = "Terrain_Description"; break;
			case 0x0018:	BlockDesc = "Debug"; break;
			case 0x0C00:	BlockDesc = "CS000-Intro"; break;
			default:		BlockDesc = BlockDesc; break;
		}
	// Hardcode cutscenes and some NPCs
		if (!IsUW2) {
		// UW1
			switch (BlockID) {
				case 0x0C01:	BlockDesc = "CS001-Outro"; break;
				case 0x0C02:	BlockDesc = "CS002-Tyball"; break;
				case 0x0C03:	BlockDesc = "CS003-Arial"; break;
				case 0x0C18:	BlockDesc = "CS030-Garamon"; break;
				case 0x0C19:	BlockDesc = "CS031-Garamon"; break;
				case 0x0C1A:	BlockDesc = "CS032-Garamon"; break;
				case 0x0C1B:	BlockDesc = "CS033-Garamon"; break;
				case 0x0C1C:	BlockDesc = "CS034-Garamon"; break;
				case 0x0C1D:	BlockDesc = "CS035-Garamon"; break;
				case 0x0C1E:	BlockDesc = "CS036-Garamon"; break;
				case 0x0C1F:	BlockDesc = "CS037-Garamon"; break;
				case 0x0C20:	BlockDesc = "CS040-Garamon"; break;
				case 0x0C21:	BlockDesc = "CS041-Garamon"; break;
				case 0x0E18:	BlockDesc = "NPC-Prisoner (Murgo)"; break;	// Hardcoded to include NPC's actual name
				case 0x0E73:	BlockDesc = "NPC-Crazy Bob"; break;	// Prices are crazy
			//	These map to block 4 string id = BlockID - 0x0F00 + 64
			//	But, in order to call out the specific type for some of them (i.e. Green Goblin (Club), it's all hardcoded here
				case 0x0F06:	BlockDesc = "Generic-Green Goblin (Club)"; break;
				case 0x0F07:	BlockDesc = "Generic-Green Goblin (Sword)"; break;
				case 0x0F0C:	BlockDesc = "Generic-Gray Goblin (Club)"; break;
				case 0x0F10:	BlockDesc = "Generic-Gray Goblin (Sword)"; break;
				case 0x0F14:	BlockDesc = "Generic-Mountainman (Red)"; break;
				case 0x0F15:	BlockDesc = "Generic-Green Lizardman"; break;
				case 0x0F16:	BlockDesc = "Generic-Mountainman (White)"; break;
				case 0x0F18:	BlockDesc = "Generic-Red Lizardman"; break;
				case 0x0F19:	BlockDesc = "Generic-Gray Lizardman"; break;
				case 0x0F1A:	BlockDesc = "Generic-Outcast"; break;
				case 0x0F20:	BlockDesc = "Generic-Troll"; break;
				case 0x0F23:	BlockDesc = "Generic-Ghoul"; break;
				case 0x0F27:	BlockDesc = "Generic-Mage (Male)"; break;
				case 0x0F29:	BlockDesc = "Generic-Dark Ghoul"; break;
				case 0x0F3A:	BlockDesc = "Generic-Wisp"; break;
				default:		BlockDesc = BlockDesc; break;
			}
		}
		else {
		// UW2
			switch (BlockID) {
				case 0x0C02:	BlockDesc = "CS002-Outro"; break;
				case 0x0C04:	BlockDesc = "CS004-Guardian"; break;
				case 0x0C05:	BlockDesc = "CS005-Guardian"; break;
				case 0x0C06:	BlockDesc = "CS006-Guardian"; break;
				case 0x0C07:	BlockDesc = "CS007-Guardian"; break;
				case 0x0C18:	BlockDesc = "CS030-Guardian"; break;
				case 0x0C19:	BlockDesc = "CS031-Guardian"; break;
				case 0x0C1A:	BlockDesc = "CS032-Guardian"; break;
				case 0x0C1B:	BlockDesc = "CS033-Guardian"; break;
				case 0x0C1C:	BlockDesc = "CS034-Guardian"; break;
				case 0x0C1D:	BlockDesc = "CS035-Guardian"; break;
				case 0x0C1E:	BlockDesc = "CS036-Guardian"; break;
				case 0x0C20:	BlockDesc = "CS040-Guardian"; break;
				case 0x0C28:	BlockDesc = "CS004-Guardian_Dup"; break;
				case 0x0C29:	BlockDesc = "CS005-Guardian_Dup"; break;
				case 0x0C2A:	BlockDesc = "CS006-Guardian_Dup"; break;
				case 0x0C2B:	BlockDesc = "CS007-Guardian_Dup"; break;
				case 0x0E00:	BlockDesc = "NPC-Dialogician"; break;	// Partial duplicate and pretty sure invalid/unused but that _is_ what is here
				case 0x0E12:	BlockDesc = "NPC-Goblin Guard"; break;
				case 0x0E13:	BlockDesc = "NPC-Goblin Guard"; break;
				case 0x0E21:	BlockDesc = "NPC-Silanus"; break;
				case 0x0E23:	BlockDesc = "NPC-Unknown"; break;
				case 0x0E3A:	BlockDesc = "NPC-Brain Creature"; break; // This is a partial dupe of 3B (missing keep falling strings) but tied to the 2 brain creatures in Killorn Keep - don't recall that you can actually talk to them?
				case 0x0E3B:	BlockDesc = "NPC-Trilkhai"; break;
				case 0x0E3F:	BlockDesc = "NPC-A.I. Crunchowicz"; break;
				case 0x0E40:	BlockDesc = "NPC-Prinx"; break;
				case 0x0E53:	BlockDesc = "NPC-Flip"; break;
				case 0x0E90:	BlockDesc = "NPC-MoglopGoblin"; break;
				default:		BlockDesc = BlockDesc; break;
			}
		}

	// Get NPC names (unless defined above)
		if (BlockDesc == "" && BlockID > 0x0DFF && BlockID < 0x0F00) {
			BlockDesc = "NPC-" + CleanDisplayName(gs.GetString(7, BlockID - 0x0E00 + 16), true, false);
		}

	// Get count of non-blank strings
		int HasValue = 0;
		for (int v = 0; v < stringList.size(); v++) {
			if (stringList[v] != "") {
				HasValue += 1;
			}
		}

	// Export block summary
		fprintf(
			StringTOC,
			"%04X,"	// BlockID
			"%s,"	// Description
			"%u,"	// Count -- Number of non-blank strings
			"%u\n",	// Total
			BlockID,			// BlockID
			BlockDesc.c_str(),	// Description
			HasValue,			// Count
			stringList.size()	// Total
		);

	// Export block strings
		size_t StringCount = stringList.size();
		for (size_t StringID = 0; StringID < StringCount; StringID++) {
			std::string StringText{stringList[StringID]};

		// Replace embedded newlines with code
			StringReplace(StringText, "\n", "\\n");

		// Text qualify strings containing commas
			if (StringText.find(',') != std::string::npos) {
				StringReplace(StringText, "\"", "\"\"");
				StringText = "\"" + StringText + "\"";
			}

		// Fix leading " in Data Integrator line (3 instances)
			if (IsUW2 && BlockID == 0x0E9B) {
				if (StringText == "\"And how may I engage Skup-interlok?") {
					StringText = "\"\"\"\"And how may I engage Skup-interlok?";
				}
			}

		// Export strings
			fprintf(
				StringOut,
				"%04X,"	// BlockID
				"%u,"	// StringID
				"%s\n",	// StringText
				BlockID,			// BlockID
				StringID,			// StringID
				StringText.c_str()	// StringText
			);
		}
	}
	fclose(StringTOC);
	fclose(StringOut);

	printf("PAK strings extracted to %s\n", OutPath.c_str());
	return 0;
}
