/*************
	UWXtract - Ultima Underworld Extractor

	PLAYER.DAT decoder/extractor

	Calls the game save extract functions (UW1 or UW2)

	Note:
		While there is a fair amount of overlap between UW1 & UW2 saves
		between the number of differences and the size of the code,
		it makes more sense to have these split into two files/functions
*************/
#include "UWXtract.h"

extern bool AvailSaveGame(std::string UWPath, int SaveID);		// Util.cpp
extern int ProcessUW1SAV(char* SourcePath, const std::string UWPath, const std::string OutPath); // SAVXtractUW1.cpp
extern int ProcessUW2SAV(char* SourcePath, const std::string UWPath, const std::string OutPath); // SAVXtractUW2.cpp

static int ProcessBGlobal(
	const bool IsUW2,
	char* SourcePath,
	const std::string UWPath,
	const std::string OutPath
) {
/***
	Don't really know how conversations work at all (me no talk gud) so not sure
	if there are common variables where adding a description field would make sense

	May look into how convos work and add that if that's the case.
	Not going to bother if variables are always (or near always) unique.
***/

// Reusable variable for setting target file names
	char TempPath[256];

// Get strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Create export file and set header
	sprintf(TempPath, "%s\\%s_BGlobal.csv", OutPath.c_str(), SourcePath);
	FILE* BGlobalOut = fopen(TempPath, "w");
	fprintf(BGlobalOut, "ConversationID,NPCName,VariableID,Value\n");


// Get number of conversations from BABGLOBS.DAT
	sprintf(TempPath, "%s\\DATA\\BABGLOBS.DAT", UWPath.c_str());
	FILE* BABGLOBS = fopen(TempPath, "rb");

	fseek(BABGLOBS, 0, SEEK_END);
	int RecordCount = ftell(BABGLOBS) / 4;	// BABGLOBS is 4 bytes per conv record - u16 ConvID & u16 VarCount
	fclose(BABGLOBS);

// Read in BGLOBALS.DAT
	sprintf(TempPath, "%s\\%s\\BGLOBALS.DAT", UWPath.c_str(), SourcePath);
	FILE* BGlobalFile = fopen(TempPath, "rb");

// Loop each conversation
	for (int i = 0; i < RecordCount; i++) {
	// Get target NPC ID/Name
		unsigned int ConvID = 0;
		fread(&ConvID, sizeof(short int), 1, BGlobalFile);
		std::string NPCName = CleanDisplayName(gs.get_string(7, ConvID + 16), true, false);

	// Assign missing (or fix) NPC names
		if (!IsUW2) {
			switch (ConvID) {
				case  24:	NPCName = "Prisoner (Murgo)"; break;	// Modifying to include Murgo
				case 115:	NPCName = "Crazy Bob"; break;
				case 262:	NPCName = "Green Goblin (Club)"; break;
				case 263:	NPCName = "Green Goblin (Sword)"; break;
				case 268:	NPCName = "Gray Goblin (Club)"; break;
				case 272:	NPCName = "Gray Goblin (Sword)"; break;
				case 276:	NPCName = "Mountainman (Red)"; break;
				case 277:	NPCName = "Green Lizardman"; break;
				case 278:	NPCName = "Mountainman (White)"; break;
				case 280:	NPCName = "Red Lizardman"; break;
				case 281:	NPCName = "Gray Lizardman"; break;
				case 282:	NPCName = "Outcast"; break;
				case 288:	NPCName = "Troll"; break;
				case 291:	NPCName = "Ghoul"; break;
				case 295:	NPCName = "Mage (Male)"; break;
				case 297:	NPCName = "Dark Ghoul"; break;
				case 314:	NPCName = "Wisp"; break;
			};
		}
		else {
			switch (ConvID) {
				case  18:	NPCName = "Goblin Guard"; break;
				case  19:	NPCName = "Goblin Guard"; break;
				case  35:	NPCName = "Unknown"; break;
				case  59:	NPCName = "Trilkhai"; break;
				case  63:	NPCName = "Prinx"; break;
				case  83:	NPCName = "Flip"; break;
				case 144:	NPCName = "Moglop Goblin"; break;
			};
		}

	// Get number of variables for conv
		unsigned short int VarCount;
		fread(&VarCount, sizeof(short int), 1, BGlobalFile);

	// Loop conv variables
		for (int v = 0; v < VarCount; v++) {
			unsigned short int VarVal;
			fread(&VarVal, sizeof(short int), 1, BGlobalFile);

			fprintf(
				BGlobalOut,
				"%u,"	// ConversationID
				"%s,"	// NPCName
				"%u,"	// VariableID
				"%u\n",	// Value
				ConvID,				// ConversationID
				NPCName.c_str(),	// NPCName
				v,					// VariableID
				VarVal				// Value
			);
		}
	}

// Close files and return
	fclose(BGlobalFile);
	fclose(BGlobalOut);

	return 0;
}

int SAVXtract(
	const bool IsUW2,
	std::string ExportTarget,
	const std::string UWPath,
	const std::string OutPath
) {
// Using this to get isdigit to behave for save check (I'm certainly doing something wrong but this works and tired of messing with it)
	char SaveID[1];
	sprintf(SaveID, "%s", ExportTarget.c_str());

// Export all
	if (ExportTarget == "*") {
	// Create output folder (done here in case invalid parameter passed)
		CreateFolder(OutPath);

		//ProcessUW1SAV("DATA", UWPath, OutPath); // For saves we'll treat the generic one in DATA the same as 0 where it must be specified as there's not much point in exporting it - Actually, we'll just drop it completely

	// Loop through and export any saves that exist -- Note:  Skipping temp SAVE0 when exporting all (not really all I guess :P)
		for (int s = 1; s < 5; s++) {
			if (AvailSaveGame(UWPath, s)) {
				char SavePath[5];
				sprintf(SavePath, "SAVE%u", s);

				if (!IsUW2) {
					ProcessUW1SAV(SavePath, UWPath, OutPath);
				}
				else {
					ProcessUW2SAV(SavePath, UWPath, OutPath);
				}
				ProcessBGlobal(IsUW2, SavePath, UWPath, OutPath);
			}
		}

		printf("Saved games extracted to %s\n", OutPath.c_str());
		return 0;
	}
/***
	Data only
	On further review, the data file is incomplete and missing 115 bytes -- I can tell what some of it is but it's not worth the effort to try to map the data
	to what it goes to/what properties are skipped and likely not really possible as it's already an invalid file and can't be loaded to check anything

	RIP GRONKEY
	We hardly knew ye
***/
	else if (ExportTarget == "d" || ExportTarget == "D") {
	/***
		if (!IsUW2) {
		// Create output folder (done here in case invalid parameter passed)
			CreateFolder(OutPath);
			ProcessUW1SAV("DATA", UWPath, OutPath);
		}
	// UW2 - DATA\PLAYER.DAT is a left over file from UW1 - While I could run that function against it, it's probably better to just fail it completely due to the difference in format
		else {
			printf("DATA\\PLAYER.DAT extract is valid for UW1 only.  Valid values are 1-4.\n");
			return -1;
		}
	***/
		printf("DATA\\PLAYER.DAT is an invalid file and cannot be extracted.  Valid values are 1-4.\n");
		return -1;
	}
// Error if non numeric value passed
	else if (isdigit(SaveID[0]) == 0) {
		printf("Invalid SAVE specified.  Valid values are 1-4.\n");
		return -1;
	}
// Error if out of range
	else if (stoi(ExportTarget) > 4) {
		printf("Invalid SAVE specified.  Valid values are 1-4.\n");
		return -1;
	}
// Error if temp SAVE0 specified and not found -- Point out game/save needs run/loaded
	else if (stoi(ExportTarget) == 0 && !AvailSaveGame(UWPath, stoi(ExportTarget))) {
		printf("No data for SAVE0 found.  Make sure game is currently running with a save loaded.\n");
		return -1;
	}
// Error if specified save does not exist
	else if (!AvailSaveGame(UWPath, stoi(ExportTarget))) {
		printf("No data for SAVE%u found.\n", stoi(ExportTarget));
		return -1;
	}
// Export specified save
	else {
	// Create output folder (done here in case invalid parameter passed)
		CreateFolder(OutPath);

		char SavePath[255];
		sprintf(SavePath, "SAVE%u", stoi(ExportTarget));

		if (!IsUW2) {
			ProcessUW1SAV(SavePath, UWPath, OutPath);
		}
		else {
			ProcessUW2SAV(SavePath, UWPath, OutPath);
		}
		ProcessBGlobal(IsUW2, SavePath, UWPath, OutPath);
	}

	printf("Saved game extracted to %s\n", OutPath.c_str());
	return 0;
}
