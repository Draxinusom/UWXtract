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

		//ProcessUW1SAV("DATA", UWPath, OutPath); // For saves we'll treat the generic one in DATA the same as 0 where it must be specified as there's not much point in exporting it

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
	}

	printf("Saved game extracted to %s\n", OutPath.c_str());
	return 0;
}
