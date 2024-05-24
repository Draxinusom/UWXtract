/*************
	UWXtract - Ultima Underworld Extractor

	SCD.ARK decoder/extractor

	Extracts the scheduled events from SCD.ARK

	Todo:
		Handful of unknowns to identify
		Identify difference between standard/alternate event types (i.e. 2/Move_NPC & 254/Move_NPC_Alt
		Determine usage of second u16 in block header and record 1 usage after events processed

	Likely need to check this at various points in the game as a few of these _appear_
	to be broken or bugged which may well be the case (game has a few issue :P)
	but would be better	to see when executed normally versus rigging things to fire
	after the first time you talk to LB
*************/
#include "UWXtract.h"
#include <set>
#include "VariableUW2.hpp"

extern bool AvailSaveGame(std::string UWPath, int SaveID);		// Util.cpp
extern std::string ByteToBitArray(const unsigned char ByteIn);	// Util.cpp
extern std::string UW2LevelName(const int LevelID);				// Util.cpp

// Not sure if there are "official" names for the event types
// Most of the names are taken from Underworld Exporter with some renames/flushing out
static std::map<unsigned int, std::string> SCDEventTypeName = {
	{0, "None"},	// I kinda suspect this may be a reset event for that block's XClock
	{1, "Set_Goal_Target"},
	{2, "Move_NPC"},
	{3, "Kill_NPC"},
	{4, "Set_Quest_Flag"},
	{5, "Fire_Triggers"},
	{6, "Unused"},
	{7, "Set_NPC_Props"},
	{8, "Set_Race_Attitude"},
	{9, "Set_Variable"},
	{10, "Check_Variable"},
	{11, "Remove_NPC"},
// I assume there is a difference between the two...
	{245, "Remove_NPC_Alt"},
	{246, "Check_Variable_Alt"},
	{247, "Set_Variable_Alt"},
	{248, "Set_Race_Attitude_Alt"},
	{249, "Set_NPC_Props_Alt"},
	{250, "Unused_Alt"},
	{251, "Fire_Triggers_Alt"},
	{252, "Set_Quest_Flag_Alt"},
	{253, "Kill_NPC_Alt"},
	{254, "Move_NPC_Alt"},
	{255, "Set_Goal_Target_Alt"}
};

// Helper lookup table to avoid needing to check for both variants
static std::map<unsigned int, unsigned int> SCDEventTypeMap = {
	{0, 0},
	{1, 1},
	{2, 2},
	{3, 3},
	{4, 4},
	{5, 5},
	{6, 6},
	{7, 7},
	{8, 8},
	{9, 9},
	{10, 10},
	{11, 11},
	{245, 11},
	{246, 10},
	{247, 9},
	{248, 8},
	{249, 7},
	{250, 6},
	{251, 5},
	{252, 4},
	{253, 3},
	{254, 2},
	{255, 1}
};

static std::map<unsigned int, std::string> SCDTargetType = {
	{0, "NPC"},
	{1, "Race"},
	{2, "Object"},	// Maps to ObjectID in level
	{3, "All"}		// Bit of a guess but is only used in reference to KK after destroyed with TargetID 00
};

static std::map<unsigned int, std::string> SCDGoalType = {
	{0, "0: StandStill"},
	{1, "1: Goto"},
	{2, "2: Wander"},
	{3, "3: Follow"},
	{4, "4: Wander"},
	{5, "5: Attack"},
	{6, "6: Flee"},
	{7, "7: StandStill"},
	{8, "8: Wander"},
	{9, "9: Attack"},
	{10, "10: WantToTalk"},
	{11, "11: StandStill"},
	{12, "12: StandStill"},
	{13, "13: Unknown"},
	{14, "14: Unknown"},
	{15, "15: Petrified"}
};

// Guessing this aligns with the set variable trap
static std::map<unsigned int, std::string> SCDOperatorSet = {
	{0, "ADD"},
	{1, "SUB"},
	{2, "SET"},
	{3, "AND"},
	{4, "OR"},
	{5, "XOR"},
	{6, "LSH"},
	{7, "CLR"}	// Near as I can tell, all this does is wipe the value
};

// Not really sure this is a thing as all instances are 1 - Might try messing with it to find out
static std::map<unsigned int, std::string> SCDOperatorCheck = {
	{0, "UNK"},
	{1, "EQU"},
	{2, "UNK"},
	{3, "UNK"},
	{4, "UNK"},
	{5, "UNK"},
	{6, "UNK"},
	{7, "UNK"}
};

// Which variable array is referenced for check/set types (and one NPC prop type)
static std::map<unsigned int, std::string> SCDVariableType = {
	{0, "Variable"},
	{1, "Quest"},
	{2, "BitField"},	// This is not an actual specified type value but set if variable type and target variable is over 127
	{3, "XClock"}		// This is not an actual specified type value but set if quest type and target variable is over 143
};

int ProcessSCD(
	char* SourcePath,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[255];

// Get strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Create output files and header
	sprintf(TempPath, "%s\\%s_Header.csv", OutPath.c_str(), SourcePath);
	FILE* SCDHeaderOut = fopen(TempPath, "w");
	fprintf(SCDHeaderOut, "Block,Offset,DataSize,TotalSize,CanCompress,IsCompressed,IsPadded,ActiveRecords,UnusedRecords\n");

	sprintf(TempPath, "%s\\%s_Data.csv", OutPath.c_str(), SourcePath);
	FILE* SCDDataOut = fopen(TempPath, "w");
	fprintf(SCDDataOut, "Block,EventID,Level,XClockValue,EventType,IsRemoved,TargetType,TargetName,Goal,GTarg,Attitude,TargetLevel,TargetX,TargetY,HomeX,HomeY,Variable,VariableType,Operator,Value,Unknown1,Unknown2,Unknown3,IsActive\n");

// Read in SCD.ARK
	sprintf(TempPath, "%s\\%s\\SCD.ARK", UWPath.c_str(), SourcePath);
	FILE* SCDARK = fopen(TempPath, "rb");

// First two bytes is the number of blocks in the file
	unsigned short ActiveBlock;
	fread(&ActiveBlock, sizeof(unsigned short), 1, SCDARK);

// Skip over blank block (maybe a self reference to the header as a block?)
	fseek(SCDARK, 6, SEEK_SET);

/***
	Storing all header data in 1 array
	0:	Offset to Block
	1:	Flags			-- Don't believe this file ever compresses so always 00, including just in case
	2:	DataSize		-- Record count it this minus 4 (ActiveRecord int at the beginning) divided by 16
	3:	AvailableSize	-- Since it doesn't compress this should always equal DataSize
	4:	ActiveRecords	-- Technically not part of header but makes sense to report here

	ActiveRecords:
	I'm assuming that's what it is as it matches up row/record count wise and similar logic is applied in other ARK files
	All blocks (including empty ones) start with 20 free records -- After first event processed in block, record 1 is populated with number removed and current number of active events (same as in block header)
	Similar to objects in LEV.ARK, records fill from the bottom up
***/
	unsigned int HeaderData[16][5];

// Get standard header data
	for (int v = 0; v < 4; v++) {
		for (int b = 0; b < ActiveBlock; b++) {
			unsigned int HeaderBuffer;
			fread(&HeaderBuffer, sizeof(unsigned int), 1, SCDARK);

			HeaderData[b][v] = HeaderBuffer;
		}
	}
// Get Active Events
	for (int b = 0; b < ActiveBlock; b++) {
		fseek(SCDARK, HeaderData[b][0], SEEK_SET);
		unsigned int HeaderBuffer;
		fread(&HeaderBuffer, sizeof(short int), 1, SCDARK);	// Note, this is a u16 value, not int
		HeaderData[b][4] = HeaderBuffer;
	}

// Dump header details to CSV
	for (int b = 0; b < ActiveBlock; b++) {
		fprintf(
			SCDHeaderOut,
			"%u,"	// Block
			"%04x,"	// Offset
			"%u,"	// DataSize
			"%u,"	// TotalSize
			"%u,"	// CanCompress
			"%u,"	// IsCompressed
			"%u,"	// IsPadded
			"%u,"	// ActiveRecords
			"%u\n",	// UnusedRecords
			b,								// Block
			HeaderData[b][0],				// Offset
			HeaderData[b][2],				// DataSize
			HeaderData[b][3],				// TotalSize
			HeaderData[b][1] & 0x01,		// CanCompress
			(HeaderData[b][1] & 0x02) >> 1,	// IsCompressed
			(HeaderData[b][1] & 0x04) >> 2,	// IsPadded
			HeaderData[b][4],				// ActiveRecords
			((HeaderData[b][2] - 4) / 16) - HeaderData[b][4]	// UnusedRecords
		);
	}
// Done with the header output
	fclose(SCDHeaderOut);

// Loop and dump block data
	for (int b = 0; b < ActiveBlock; b++) {
		fseek(SCDARK, (HeaderData[b][0] + 4), SEEK_SET);	// Note - Skip 4 byte active record block header

		int TotalRecords = ((HeaderData[b][2] - 4) / 16);

	// Loop each record
		for (int r = 0; r < TotalRecords; r++) {
		// Read in record
			std::vector<unsigned char> SCDData(16);
			for (int i = 0; i < 16; i++) {
				SCDData[i] = fgetc(SCDARK);
			}

		// Since all events are duplicated to avoid looking up both types while determining what values to grab, make variable with mapped type
			unsigned int EventTypeID = SCDEventTypeMap[SCDData[4]];

		// What values are populated depends on the event type so just declare/initialize a variable to blank and update where applicable
			std::string LevelName = "";
			std::string TargetType = "";
		//	std::string TargetID;
			std::string TargetName = "";
			std::string Goal = "";
			std::string GTarg = "";
			std::string Attitude = "";
			//std::string TargetLevelID = "";
			std::string TargetLevelName = "";
			std::string TargetX = "";
			std::string TargetY = "";
			std::string HomeX = "";
			std::string HomeY = "";
			std::string Variable = "";
			std::string VariableType = "";
			std::string Operator = "";
			std::string Value = "";
			std::string Unknown1 = "";
			std::string Unknown2 = "";
			std::string Unknown3 = "";

		// First record's usage is unknown but will map to invalid values so to avoid confusing things until its purpose is determined, we'll mostly skip processing it
			if (r > 0) {
			// LevelName -- All types
				if (r >= TotalRecords - HeaderData[b][4] || SCDData[2] > 0) {	// Leave LevelName blank if value is 0 and is not an active record (will show Unknown otherwise)
					LevelName = UW2LevelName(SCDData[2]);
				}

			// TargetType/Name
			  // Set_Goal_Target/Move_NPC/Kill_NPC/Set_Race_Attitude/Remove_NPC
				if (std::set<unsigned int>{1,2,3,8,11}.count(EventTypeID)
				// Set_NPC_Props & Goal IN(Goto/Wander/Flee/StandStill)
					|| (EventTypeID == 7 && std::set<unsigned int>{1,2,6,7}.count(SCDData[5]))
				) {
					unsigned int TargetIntID;
				// Set_NPC_Props & Goal = Wander
					if (EventTypeID == 7 && SCDData[5] == 2) {
						TargetType = SCDTargetType[SCDData[8]];
						TargetIntID = SCDData[9];
					}
				// Set_NPC_Props & Goal IN(Goto/Flee/StandStill)
					else if (EventTypeID == 7) {
						TargetType = SCDTargetType[SCDData[6]];
						TargetIntID = SCDData[7];
					}
				// Move_NPC
					else if (EventTypeID == 2) {
						TargetType = SCDTargetType[SCDData[7]];
						TargetIntID = SCDData[8];
					}
				// Set_Goal_Target/Kill_NPC/Set_Race_Attitude/Remove_NPC
					else {
						TargetType = SCDTargetType[SCDData[5]];
						TargetIntID = SCDData[6];
					}

				// Set output TargetID
				//	TargetID = std::to_string(TargetIntID);

				// Lookup target name -- varies by type
					if (TargetType == "NPC") {
						TargetName = std::to_string(TargetIntID) + ": " + CleanDisplayName(gs.get_string(7, TargetIntID + 16), true, false);
					}
					else if (TargetType == "Race") {
						TargetName = std::to_string(TargetIntID) + ": " + CleanDisplayName(gs.get_string(1, TargetIntID + 370), true, false);
					}
				/***
					Object maps to the ObjectID in that level in LEV.ARK
					Reading that in for this would be quite a bit of effort for just the two that exist
					so they are just hardcoded if it matches the expected level/ObjectIDs

					Not 100% sure whether new events can get added during play and I'm very unlikely to
					add logic to do lookups on anything else so, if that does happen, they'll be blank
				***/
					else if (TargetType == "Object" && SCDData[2] == 1 && (TargetIntID == 231 || TargetIntID == 248)) {
						TargetName = std::to_string(TargetIntID) + ": Guard";
					}
				// Not going to set anything for All type
				}

			// Goal/GTarg
			  // Set_Goal_Target
				if (EventTypeID == 1) {
					Goal = SCDGoalType[SCDData[7]];
					GTarg = std::to_string(SCDData[8]);
				}
			  // Move_NPC
				else if (EventTypeID == 2) {
					Goal = SCDGoalType[SCDData[11]];
				}
			  // Set_NPC_Props
				else if (EventTypeID == 7) {
					Goal = SCDGoalType[SCDData[5]];
				}

			// Attitude -- I suspect this may be one of the unknown values for a couple types
			  // Set_Race_Attitude
				if (EventTypeID == 8) {
					Attitude = std::to_string(SCDData[7]) + ": " + CleanDisplayName(gs.get_string(5, SCDData[7] + 96), true, false);
				}

			// TargetLevelID/Name
			  // Move_NPC
				if (EventTypeID == 2) {
					//TargetLevelID = std::to_string(SCDData[9]);
					TargetLevelName = std::to_string(SCDData[9]) + ": " + UW2LevelName(SCDData[9]);
				}

			// Target/HomeX/Y
			  // Move_NPC/Fire_Triggers
				if (EventTypeID == 2 || EventTypeID == 5) {
					TargetX = std::to_string(SCDData[5]);
					TargetY = std::to_string(SCDData[6]);
				}
			  // Set_NPC_Props & Goal = Goto
				else if (EventTypeID == 7 && SCDData[5] == 1) {
					TargetX = std::to_string(SCDData[8]);
					TargetY = std::to_string(SCDData[9]);
				}
			  // Set_NPC_Props & Goal = Wander
				else if (EventTypeID == 7 && SCDData[5] == 2) {
					TargetX = std::to_string(SCDData[10]);
					TargetY = std::to_string(SCDData[11]);
					HomeX = std::to_string(SCDData[12]);
					HomeY = std::to_string(SCDData[13]);
				}
			  // Set_NPC_Props & Goal = StandStill
				else if (EventTypeID == 7 && SCDData[5] == 7) {
					TargetX = std::to_string(SCDData[8]);
					TargetY = std::to_string(SCDData[9]);
					HomeX = std::to_string(SCDData[10]);
					HomeY = std::to_string(SCDData[11]);
				}

			/***
				Variables

				Variable handling is a little odd, both here and how it's stored in PLAYER.DAT (at least for Quest flags)

				Quest:
					Since this one is the odd one, a little explanation on how it works.
					In PLAYER.DAT the quest flags are stored as groups of 4 quests across 4 bytes.
					However, for reasons that are unclear, if not baffling, the data is stored in the
					first 4 bits of the first byte for that group as a single bit flag

					Example:
						Quests 0-3 are stored in bytes 0067-006A
						If you are checking/setting quest 0 (Garg is free), bit 0 of byte 0067 is checked/set
						That makes sense, if not a bit wasteful for a single bit flag

						Now for the baffling piece
						If you are checking/setting quest 1 (Bishop is free), bit 1 of byte 0067 is checked/set _not_ bit 0 of byte 0068
						Byte 0068-006A are not touched at all

					So, if you set quest 3 to true and (assuming 0-2 are false) what will happen is bit 3 is set on 0067, giving you a value of 8
					which makes it a bit confusing when trying to check what exactly was done by a scheduled event in a save file

					This continues through the quest flags (i.e. quests 4-7 are bytes 006B-006E), though there may be (probably) some exceptions.

				  Set_Quest_Flag (EventTypeID 4)
					This is always the type set (no VariableType value) and it sets the value to whatever value is passed, unlike Set_Variable
					If passed a quest variable ID that's out of range, it'll update a variable but that looks to be a bug/lack of error checking

					Of note, due to how the quest flags are expected to work, it _will_ alter a different quest than specified if a value other than 1 or 0 is passed.
					Example:
						If an event were to specify updating quest 2 with a value of 2, which in binary is 00000010, it would take that value and bit shift it 2,
						making the value 00001000, with the expectation it would align with the bit for quest 2.  However, what is actually set is
						the bit flag for quest 3.

					No instances of that occuring, usage in the file is limited to setting the value to 0/1, just an explanation of what it is actually doing.


				  Check/Set_Variable (EventTypeID 9/10)
					For these event types, the VariableType value must be 1 _AND_ the target Variable must be less than 144.
					If VariableType = 1 & the target Variable is 144 or greater, it is instead checking/setting an XClock value (described below)

					For this variable type, it appears to ignore the Value and just checks the flag is set or sets the flag for that variable

				Variable:
					For these event types, the VariableType value must be 0 _AND_ the target Variable must be less than 128.
					If VariableType = 0 & the target Variable is 128 or greater, it is instead checking/setting a BitField value (described below)

				  Check_Variable (EventTypeID 10)
					For this type, it appears to check the value of the variable EQ the Value.  There is another value I'm treating as an Operator
					like the Set type that is set to 1 for all instances of Check_Variable and changing it did not seem to alter it's behavior so
					this likely wrong or there just isn't any other type it will do here.

				  Set_Variable (EventTypeID 9)
					This updates the value of the variable based on the Value & Operator values.  Operator appears to match how set_variable traps work.

				BitField:
					For these event types, the VariableType value must be 0 _AND_ the target Variable must be greater than 127.  If so, subtract 128
					from the Variable to get the BitField ID.

					Otherwise, behaves identical to the Variable type.

				XClock:
					For these event types, the VariableType value must be 1 _AND_ the target Variable must be greater than 143.
					If so, subtract 144 from the Variable to get the XClock ID.

					Otherwise, behaves identical to the Variable type.
			***/
			// Set_NPC_Props & Goal = Goto (it's kinda odd but tested/confirmed) -- Behaves like Check_Variable
				if (EventTypeID == 7 && SCDData[5] == 1) {
					unsigned int VariableID = SCDData[10];
					unsigned int VariableTypeID = SCDData[11];
					std::string VariableName = "";

				// BitField isn't directly specified and is just handled as a run on of Variable type so alter the reported variable/type to match
					if (VariableID > 127 && VariableTypeID == 0) {
						VariableID -= 128;
						VariableTypeID = 2;
						VariableName = UW2BitField[VariableID];
					}
				// XClock isn't directly specified and is just handled as a run on of Quest type so alter the reported variable/type to match
					if (VariableID > 143 && VariableTypeID == 1) {
						VariableID -= 144;
						VariableTypeID = 3;
						// Not going to put XClock value names here
					}

				// VariableName -- Variable
					if (VariableTypeID == 0) {
						VariableName = UW2Variable[VariableID];
					}
				// Quest
					else if (VariableTypeID == 1) {
						VariableName = UW2Quest[VariableID];
					}

					Variable = std::to_string(VariableID) + (VariableName == "" ? "" : ": " + VariableName);
					VariableType = SCDVariableType[VariableTypeID];
					Operator = SCDOperatorSet[SCDData[12]];
					Value = std::to_string(SCDData[13]);
				}
			// Set_Variable
				else if (EventTypeID == 9) {
					unsigned int VariableID = SCDData[5];
					unsigned int VariableTypeID = SCDData[6];
					std::string VariableName = "";

				// BitField isn't directly specified and is just handled as a run on of Variable type so alter the reported variable/type to match
					if (VariableID > 127 && VariableTypeID == 0) {
						VariableID -= 128;
						VariableTypeID = 2;
						VariableName = UW2BitField[VariableID];
					}
				// XClock isn't directly specified and is just handled as a run on of Quest type so alter the reported variable/type to match
					if (VariableID > 143 && VariableTypeID == 1) {
						VariableID -= 144;
						VariableTypeID = 3;
						// Not going to put XClock value names here
					}

				// VariableName -- Variable
					if (VariableTypeID == 0) {
						VariableName = UW2Variable[VariableID];
					}
				// Quest
					else if (VariableTypeID == 1) {
						VariableName = UW2Quest[VariableID];
					}

					Variable = std::to_string(VariableID) + (VariableName == "" ? "" : ": " + VariableName);
					VariableType = SCDVariableType[VariableTypeID];
					Operator = SCDOperatorSet[SCDData[7]];
					Value = std::to_string(SCDData[8]);
				}
			// Set_Quest_Flag
				else if (EventTypeID == 4) {
					std::string VariableName = "";
					VariableName = UW2Quest[SCDData[5]];

					Variable = std::to_string(SCDData[5]) + (VariableName == "" ? "" : ": " + VariableName);
					VariableType = "Quest";
					Operator = "SET";
					Value = std::to_string(SCDData[6]);
				}

			// Check_Variable
				if (EventTypeID == 10) {
					unsigned int VariableID = SCDData[5];
					unsigned int VariableTypeID = SCDData[6];
					std::string VariableName = "";

				// BitField isn't directly specified and is just handled as a run on of Variable type so alter the reported variable/type to match
					if (VariableID > 127 && VariableTypeID == 0) {
						VariableID -= 128;
						VariableTypeID = 2;
						VariableName = UW2BitField[VariableID];
					}

				// XClock isn't directly specified and is just handled as a run on of Quest type so alter the reported variable/type to match
					if (VariableID > 143 && VariableTypeID == 1) {
						VariableID -= 144;
						VariableTypeID = 3;
						// Not going to put XClock value names here
					}

				// VariableName -- Variable
					if (VariableTypeID == 0) {
						VariableName = UW2Variable[VariableID];
					}
				// Quest
					else if (VariableTypeID == 1) {
						VariableName = UW2Quest[VariableID];
					}

					Variable = std::to_string(VariableID) + (VariableName == "" ? "" : ": " + VariableName);
					VariableType = SCDVariableType[VariableTypeID];
					Operator = SCDOperatorCheck[SCDData[7]];
					Value = std::to_string(SCDData[10]);
				}
			}
		/***
			Unknown
			There's a few events that have non-zero values whose purpose is unknown (instead of guessed like most of the rest :P)
			Rather than tacking on 16 additional columns to return those, just going to stuff them into an "Unknown" column
			numbered in order of first unknown value found for that event type

			It's possible there are additional unknown values where a value is technically used by/passed to whatever function is
			being executed but all instances of its usage has a value of 0 so it appears to be unused for that type
		***/
		  // Row 0 -- Only showing if there are updates
			if(r == 0 && (SCDData[4] != 0x00 || SCDData[6] != 0x00)) {
				Unknown1 = std::to_string(SCDData[4]);
				Unknown2 = std::to_string(SCDData[6]);
			}
		  // None -- Don't return on free records
			else if (EventTypeID == 0 && r >= TotalRecords - HeaderData[b][4]) {
				Unknown1 = std::to_string(SCDData[1]);
			}
		  // Move_NPC
			else if (EventTypeID == 2) {
				Unknown1 = std::to_string(SCDData[10]);
			}
		  // Kill_NPC
			else if (EventTypeID == 3) {
				Unknown1 = std::to_string(SCDData[7]);
				Unknown2 = std::to_string(SCDData[8]);
			}
		  // Set_NPC_Props & Goal = Follow
			else if (EventTypeID == 7 && SCDData[5] == 3) {
				Unknown1 = std::to_string(SCDData[6]);
				Unknown2 = std::to_string(SCDData[7]);
				Unknown3 = std::to_string(SCDData[8]);
			}
		  // Set_NPC_Props & Goal = Flee
			else if (EventTypeID == 7 && SCDData[5] == 6) {
				Unknown1 = std::to_string(SCDData[8]);
			}

		// Export to CSV
			fprintf(
				SCDDataOut,
				"%u,"	// Block
				"%u,"	// EventID
				"%u%s,"	// Level
				//"%s,"	// LevelName
				"%u,"	// XClockValue
				"%u: %s,"	// EventType
				//"%s,"	// EventTypeName
				"%u,"	// IsRemoved -- Indicates the event should be removed once executed (i.e. run once) -- Calling it IsRemoved as the record is actually removed from SCD.ARK (game save copy) when executed
				"%s,"	// TargetType
			//	"%s,"	// TargetID
				"%s,"	// TargetName
				"%s,"	// Goal
				"%s,"	// GTarg
				"%s,"	// Attitude
				//"%s,"	// TargetLevelID
				"%s,"	// TargetLevelName
				"%s,"	// TargetX
				"%s,"	// TargetY
				"%s,"	// HomeX
				"%s,"	// HomeY
				"%s,"	// Variable
				"%s,"	// VariableType
				"%s,"	// Operator
				"%s,"	// Value
				"%s,"	// Unknown1
				"%s,"	// Unknown2
				"%s,"	// Unknown3
				"%u\n",	// IsActive
				b,										// Block
				r,										// EventID
				SCDData[2], SCDData[2] == 0 ? "" : (": " + LevelName).c_str(),	// LevelID
				//LevelName.c_str(),						// LevelName
				SCDData[0],								// XClockValue
				r == 0 ? 0 : SCDData[4], r == 0 ? "None" : SCDEventTypeName[SCDData[4]].c_str(),				// EventType -- Note:  Hiding row 1 (or 0 whatever) value for now
				SCDData[3],								// IsRemoved
				TargetType.c_str(),
			//	TargetID.c_str(),
				TargetName.c_str(),
				Goal.c_str(),
				GTarg.c_str(),
				Attitude.c_str(),
				//TargetLevelID.c_str(),
				TargetLevelName.c_str(),
				TargetX.c_str(),
				TargetY.c_str(),
				HomeX.c_str(),
				HomeY.c_str(),
				Variable.c_str(),
				VariableType.c_str(),
				Operator.c_str(),
				Value.c_str(),
				Unknown1.c_str(),
				Unknown2.c_str(),
				Unknown3.c_str(),
				r >= TotalRecords - HeaderData[b][4] ? 1 : 0	// IsActive
			);
		}
	}
// Finally close out
	fclose(SCDDataOut);
	fclose(SCDARK);

	return 0;
}

int SCDXtract(
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

		ProcessSCD("DATA", UWPath, OutPath);

	// Loop through and export any saves that exist -- Note:  Skipping temp SAVE0 when exporting all (not really all I guess :P)
		for (int s = 1; s < 5; s++) {
			if (AvailSaveGame(UWPath, s)) {
				char SavePath[5];
				sprintf(SavePath, "SAVE%u", s);

				ProcessSCD(SavePath, UWPath, OutPath);
			}
		}
	}
// Data only
	else if (ExportTarget == "d" || ExportTarget == "D") {
	// Create output folder (done here in case invalid parameter passed)
		CreateFolder(OutPath);
		ProcessSCD("DATA", UWPath, OutPath);
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

		ProcessSCD(SavePath, UWPath, OutPath);
	}

	printf("SCD.ARK extracted to %s\n", OutPath.c_str());
	return 0;
}
