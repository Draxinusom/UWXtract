/*************
	UWXtract - Ultima Underworld Extractor

	UW2 PLAYER.DAT decoder/extractor

	Extracts details from game saves

	Note:
		While there is a fair amount of overlap between UW1 & UW2 saves
		between the number of differences and the size of the code,
		it makes more sense to have these split into two files/functions

	Todo:
		Quest flags are incorrect and include 3 which are not
		Add Quest/Variable/BitField/XClock descriptions
		Might add some sort of item path like:
			ItemName (<ObjectID>)\ItemName (<ObjectID>)\<Position>
			Pack (1)\Sack (3)\2
*************/
#include "UWXtract.h"
#include "VariableUW2.hpp"
#include <vector>

extern std::string ByteToBitArray(const unsigned char ByteIn);	// Util.cpp
extern std::string UW2LevelName(const int LevelID);				// Util.cpp

static std::map<unsigned int, std::string> UW2MotionState = {
	{0, "Swimming"},
	{1, "Entering Lava"},
	{2, "On Ice/Snow"},
	{3, "On Ground"},
	{4, "Entering Water"},
	{5, "Unknown"},
	{6, "Unknown"},
	{7, "Jumping"}
};

static std::map<unsigned int, std::string> UW2TileState = {
	{0, "In Water"},
	{1, "In Lava"},
	{2, "On Ice/Snow"},
	{3, "Head Bob"},
	{4, "Unknown"},
	{5, "Screen Shake"},
	{6, "Screen Tilt Left/Right"},
	{7, "Screen Tilt Right/Left"}
};

int ProcessUW2SAV(
	char* SourcePath,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

// Get item properties (QualityType/CanBeOwned) - needed for some item related things
	sprintf(TempPath, "%s\\DATA\\COMOBJ.DAT", UWPath.c_str());
	FILE* COMOBJ = fopen(TempPath, "rb");
	fseek(COMOBJ, 0, SEEK_END);
	int RecordCount = (ftell(COMOBJ) - 2) / 11;
	fseek(COMOBJ, 2, SEEK_SET);

	std::vector<unsigned char> ItemProperty;

	for (int i = 0; i < RecordCount; i++) {
		unsigned char ItemBuffer[11];  // Easier to just read the full item in and pick out targets
		fread(ItemBuffer, 1, 11, COMOBJ);

		ItemProperty.push_back((ItemBuffer[10] & 0x0F) | (ItemBuffer[7] & 0x80));
	}
	fclose(COMOBJ);

// Create export files and set headers
	sprintf(TempPath, "%s\\%s_Data.csv", OutPath.c_str(), SourcePath);
	FILE* SaveOut = fopen(TempPath, "w");
	fprintf(SaveOut, "Offset,Size,Property,Value\n");

	sprintf(TempPath, "%s\\%s_Inventory.csv", OutPath.c_str(), SourcePath);
	FILE* InvOut = fopen(TempPath, "w");
	fprintf(InvOut, "ObjectID,ItemID,ItemName,Condition,Enchantment,Identified,OwnedBy,NextObjectID,LinkID,Flags,IsEnchanted,IsDirection,IsInvisible,IsQuantity,Heading,PosX,PosY,PosZ,Quality,Owner,Quantity,Property\n");

	sprintf(TempPath, "%s\\%s\\PLAYER.DAT", UWPath.c_str(), SourcePath);
	FILE* SAVFile = fopen(TempPath, "rb");

// Get strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());


// Create data buffers
	std::vector<unsigned char> SAVData(995);

// UW2 stores the inventory object count in the unencrypted data so we can use that instead of calculating off of file size -- Note:  Count is inventory objects + 1 - I'm assuming it's padding in the player object and so I will too
	fseek(SAVFile, 894, SEEK_SET);
	int ItemCount = fgetc(SAVFile);
	fseek(SAVFile, 0, SEEK_SET);

	std::vector<std::vector<unsigned short int>> ItemData(ItemCount, std::vector<unsigned short int>(4));

	// Decrypt data
	{
	// Read in raw data
		std::vector<unsigned char> InBuffer(995);
		for (int i = 0; i < 995; i++) {
			InBuffer[i] = fgetc(SAVFile);
		}

	// Get inventory objects
		for (int i = 1; i < ItemCount; i++) {
			unsigned short int ItemByte[4];
			fread(ItemByte, sizeof(short int), 4, SAVFile);
			for (int b = 0; b < 4; b++) {
				ItemData[i][b] = ItemByte[b];
			}
		}

	// Add in player object
		{
			fseek(SAVFile, 896, SEEK_SET);
			unsigned short int ItemByte[4];
			fread(ItemByte, sizeof(short int), 4, SAVFile);
			for (int b = 0; b < 4; b++) {
				ItemData[0][b] = ItemByte[b];
			}
		}

	// Get "Magic Byte"
		unsigned char MagicByte = InBuffer[0];
		MagicByte += 7;

	// Build "Magic Array"
		std::vector<int> MagicArray(80);
		for (int i = 0; i < 80; i++) {
			MagicByte += 0x06;
			MagicArray[i] = MagicByte;
		}

		for (int i = 0; i < 16; i++) {
			MagicByte += 0x07;
			MagicArray[i * 5] = MagicByte;
		}

		for (int i = 0; i < 4; i++) {
			MagicByte += 0x29;
			MagicArray[i * 12] = MagicByte;
		}

		for (int i = 0; i < 11; i++) {
			MagicByte += 0x49;
			MagicArray[i * 7] = MagicByte;
		}

	// Add the "Magic Byte" to output -- Note:  Original, not modified value for the array
		SAVData[0] = InBuffer[0];

	// Decrypt data -- 0x0001-0x037D
		int CurrentByte = 1;

	// Data is XORed against the Magic Array in a loop of 80 byte blocks excluding the last block which is only 12 bytes
		for (int b = 0; b < 12; b++) {
			[&] {
			// The first byte in each block is only XORed against the Magic Array
				SAVData[CurrentByte] = (InBuffer[CurrentByte] ^ MagicArray[0]);
				CurrentByte++;

				for (int i = 1; i < 0x50; ++i) {
				// Break out of loop once we've hit byte 0x37D (892) -- The remaining data is unencrypted
					if (CurrentByte == 0x037D) {
						return;
					}

				// Remaining bytes in the block are XORed against the previous byte's decrypted data + the previous byte's encrypted data + the matching Magic Array value
					SAVData[CurrentByte] = (InBuffer[CurrentByte] ^ (SAVData[CurrentByte - 1] + InBuffer[CurrentByte - 1] + MagicArray[i])) & 0xFF;
					CurrentByte++;

				}
			}();
		}

	// Now add remaining unencrypted data
		while (CurrentByte < 995) {
			SAVData[CurrentByte] = InBuffer[CurrentByte];
			CurrentByte++;
		}
	}

// Export -- Done in segments
// 0000 - Magic Byte
	fprintf(
		SaveOut,
		"0000,"		// Offset
		"1,"		// Size
		"Magic Byte,"	// Property
		"%02X\n",		// Value
		SAVData[0]		//Value
	);


// 0001-000E -- Name
	{
	// Assume there's a better way to do this...
		std::string Name = "";
		for (int i = 1; i < 30; i++) {
			if (SAVData[i] == 0x00) {
				break;
			}
			Name += SAVData[i];
		}
	
		fprintf(
			SaveOut,
			"0001,"	// Offset
			"30,"	// Size
			"Name,"	// Description
			"%s\n",	// Value
			Name.c_str()	//Value
		);

	}

	/*** This is wrong - Max name length appears to actually be based on the number of characters _or_ the size of the name pixel wise - whichever is greater
		Verified by entering ............................. as a name which is 29 characters long
		and by entering AAAAAAAAAAA and getting capped at 11 characters

		Using FONTCHAR as reference, periods have the smallest character size for that font at 3 pixels wide
		and capital A has the widest size at 9 pixels

		3 x 29 is a length of 87 pixels
		9 x 11 is a length of 99 pixels

		My guess is it requires at least one padding byte as a break, so 29 character cap, and also is ensuring you're <= 100 pixels wide to avoid issues in the UI (characters overflowing)

		Not really worth calling out the padding/break byte so I'm just going to show the name field as 30 bytes

	// 000E-001D -- Unused
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"16,"		// Size
			"Unused,"	// Description
			"\n",		// Value
			14 + FileByteOffset	// Offset
		);
	***/

// 001F-003D -- Attributes/Stats
	for (int v = 31; v < 63; v++) {
		const char* Property;
		switch (v) {
		case 31:	Property = "Strength"; break;
		case 32:	Property = "Dexterity"; break;
		case 33:	Property = "Intelligence"; break;
		case 34:	Property = "Attack"; break;
		case 35:	Property = "Defense"; break;
		case 36:	Property = "Unarmed"; break;
		case 37:	Property = "Sword"; break;
		case 38:	Property = "Axe"; break;
		case 39:	Property = "Mace"; break;
		case 40:	Property = "Missile"; break;
		case 41:	Property = "Mana"; break;
		case 42:	Property = "Lore"; break;
		case 43:	Property = "Casting"; break;
		case 44:	Property = "Traps"; break;
		case 45:	Property = "Search"; break;
		case 46:	Property = "Track"; break;
		case 47:	Property = "Sneak"; break;
		case 48:	Property = "Repair"; break;
		case 49:	Property = "Charm"; break;
		case 50:	Property = "Picklock"; break;
		case 51:	Property = "Acrobat"; break;
		case 52:	Property = "Appraise"; break;
		case 53:	Property = "Swimming"; break;
		case 54:	Property = "Vitality (Current)"; break;
		case 55:	Property = "Vitality (Max)"; break;
		case 56:	Property = "Mana (Current)"; break;
		case 57:	Property = "Mana (Max)"; break;
		case 58:	Property = "Hunger"; break;
		case 59:	Property = "Fatigue"; break;
		case 60:	Property = "Unknown"; break;
		case 61:	Property = "Unknown"; break;
		case 62:	Property = "Level (Player)"; break;
		default:	Property = "Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X,"	// Offset
			"1,"	// Size
			"%s,"	// Property
			"%u\n",	// Value
			v,			// Offset
			Property,	// Property
			SAVData[v]	// Value
		);
	}

// 003F-0044 -- Active Spells
	{
		// Cancelled spells left in data and ignored based on another value in the file, so get that value to mark inactive spells
		int ActiveSpellCount = (SAVData[97] & 0xC0) >> 6;

		for (int s = 0; s < 3; s++) {
			int SpellID = ((SAVData[63 + (s * 2)] & 0xF0) >> 4) | ((SAVData[63 + (s * 2)] & 0x0F) << 4);

			std::string SpellName = CleanDisplayName(gs.get_string(6, SpellID == 0 ? 511 : SpellID).c_str(), true, false);
			if ((s + 1) > ActiveSpellCount && SpellID != 0) {
				SpellName += " (Inactive)";
			}

			fprintf(
				SaveOut,
				"%04X,"					// Offset
				"1,"					// Size
				"ActiveSpell%u_Name,"	// Property
				"%s\n",					// Value
				63 + (s * 2),		// Offset
				s + 1,				// Property
				SpellName.c_str()	// Value -- Note: Pushing SpellID out of range if 00
			);

			fprintf(
				SaveOut,
				"%04X,"						// Offset
				"1,"						// Size
				"ActiveSpell%u_Duration,"	// Property
				"%s\n",						// Value
				63 + (s * 2) + 1,					// Offset
				s + 1,								// Property
				SpellName == "" ? "" : std::to_string(SAVData[63 + (s * 2) + 1]).c_str()	// Value
			);
		}
	}

// 0045 -- RuneOwned (A-H)
	{
		std::string RuneList = "";
		for (int b = 7; b >= 0; b--) {
			if ((SAVData[69] & (0x01 << b)) == (0x01 << b)) {
				std::string RuneName = "";
				switch (b) {
					case 0:	RuneName = "Hur"; break;
					case 1: RuneName = "Grav"; break;
					case 2: RuneName = "Flam"; break;
					case 3: RuneName = "Ex"; break;
					case 4: RuneName = "Des"; break;
					case 5: RuneName = "Corp"; break;
					case 6: RuneName = "Bet"; break;
					case 7: RuneName = "An"; break;
				}
				if (RuneList == "") {
					RuneList += RuneName;
				}
				else {
					RuneList += "/" + RuneName;
				}
			}
		}
		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"1,"				// Size
			"RuneOwned_A-H,"	// Property
			"%s\n",				// Value
			69,						// Offset
			RuneList.c_str()		// Value
		);
	}

// 0046 -- RuneOwned (I-P)
	{
		std::string RuneList = "";
		for (int b = 7; b >= 0; b--) {
			if ((SAVData[70] & (0x01 << b)) == (0x01 << b)) {
				std::string RuneName = "";
				switch (b) {
					case 0: RuneName = "Por"; break;
					case 1: RuneName = "Ort"; break;
					case 2: RuneName = "Nox"; break;
					case 3: RuneName = "Mani"; break;
					case 4: RuneName = "Lor"; break;
					case 5: RuneName = "Kal"; break;
					case 6: RuneName = "Jux"; break;
					case 7: RuneName = "In"; break;
				}
				if (RuneList == "") {
					RuneList += RuneName;
				}
				else {
					RuneList += "/" + RuneName;
				}
			}
		}
		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"1,"				// Size
			"RuneOwned_I-P,"	// Property
			"%s\n",				// Value
			70,						// Offset
			RuneList.c_str()		// Value
		);
	}

// 0047 -- RuneOwned (Q-Y) -- Note:  No X rune
	{
		std::string RuneList = "";
		for (int b = 7; b >= 0; b--) {
			if ((SAVData[71] & (0x01 << b)) == (0x01 << b)) {
				std::string RuneName = "";
				switch (b) {
					case 0: RuneName = "Ylem"; break;
					case 1: RuneName = "Wis"; break;
					case 2: RuneName = "Vas"; break;
					case 3: RuneName = "Uus"; break;
					case 4: RuneName = "Tym"; break;
					case 5: RuneName = "Sanct"; break;
					case 6: RuneName = "Rel"; break;
					case 7: RuneName = "Quas"; break;
				}
				if (RuneList == "") {
					RuneList += RuneName;
				}
				else {
					RuneList += "/" + RuneName;
				}
			}
		}
		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"1,"				// Size
			"RuneOwned_Q-Y,"	// Property
			"%s\n",				// Value
			71,						// Offset
			RuneList.c_str()		// Value
		);
	}

// 0048-004A -- RuneActive
	for (int i = 72; i < 75; i++) {
		std::string RuneName = "";
		switch (SAVData[i]) {
			case  0: RuneName = "An"; break;
			case  1: RuneName = "Bet"; break;
			case  2: RuneName = "Corp"; break;
			case  3: RuneName = "Des"; break;
			case  4: RuneName = "Ex"; break;
			case  5: RuneName = "Flam"; break;
			case  6: RuneName = "Grav"; break;
			case  7: RuneName = "Hur"; break;
			case  8: RuneName = "In"; break;
			case  9: RuneName = "Jux"; break;
			case 10: RuneName = "Kal"; break;
			case 11: RuneName = "Lor"; break;
			case 12: RuneName = "Mani"; break;
			case 13: RuneName = "Nox"; break;
			case 14: RuneName = "Ort"; break;
			case 15: RuneName = "Por"; break;
			case 16: RuneName = "Quas"; break;
			case 17: RuneName = "Rel"; break;
			case 18: RuneName = "Sanct"; break;
			case 19: RuneName = "Tym"; break;
			case 20: RuneName = "Uus"; break;
			case 21: RuneName = "Vas"; break;
			case 22: RuneName = "Wis"; break;
			case 23: RuneName = "Ylem"; break;
		}
		fprintf(
			SaveOut,
			"%04X,"			// Offset
			"1,"			// Size
			"RuneActive%u,"	// Property
			"%s\n",			// Value
			i,					// Offset
			i - 71,				// Property
			RuneName.c_str()	// Value
		);
	}

// 004B-004C -- CurrentWeight --  Contrary documentation describes as number of items but weight matches exactly in my tests so going with that
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"2,"				// Size
		"CurrentWeight,"	// Property
		"%.1f\n",			// Value
		75,											// Offset
		((SAVData[76] << 8) | SAVData[75]) * 0.1	// Value
	);

// 004D-004E -- MaxWeight
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"2,"			// Size
		"MaxWeight,"	// Property
		"%.1f\n",		// Value
		77,											// Offset
		((SAVData[78] << 8) | SAVData[77]) * 0.1	// Value
	);

// 004F-0052 -- Experience
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"4,"			// Size
		"Experience,"	// Property
		"%.1f\n",		// Value
		79,						// Offset
		((SAVData[82] << 24) | (SAVData[81] << 16) | (SAVData[80] << 8) | SAVData[79]) * 0.1	// Value
	);

// 0053 -- SkillPointAvail
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"SkillPointAvail,"	// Property
		"%u\n",				// Value
		83,				// Offset
		SAVData[83]		// Value
	);

// 0054 -- Unknown (Other docs show this as a duplicate of SkillPointAvail but does not match value in new test save, could be u16 though seems unlikely you'd bank enough to need the extra byte - don't think you could earn enough legit either)
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Unknown,"	// Property
		"%02X\n",	// Value
		84,				// Offset
		SAVData[84]		// Value
	);

// 0055 -- Unknown
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Unknown,"	// Property
		"%02X\n",	// Value
		85,				// Offset
		SAVData[85]		// Value
	);

// 0056-005B -- XYZ
	for (int b = 86; b < 92; b++) {
		const char* Property;
		switch (b) {
		case 86:	Property = "TileX"; break;
		case 87:	Property = "PosX"; break;	// Making a guess that this is a coordinate on the tile itself, similar to objects
		case 88:	Property = "TileY"; break;
		case 89:	Property = "PosY"; break;	// Making a guess that this is a coordinate on the tile itself, similar to objects
		case 90:	Property = "TileZ"; break;	// Not sure if this is an offset from the tile's height, value doesn't match up
		case 91:	Property = "PosZ"; break;	// In a couple test saves this was the same value as above, not sure how this works so will just treat same as X/Y
		default:	Property = "Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X,"	// Offset
			"1,"	// Size
			"%s,"	// Property
			"%u\n",	// Value
			b,			// Offset
			Property,	// Property
			SAVData[b]	// Value
		);
	}

// 005C -- Heading -- Leaving out of above to add direction translation NSEW at some point																		<---- REMINDER
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Heading,"	// Property
		"%u\n",		// Value
		92,				// Offset
		SAVData[92]		// Value
	);

// 005D -- Level (Dungeon)
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"Level_Dungeon,"	// Property
		"%u: %s\n",			// Value
		93,												// Offset
		SAVData[93], UW2LevelName(SAVData[93]).c_str()	// Value
	);

// 005E -- Unknown -- Possibly second byte of u16 value for dungeon level?
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Unknown,"	// Property
		"%u\n",		// Value
		94,				// Offset
		SAVData[94]		// Value
	);

// 005F -- Level (Moonstone 1)
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"Level_Moonstone1,"	// Property
		"%u: %s\n",			// Value
		95,												// Offset
		SAVData[95], UW2LevelName(SAVData[95]).c_str()	// Value
	);

// 0060 -- Level (Moonstone 2)
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"Level_Moonstone2,"	// Property
		"%u: %s\n",			// Value
		96,												// Offset
		SAVData[96], UW2LevelName(SAVData[96]).c_str()	// Value
	);

// 0061-0064 -- PlayerStatus -- Treated as an int with overlapping bit ranges so handle all together
	{
		int PlayerStatus = SAVData[97] | (SAVData[98] << 8) | (SAVData[99] << 16) | (SAVData[100] << 24);

	// 0061.00 -- BitArray -- WeaponDrawn
		fprintf(
			SaveOut,
			"0061.00,"			// Offset
			"2,"				// Size	-- Really just a boolean so only grabbing 1
			"WeaponDrawn,"		// Property
			"%u\n",				// Value
			PlayerStatus & 0x01		// Value
		);

	// 0061.02 -- BitArray -- Play_Poison
		fprintf(
			SaveOut,
			"0061.02,"			// Offset
			"4,"				// Size
			"Play_Poison,"		// Property
			"%u\n",				// Value
			(PlayerStatus >> 2) & 0x0F	// Value -- Need to mess with this to see how the value maps to strings (<blank>, barely, mildly, badly, seriously, & critically)
		);

	// 0061.06 -- BitArray -- Active Spell Count
		fprintf(
			SaveOut,
			"0061.06,"				// Offset
			"3,"					// Size -- Pretty sure this can only be 2 long but documented as using 3 bits
			"ActiveSpell_Count,"	// Property
			"%u\n",					// Value
			(PlayerStatus >> 6) & 0x07	// Value
		);

	// 0061.09 -- BitArray -- Rune Count
		fprintf(
			SaveOut,
			"0061.09,"		// Offset
			"2,"			// Size
			"RuneCount,"	// Property
			"%u\n",			// Value
			(PlayerStatus >> 9) & 0x03	// Value
		);

	// 0061.0B -- BitArray -- Armageddon Cast
		fprintf(
			SaveOut,
			"0061.0B,"			// Offset
			"1,"				// Size
			"ArmageddonCast,"	// Property
			"%u\n",				// Value
			(PlayerStatus >> 11) & 0x01	// Value
		);

	// 0061.0C -- BitArray -- Hallucination Level
		fprintf(
			SaveOut,
			"0061.0C,"				// Offset
			"2,"					// Size
			"HallucinationLevel,"	// Property
			"%u\n",					// Value
			(PlayerStatus >> 12) & 0x03	// Value
		);

	// 0061.0E -- BitArray -- Intoxication Level
		fprintf(
			SaveOut,
			"0061.0E,"				// Offset
			"6,"					// Size
			"IntoxicationLevel,"	// Property
			"%u\n",					// Value
			(PlayerStatus >> 14) & 0x3F	// Value -- Ultima Facts: You can get much drunker than you can trip
		);

	// 0061.14 -- BitArray -- Intoxication Level
		fprintf(
			SaveOut,
			"0061.14,"			// Offset
			"1,"				// Size
			"AutomapActive,"	// Property
			"%u\n",				// Value
			(PlayerStatus >> 20) & 0x01	// Value
		);

	// 0061.15 -- BitArray -- Show Text Traps (apparently?)
		fprintf(
			SaveOut,
			"0061.15,"		// Offset
			"1,"			// Size
			"ShowTextTrap,"	// Property
			"%u\n",			// Value
			(PlayerStatus >> 21) & 0x01	// Value
		);

	// 0061.16 -- BitArray -- Dream Plant Counter
		fprintf(
			SaveOut,
			"0061.16,"				// Offset
			"3,"					// Size
			"DreamPlantCounter,"	// Property
			"%u\n",					// Value
			(PlayerStatus >> 22) & 0x07	// Value
		);

	// 0061.19 -- BitArray -- IsDreaming
		fprintf(
			SaveOut,
			"0061.19,"		// Offset
			"1,"			// Size
			"IsDreaming,"	// Property
			"%u\n",			// Value
			(PlayerStatus >> 25) & 0x01	// Value
		);

	// 0061.1A -- BitArray -- IsDueling
		fprintf(
			SaveOut,
			"0061.1A,"		// Offset
			"1,"			// Size
			"IsDueling,"	// Property
			"%u\n",			// Value
			(PlayerStatus >> 26) & 0x01	// Value
		);

	// 0061.1B-1F -- Unknown -- No idea if used or if any/all are grouped together so just going to dump them individually
		for (int b = 27; b < 32; b++) {
			fprintf(
				SaveOut,
				"0061.%02X,"	// Offset
				"1,"			// Size
				"Unknown,"		// Property
				"%u\n",			// Value
				b,				// Offset
				(PlayerStatus >> b) & 0x01	// Value
			);
		}
	}

// 0065 -- LightLevel -- Don't know that the first nibble is used?
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"1,"			// Size
		"LightLevel,"	// Property
		"%u: %s\n",		// Value
		101,	// Offset
		SAVData[101] >> 4, CleanDisplayName(gs.get_string(6, SAVData[101] >> 4).c_str(), true, false).c_str()	// Value
	);

// 0066 -- Generic character details
	{
	// Handedness
		fprintf(
			SaveOut,
			"%04X.0,"		// Offset
			"1,"			// Size
			"Handedness,"	// Property
			"%s\n",			// Value
			102,												// Offset
			(SAVData[102] & 0x01) == 0 ? "0: Left" : "1: Right"	// Value
		);
	// Gender
		fprintf(
			SaveOut,
			"%04X.1,"	// Offset
			"1,"		// Size
			"Gender,"	// Property
			"%s\n",		// Value
			102,												// Offset
			(SAVData[102] & 0x02) == 0 ? "0: Male" : "1: Female"// Value
		);
	// BodyType
		fprintf(
			SaveOut,
			"%04X.2,"	// Offset
			"3,"		// Size
			"BodyType,"	// Property
			"%u\n",		// Value
			102,						// Offset
			(SAVData[102] >> 2) & 0x03	// Value
		);
	// Class
		fprintf(
			SaveOut,
			"%04X.5,"	// Offset
			"3,"		// Size
			"Class,"	// Property
			"%u: %s\n",	// Value
			102,	// Offset
			SAVData[102] >> 5, CleanDisplayName(gs.get_string(2, (SAVData[102] >> 5) + 23), true, false).c_str()	// Value
		);
	}

/***
	0067-00E6 -- Quest - Flags
	First 128 Quest values are stored kinda odd, with 4 quests stored in 4 byte blocks but one the first 4 bits of the first byte (little endian) used

	Somewhat baffled by the thought process behind this
***/
	for (int b = 0; b < 32; b++) {
		for (int q = 0; q < 4; q++) {
			std::string QuestName = UW2Quest[(b * 4) + q];
			if (QuestName != "") {
				QuestName = "_" + QuestName;
			}

			fprintf(
				SaveOut,
				"%04X.%u,"			// Offset (+Bit)
				"4,"				// Size
				"Quest%03u%s,"	// Property
				"%u\n",														// Value
				103 + (b * 4), q,											// Offset (+Bit)
				(b * 4) + q, QuestName.c_str(),								// Property
				((SAVData[103 + (b * 4)] >> q) & 0x01) == 0x01 ? 1 : 0		// Value
			);
		}
	}

/***
	00E7-00F6 -- Quest - Bytes
	Next 16 Quest values are stored as full bytes with 3 known to be world bitfields
***/
	for (int q = 0; q < 16; q++) {
		std::string QuestValue = "";
		unsigned int QuestID = 128 + q;

		std::string QuestName = UW2Quest[QuestID];
		if (QuestName != "") {
			QuestName = "_" + QuestName;
		}

	// PrisonTowerPassword -- Note:  Password is not case sensitive -- Note:  Supposidly is reused as BrainCreature kill counter in KK2 - making the assumption it uses the second nibble for that but leaving out until tested
		if (QuestID == 134) {
			switch (SAVData[231 + q] & 0x0F) {
				case 0: QuestValue = "0: NotSet"; break;	// Password is (semi) randomly set in conversation when first asked password or told it by Borne if 0
				case 1:	QuestValue = "1: Swordfish"; break;
				case 2:	QuestValue = "2: Melanoma"; break;
				case 3:	QuestValue = "3: Silhouette"; break;
				case 4:	QuestValue = "4: Cyclone"; break;
				case 5:	QuestValue = "5: Harbinger"; break;
				case 6:	QuestValue = "6: Meander"; break;
				case 7:	QuestValue = "7: Quicksilver"; break;
				case 8:	QuestValue = "8: Shibboleth"; break;
				case 9:	QuestValue = "9: Fisticuffs"; break;
			};
		}
	// LinesOfPowerCut/BlackrockGemsUsed/WorldsKnown -- BitFields
		else if (QuestID == 128 || QuestID == 130 || QuestID == 131) {
			for (int b = 0; b < 8; b++) {
				if ((SAVData[231 + q] & (0x01 << b)) == (0x01 << b)) {
					std::string WorldName = "";
					switch (b) {
						case 0: WorldName = "Prison Tower"; break;
						case 1: WorldName = "Killorn Keep"; break;
						case 2: WorldName = "Ice Caverns"; break;
						case 3: WorldName = "Talorus"; break;
						case 4: WorldName = "Scintillus Academy"; break;
						case 5: WorldName = "Tomb of Praecor Loth"; break;
						case 6: WorldName = "Pits of Carnage"; break;
						case 7: WorldName = "Ethereal Void"; break;
					}
					if (QuestValue == "") {
						QuestValue += WorldName;
					}
					else {
						QuestValue += "/" + WorldName;
					}
				}
			}

			if (QuestValue == "") {
				QuestValue = "None";
			}
		}
	// CutsceneToPlay
		else if (QuestID == 143) {
		// Skip if 00 - That would point to the intro cutscene
			if (SAVData[231 + q] != 0x00) {
				char CutSceneID[5];
				sprintf(CutSceneID, "CS%03o", SAVData[231 + q]);

				QuestValue = CutSceneID;
			}
			else {
				QuestValue = "None";
			}
		}
	// ArenaOpponentsKilled/JospurDebt/BloodWormsKilled -- Numeric values
		else if (QuestID == 129 || QuestID == 133 || QuestID == 135) {
			QuestValue = std::to_string(SAVData[231 + q]);
		}
	// Unknown -- Leave as hex
		else {
			char UnknownValue[2];
			sprintf(UnknownValue, "%02X", SAVData[231 + q]);

			QuestValue = UnknownValue;
		}

		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"1,"				// Size
			"Quest%03u%s,"	// Property
			"%s\n",				// Value
			231 + q,				// Offset
			q + 128, QuestName.c_str(),	// Property
			QuestValue.c_str()		// Value
		);
	}

// 00F7-00F9 -- Unknown
	for (int b = 0; b < 3; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Property
			"%02X\n",	// Value
			247 + b,			// Offset
			SAVData[247 + b]	// Value
		);
	}

/***
	00FA-01F9 -- Variables - All known usage (except SA vending machines) appears to be treating it as a numeric value so going with that for all (known/unknown)
***/
	for (int v = 0; v < 128; v++) {
		std::string VariableName = UW2Variable[v];
		if (VariableName != "") {
			VariableName = "_" + VariableName;
		}

		std::string VariableValue = "";

	// SA Vending Machines
		if (v == 31 || v == 32 || v == 33) {
			switch (SAVData[250 + (v * 2)]) {
				case 0:	VariableValue = "0: Fish"; break;
				case 1:	VariableValue = "1: Piece of Meat"; break;
				case 2:	VariableValue = "2: Bottle of Ale"; break;
				case 3:	VariableValue = "3: Leeches"; break;
				case 4:	VariableValue = "4: Bottle of Water"; break;
				case 5:	VariableValue = "5: Dagger"; break;
				case 6:	VariableValue = "6: Lockpick"; break;
				case 7:	VariableValue = "7: Torch"; break;
			}
		}
		else {
			VariableValue = std::to_string(SAVData[250 + (v * 2)] | (SAVData[251 + (v * 2)] << 8));
		}		

		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"2,"				// Size
			"Variable%03u%s,"	// Property
			"%s\n",			// Value
			250 + (v * 2),				// Offset
			v, VariableName.c_str(),	// Property
			VariableValue.c_str()		// Value
		);
	}

/***
	01FA-02F9 -- Bit Field Variables -- While classed as bit arrays, their usage is largely just as a single boolean flag or numeric so going with that for these as well
***/
	for (int v = 0; v < 128; v++) {
		std::string BitFieldName = UW2BitField[v];
		if (BitFieldName != "") {
			BitFieldName = "_" + BitFieldName;
		}

		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"2,"				// Size
			"BitField%03u%s,"	// Property
			"%u\n",				// Value
			506 + (v * 2),											// Offset
			v, BitFieldName.c_str(),								// Property
			SAVData[506 + (v * 2)] | (SAVData[507 + (v * 2)] << 8)	// Value
		);
	}

/***
	02FA-0301 -- Dream Level Location
	The documentation on these are a little unclear to me so no idea on the accuracy of these

	It sort of reads wrong to me as it doesn't have the position in the tile and/or the Z tile/pos
	Seems much more likely that they'd want to store all of that here and use the regular values for your positioning in the void

	I'll need to get a character up to that point (or cheat in a dream plant) to test
***/
// 02FA -- AutomapBackup -- Stores whether the automap was enabled prior to entering dream/void -- Left out of loop to do bit check
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"AutomapBackup,"	// Description
		"%u\n",				// Value
		762,					// Offset
		SAVData[762] & 16		// Value
	);

// 02FB-02FF -- XY
	for (int b = 763; b < 768; b++) {
		const char* Property;
		switch (b) {
			case 763:	Property = "Unknown"; break;		// Not sure what this is supposed to be if used at all
			case 764:	Property = "TileX_Dream"; break;	// I'm reading this as being the X tile in the void / player's current location while dreaming
			case 765:	Property = "TileX_Orig"; break;		// I'm reading this as being the X tile in the level the player was in before dreaming
			case 766:	Property = "TileY_Dream"; break;	// I'm reading this as being the Y tile in the void / player's current location while dreaming
			case 767:	Property = "TileY_Orig"; break;		// I'm reading this as being the Y tile in the level the player was in before dreaming
			default:	Property = "Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X,"	// Offset
			"1,"	// Size
			"%s,"	// Property
			"%u\n",	// Value
			b,			// Offset
			Property,	// Property
			SAVData[b]	// Value
		);
	}

// 0300 -- Heading -- Really can't parse what has been documented here, making a complete guess this is what's intended
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"1,"			// Size
		"Heading_Orig,"	// Property -- Coin flip as to which this should be (Dream/Orig) assuming it's this at all
		"%u\n",			// Value
		768,				// Offset
		SAVData[768]		// Value
	);
	
// 0301 -- Level (DungeonOrig)
	fprintf(
		SaveOut,
		"%04X,"					// Offset
		"1,"					// Size
		"Level_DungeonOrig,"	// Property
		"%u: %s\n",				// Value
		769,												// Offset
		SAVData[301], UW2LevelName(SAVData[769] + 1).c_str()	// Value
	);

// 0302 -- Option-Difficulty
	{
		std::string Value;
		switch (SAVData[770]) {
			case 0:		Value = "0: Standard"; break;
			case 1:		Value = "1: Easy"; break;
			default:	Value = std::to_string(SAVData[770]) + ": Unknown"; break;
		}
	
		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"1,"				// Size
			"Option_Difficulty,"// Property
			"%s\n",				// Value
			770,			// Offset
			Value.c_str()	// Value
		);
	}

// 0303 -- Option-Settings
	{
	// Sound
		fprintf(
			SaveOut,
			"%04X.0,"		// Offset
			"2,"			// Size	-- Think this is 2 bits even though it's just a boolean 1/0 value
			"Option_Sound,"	// Property
			"%s\n",			// Value
			771,											// Offset
			(SAVData[771] & 0x01) == 1 ? "1: On" : "0: Off"	// Value
		);

	// Music
		fprintf(
			SaveOut,
			"%04X.2,"		// Offset
			"2,"			// Size	-- Think this is 2 bits even though it's just a boolean 1/0 value
			"Option_Music,"	// Property
			"%s\n",			// Value
			771,											// Offset
			(SAVData[771] & 0x04) == 4 ? "1: On" : "0: Off"	// Value
		);

	// Detail
		std::string Value;
		switch (SAVData[771] >> 4) {
			case 0:		Value = "0: Low"; break;
			case 1:		Value = "1: Medium"; break;
			case 2:		Value = "2: High"; break;
			case 3:		Value = "3: Very High"; break;
			default:	Value = std::to_string(SAVData[771]) + ": Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X.4,"			// Offset
			"4,"				// Size
			"Option_Detail,"	// Property
			"%s\n",				// Value
			771,			// Offset
			Value.c_str()	// Value
		);
	}

// 304 -- Motion State -- Multiple can be flagged (at least Swimming & Entering Water (17) has been shown as valid) so need to concatenate list
	{
		std::string Value = std::to_string(SAVData[772]) + ": ";
		std::string Delim = "";

		if (SAVData[772] == 0x00) {
			Value += "Unknown"; // I'm assuming?
		}
		else {
			for (unsigned int b = 0; b < 8; b++) {
				if (((SAVData[772] >> b) & 0x01) == 0x01) {
					Value += Delim + UW2MotionState[b];
					Delim = "/";
				}
			}
		}

		fprintf(
			SaveOut,
			"%04X,"			// Offset
			"1,"			// Size
			"MotionState,"	// Property
			"%s\n",			// Value
			772,				// Offset
			Value.c_str()		// Value
		);
	}

// 305 -- Unknown -- May just be second byte of short value
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Unknown,"	// Property
		"%02X\n",	// Value
		773,			// Offset
		SAVData[773]	// Value
	);

// 306 -- Paralyze Duration -- Assuming this is seconds
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"ParalyzeDuration,"	// Property
		"%u\n",				// Value
		774,			// Offset
		SAVData[774]	// Value
	);

// 0307 -- Tile State -- A little unclear how this is used though I suspect multiple could be flagged so handle the same as MotionState
	{
		std::string Value = std::to_string(SAVData[775]) + ": ";
		std::string Delim = "";

		if (SAVData[775] == 0x00) {
			Value += "On Ground"; // I'm assuming?
		}
		else {
			for (unsigned int b = 0; b < 8; b++) {
				if (((SAVData[775] >> b) & 0x01) == 0x01) {
					Value += Delim + UW2TileState[b];
					Delim = "/";
				}
			}
		}

		fprintf(
			SaveOut,
			"%04X,"			// Offset
			"1,"			// Size
			"TileState,"	// Property
			"%s\n",			// Value
			775,				// Offset
			Value.c_str()		// Value
		);
	}

// 0308 -- DrownTimer -- Assuming this is seconds
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"DrownTimer,"		// Property
		"%u\n",				// Value
		776,			// Offset
		SAVData[776]	// Value
	);

// 0309-0310 -- Unknown
	for (int b = 0; b < 8; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Property
			"%02X\n",	// Value
			777 + b,			// Offset
			SAVData[777 + b]	// Value
		);
	}

// 0311-0360 -- Lore Level
	for (int b = 0; b < 80; b++) {
		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"1,"				// Size
			"Lore_Level%02u,"	// Property
			"%u\n",				// Value
			785 + b,				// Offset
			b,						// Property
			SAVData[785 + b]		// Value
		);
	}

// 0361-0365 -- Pit Opponent Index -- I'm assuming this is an index to the mobile object in LEVARK -- No intention of pulling that in just to grab that info so just going to provide that value
	for (int b = 1; b < 6; b++) {
		fprintf(
			SaveOut,
			"%04X,"					// Offset
			"1,"					// Size
			"PitOpponentIndex%u,"	// Property
			"%s\n",					// Value
			864 + b,																// Offset
			b,																		// Property
			SAVData[864 + b] == 0 ? "" : std::to_string(SAVData[864 + b]).c_str()	// Value
		);
	}

// 0366-0369 -- Unknown
	for (int b = 0; b < 4; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Property
			"%02X\n",	// Value
			870 + b,			// Offset
			SAVData[870 + b]	// Value
		);
	}

// 036A-036D -- GameTime
	{
		int GameTime = SAVData[875] + (SAVData[876] << 8) + (SAVData[877] << 16);

		short int Day = GameTime / 86400;
		short int Hour = (GameTime - (Day * 86400)) / 3600;
		short int Minute = (GameTime - (Day * 86400) - (Hour * 3600)) / 60;
		short int Second = (GameTime - (Day * 86400) - (Hour * 3600) - (Minute * 60));
		short int Milli = SAVData[874] * (1000/255); // Think this is what this is but could be wrong on conversion - may have a max value of 249 so it's an even 4:1 ratio of game ticks to milli

		fprintf(
			SaveOut,
			"%04X,"								// Offset
			"4,"								// Size
			"GameTime,"							// Property
			"Day %u at %02u:%02u:%02u.%03u\n",	// Value
			874,					// Offset
			Day + 1, Hour, Minute, Second, Milli	// Value -- Formatting this the same as UW1 save
		);
	}

// 036E-037D -- XClock
// 036E -- XClock0 -- The Clock XClock
	{
		int XClockValue = SAVData[878];

		std::string XCHour = "";
		std::string XCMinute = "";

	// Set hour -- Starts at current time at start of game which is 5AM so need to offset
		if (XClockValue < 15) {
			XCHour = "0" + std::to_string(((XClockValue * 20) / 60) + 5);
		}
		else if (XClockValue > 56) {
			XCHour = "0" + std::to_string(((XClockValue * 20) / 60) -19);
		}
		else {
			XCHour = std::to_string(((XClockValue * 20) / 60) + 5);
		}

	// Set minute
		if (XClockValue % 3 == 0) {
			XCMinute = "00";
		}
		else {
			XCMinute = (XClockValue * 20) % 60;
		}

		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"XClock0,"	// Property
			"%u: %s:%s\n",	// Value
			878,											// Offset
			XClockValue, XCHour.c_str(), XCMinute.c_str()	// Value
		);
	}

// 036F -- XClock1 -- Main Quest -- Game is open ended in several spots so can't say for certain where you're necessarily at based on XClock value -- Could make a good guess by probing quest flags but meh 
	{
		std::string XClockValue = "";

		switch (SAVData[879]) {
			case  0:	XClockValue = "Trapped"; break;
			case  1:	XClockValue = "EnteredSewers"; break;
			case  2:	XClockValue = "PrisonTower"; break;
			case  3:	XClockValue = "BritanniaGoesDry"; break;
			case  4:	XClockValue = "LaborDispute"; break;
			case  5:	XClockValue = "IceCaverns/KillornKeep"; break;
			case  6:	XClockValue = "IceCaverns/KillornKeep"; break;
			case  7:	XClockValue = "WaterFound"; break;
			case  8:	XClockValue = "Murder"; break;
			case  9:	XClockValue = "PitsOfCarnage/ScintallusAcademy/Talorus"; break;
			case 10:	XClockValue = "PitsOfCarnage/ScintallusAcademy/Talorus"; break;
			case 11:	XClockValue = "PitsOfCarnage/ScintallusAcademy/Talorus"; break;
			case 12:	XClockValue = "SpeakToNelson"; break;
			case 13:	XClockValue = "TraitorDead"; break;
			case 14:	XClockValue = "EtherealVoid/TombOfPraecorLoth"; break;
			case 15:	XClockValue = "EtherealVoid/TombOfPraecorLoth"; break;
			case 16:	XClockValue = "Invasion"; break;
			default:	XClockValue = "Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X,"					// Offset
			"1,"					// Size
			"XClock1_MainQuest,"	// Property
			"%u: %s\n",				// Value
			879,								// Offset
			SAVData[879], XClockValue.c_str()	// Value
		);
	}

// 0370 -- XClock2 -- Blackrock Gems Used -- Pretty sure this is a straight counter, doesn't map to world gem is from -- Will modify if discover that is case
	fprintf(
		SaveOut,
		"%04X,"							// Offset
		"1,"							// Size
		"XClock2_BlackrockGemsUsed,"	// Property
		"%u\n",							// Value
		880,			// Offset
		SAVData[880]	// Value
	);

// 0371 -- XClock3 -- Djinn Quest
	{
		std::string XClockValue = "";

		switch (SAVData[881]) {
			case 0:		XClockValue = "NotStarted"; break;
			case 1:		XClockValue = "HaveDjinnBottle"; break;
			case 2:		XClockValue = "BasiliskOilMixed"; break;
			case 3:		XClockValue = "BathedInFilaniumMud"; break;
			case 4:		XClockValue = "BakedInLava"; break;
			case 5:		XClockValue = "IronFlesh"; break;
			case 6:		XClockValue = "DjinnCaptured"; break;
			default:	XClockValue = "Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X,"					// Offset
			"1,"					// Size
			"XClock3_DjinnQuest,"	// Property
			"%u: %s\n",				// Value
			881,								// Offset
			SAVData[881], XClockValue.c_str()	// Value
		);
	}

// 0372-037F -- Remaining XClocks
	for (int b = 4; b < 16; b++) {
		std::string XClockName = "";

		if (b == 14) {
			XClockName = "_ArenaVictories";
		}
		else if (b == 15) {
			XClockName = "_SCDCounter";
		}

		fprintf(
			SaveOut,
			"%04X,"			// Offset
			"1,"			// Size
			"XClock%u%s,"	// Property
			"%u\n",			// Value
			878 + b,				// Offset
			b, XClockName.c_str(),	// Property
			SAVData[878 + b]		// Value
		);
	}

// 037E-037F -- ItemCount -- Items in inventory +1 -- Guessing it's counting the player as an object as it behaves like a top level container
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"2,"			// Size
		"ItemCount,"	// Property
		"%u\n",			// Value
		894,								// Offset
		SAVData[894] | (SAVData[895] << 8)	// Value
	);

// 0380-0387 -- Player Static Object -- Object is included in inventory file so not going to bother duplicating that here
	{
	// Kind of a squirrelly way to do this
		std::string StaticObject = "";
		for (int b = 0; b < 4; b++) {
			char SOByte[4];
			sprintf(SOByte, "%02X%02X", SAVData[897 + (b * 2)], SAVData[896 + (b * 2)]);
			StaticObject += SOByte;
		}

		fprintf(
			SaveOut,
			"%04X,"					// Offset
			"8,"					// Size
			"PlayerStaticObject,"	// Property
			"%s\n",					// Value
			896,						// Offset
			StaticObject.c_str()		// Value
		);
	}

// 0388-039A -- Player Mobile Object -- Just going to treat the same as in LEVARK and dump the full 19 byte value -- May update if/when LEVARK is flushed out
	{
	// Kind of a squirrelly way to do this
		std::string MobileObject = "";
		for (int b = 0; b < 19; b++) {
			char MOByte[2];
			sprintf(MOByte, "%02X", SAVData[904 + b]);
			MobileObject += MOByte;
		}

		fprintf(
			SaveOut,
			"%04X,"					// Offset
			"19,"					// Size
			"PlayerMobileObject,"	// Property
			"%s\n",					// Value
			904,						// Offset
			MobileObject.c_str()		// Value
		);
	}

// 039B-03A2 -- Unknown
	for (int b = 0; b < 8; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Property
			"%02X\n",	// Value
			923 + b,			// Offset
			SAVData[923 + b]	// Value
		);
	}

// 03A3-03C8 -- ItemList -- Originally thought I'd put the item name here but since I think it makes more sense to have a separate inventory export file, I'll leave the index value to map to that
	for (int b = 0; b < 19; b++) {
	// ObjectID index looks to be handled the same as in LEV.ARK, where the index is bits 6-15 (0-5 is wall texture in LEV.ARK, I'm assuming this is just going to be 000000 for all of these)
		short int ItemIndex = (SAVData[931 + (b * 2)] | (SAVData[932 + (b * 2)] << 8)) >> 6;

		std::string Property;
		switch (b) {
			case  0:	Property = "Equip_Head"; break;
			case  1:	Property = "Equip_Armor"; break;
			case  2:	Property = "Equip_Gloves"; break;
			case  3:	Property = "Equip_Leggings"; break;
			case  4:	Property = "Equip_Boots"; break;
			case  5:	Property = "Equip_ShoulderRight"; break;
			case  6:	Property = "Equip_ShoulderLeft"; break;
			case  7:	Property = "Equip_HandRight"; break;
			case  8:	Property = "Equip_HandLeft"; break;
			case  9:	Property = "Equip_RingRight"; break;
			case 10:	Property = "Equip_RingLeft"; break;
			case 11:	Property = "Backpack_Item0"; break;
			case 12:	Property = "Backpack_Item1"; break;
			case 13:	Property = "Backpack_Item2"; break;
			case 14:	Property = "Backpack_Item3"; break;
			case 15:	Property = "Backpack_Item4"; break;
			case 16:	Property = "Backpack_Item5"; break;
			case 17:	Property = "Backpack_Item6"; break;
			case 18:	Property = "Backpack_Item7"; break;
		}

		fprintf(
			SaveOut,
			"%04X,"	// Offset
			"2,"	// Size
			"%s,"	// Property
			"%s\n",	// Value
			931 + (b * 2),											// Offset
			Property.c_str(),										// Property
			ItemIndex == 0 ? "" : std::to_string(ItemIndex).c_str()	// Value -- Blank if nothing
		);
	}

// 03C9-03E2 -- Unknown
	for (int b = 0; b < 26; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Property
			"%02X\n",	// Value
			969 + b,			// Offset
			SAVData[969 + b]	// Value
		);
	}

	fclose(SaveOut);


// Process list
	for (int i = 0; i < ItemCount; i++) {
	// Get ItemID
		int ItemID = ItemData[i][0] & 0x01FF;

	// Flag used for multiple things so go ahead and grab now
		int IsQuantity = ItemData[i][0] >> 15;

	// Want these to show blank based on values so define here (probably a better way to do this but ehh)
		char NextObjectID[10] = "";
		if (ItemData[i][2] >> 6 > 0) {
			sprintf(NextObjectID, "%u", ItemData[i][2] >> 6);
		}

		char LinkID[10] = "";
		if (IsQuantity == 0 && ItemData[i][3] >> 6 > 0) {
			sprintf(LinkID, "%u", ItemData[i][3] >> 6);
		}

		char Quantity[10] = "";
		if (IsQuantity == 1 && ItemData[i][3] >> 6 < 512) {
			sprintf(Quantity, "%u", ItemData[i][3] >> 6);
		}

		char Property[10] = "";
		short int PropertyID = 512;	// Used for enchantment lookup
		if (IsQuantity == 1 && (ItemData[i][3] >> 6) >= 512) {
			PropertyID = (ItemData[i][3] >> 6) - 512;
			sprintf(Property, "%u", PropertyID);
		}

	// Get item condition (where applicable)
		char Condition[16] = "";
		// First get the string block for the item type
		unsigned char QualityTypeID = (ItemProperty[ItemID] & 0x0F);

		if (QualityTypeID < 15) {	// Exclude out of range objects
			unsigned char QualityStringID = ItemData[i][2] & 0x003F;	// Get the item's quality
			/***
				Condition is broken up into 6 condition ranges based on the item's quality:
					0:		Broken
					1-16:	Heavily Damaged
					17-32:	Damaged
					33-45:	Lightly Damaged
					46-62:	Good	(Note: This _usually_ is the same value as perfect)
					63:		Perfect

				Note: Verbiage above is general description of the range, not the condition string
			***/
			if (QualityStringID == 0) {
				QualityStringID = 0;
			}
			else if (QualityStringID <= 16) {
				QualityStringID = 1;
			}
			else if (QualityStringID <= 32) {
				QualityStringID = 2;
			}
			else if (QualityStringID <= 45) {
				QualityStringID = 3;
			}
			else if (QualityStringID < 63) {
				QualityStringID = 4;
			}
			else {
				QualityStringID = 5;
			}

			sprintf(Condition, "%s", gs.get_string(5, (QualityTypeID * 6) + QualityStringID).c_str());
		}

	// Get enchantment (where applicable)
		char IsEnchanted = (ItemData[i][0] >> 12) & 0x01;
		char Enchantment[32] = "";
		std::string IDed = "";

	// Check if enchanted flag set and valid item type/property or if wand/sceptre item type with a link
		if ((IsEnchanted == 1 && ItemID < 320 && PropertyID < 512) || (ItemID >= 144 && ItemID <= 175 && IsEnchanted == 0 && LinkID != "")) {
			short int EnchantedItemID = ItemID;
			short int EnchantedPropertyID = PropertyID;

			if (IsEnchanted == 0) {
				short int EnchantedObjectID = ItemData[i][3] >> 6;
				EnchantedItemID = ItemData[EnchantedObjectID][0] & 0x01FF;

			// Spell
				if (EnchantedItemID == 288) {
					EnchantedPropertyID = (ItemData[EnchantedObjectID][3] >> 6) - 512;
				}
			// Push out of range if not spell
				else {
					EnchantedPropertyID = 367;
				}
			}

		// Weapon - Standard
			if (EnchantedItemID < 32 && EnchantedPropertyID >= 192 && EnchantedPropertyID <= 207) {
				sprintf(Enchantment, "%s", gs.get_string(6, EnchantedPropertyID + 256).c_str());
			}
		// Armor - Standard
			else if (EnchantedItemID >= 32 && EnchantedItemID < 64 && EnchantedPropertyID >= 192 && EnchantedPropertyID <= 207) {
				sprintf(Enchantment, "%s", gs.get_string(6, EnchantedPropertyID + 272).c_str());
			}
		// Weapon/Armor & Fountain - Other types
			else if (EnchantedItemID < 64 || EnchantedItemID == 302) {
				sprintf(Enchantment, "%s", gs.get_string(6, EnchantedPropertyID).c_str());
			}
		// Other types
			else if (EnchantedPropertyID < 64) {
				sprintf(Enchantment, "%s", gs.get_string(6, EnchantedPropertyID + 256).c_str());
			}
		// Other types
			else {
				sprintf(Enchantment, "%s", gs.get_string(6, EnchantedPropertyID + 144).c_str());
			}

		// Get identification state
			if ((ItemData[i][0] & 0x1000) == 0x1000) {
				switch ((ItemData[i][1] >> 7) & 0x03) {
					case 0:	IDed = "No"; break;
					case 1: IDed = "No"; break;			// Don't think it should get to this state, I had to manually mess with it to set it and it treats as unidentified
					case 2: IDed = "Partial"; break;	// Indicates you know the item is "magical"
					case 3: IDed = "Yes"; break;
				}

			// Append an * to indicate if ID has been attempted -- Probably need to have _some_ actual documentation as no one will know what that means without looking at the code, which they should, it's pretty great
				if ((ItemData[i][1] & 0x0200) == 0x0200 && IDed != "Yes") {	// Don't bother flagging if already IDed
					IDed += "*";
				}
			}
		}

	// Get owner (where applicable) -- Have no idea if this value is kept once it hits your inventory - Need to steal some crap and check and will remove if cleared
		char OwnedBy[24] = "";
		if ((ItemProperty[ItemID] & 0x80) == 0x80 && (ItemData[i][3] & 0x003F) > 0) {
			sprintf(OwnedBy, "%s", CleanDisplayName(gs.get_string(1, (ItemData[i][3] & 0x003F) + 370), true, false).c_str());
		}

	// Extract to CSV
		fprintf(InvOut,
			"%u,"	// ObjectID
			"%u,"	// ItemID
			"%s,"	// ItemName
			"%s,"	// Condition
			"%s,"	// Enchantment
			"%s,"	// Identified
			"%s,"	// OwnedBy
			"%s,"	// NextObjectID
			"%s,"	// LinkID
			"%u,"	// Flags
			"%u,"	// IsEnchanted
			"%u,"	// IsDirection
			"%u,"	// IsInvisible
			"%u,"	// IsQuantity
			"%u,"	// Heading
			"%u,"	// PosX	-- Not entirely sure what these 3 mean for items in inventory, if anything, may just carry over what it originally had and doesn't bother adjusting until you drop it
			"%u,"	// PosY
			"%u,"	// PosZ
			"%u,"	// Quality
			"%u,"	// Owner
			"%s,"	// Quantity
			"%s\n",	// Property
			i,								// ObjectID -- Inventory items start at 1 but player object is padded in for UW2 so don't increment
			ItemID,							// ItemID
			CleanDisplayName(gs.get_string(4, ItemData[i][0] & 0x01FF).c_str(), true, false).c_str(),	// ItemName
			Condition,						// Condition
			Enchantment,					// Enchantment
			IDed.c_str(),					// Identified
			OwnedBy,						// OwnedBy
			NextObjectID,					// NextObjectID
			LinkID,							// LinkID
			(ItemData[i][0] >> 9) & 0x07,   // Flags
			IsEnchanted,					// IsEnchanted
			(ItemData[i][0] >> 13) & 0x01,  // IsDirection
			(ItemData[i][0] >> 14) & 0x01,  // IsInvisible
			IsQuantity,						// IsQuantity
			(ItemData[i][1] >> 7) & 0x07,   // Heading
			ItemData[i][1] >> 13,           // PosX
			(ItemData[i][1] >> 10) & 0x07,  // PosY
			ItemData[i][1] & 0x007F,        // PosZ
			ItemData[i][2] & 0x003F,        // Quality
			ItemData[i][3] & 0x003F,        // Owner
			Quantity,						// Quantity
			Property						// Property
		);
	}
	fclose(InvOut);
	fclose(SAVFile);

	return 0;
}
