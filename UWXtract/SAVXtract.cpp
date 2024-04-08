/*************
	UWXtract - Ultima Underworld Extractor

	PLAYER.DAT decoder/extractor

	Extracts details from game saves

	Todo:
		Add UW2 saves (currently UW1 only)
		Might add some sort of item path like:
			ItemName (<ObjectID>)\ItemName (<ObjectID>)\<Position>
			Pack (1)\Sack (3)\2
*************/
#include "UWXtract.h"

extern bool AvailSaveGame(std::string UWPath, int SaveID);		// Util.cpp
extern std::string ByteToBitArray(const unsigned char ByteIn);	// Util.cpp

int ProcessUW1SAV(
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

// Check if encrypted (DATA file is not) -- technically if you're messing with things it may not be, not going to bother but some sort of validation if name characters out of standard alpha range or odd byte count could be a better check
	bool IsEncrypted = strstr(TempPath, "DATA") == NULL;
	unsigned char xorvalue = IsEncrypted ? fgetc(SAVFile) : 0;

// Add this to the byte offset to align exported value with the file (Encrypted has leading XOR value byte)
	int FileByteOffset = IsEncrypted ? 1 : 0;

// Read in SAVE data (excluding items)
	unsigned char SAVData[311];
	fread(SAVData, 1, 311, SAVFile);

// Decrypt
	if (IsEncrypted) {
		unsigned char incrnum = 3;

		for (int i = 0; i <= 219; i++) {
			if (i == 80 || i == 160) {
				incrnum = 3;
			}

			SAVData[i] ^= (xorvalue + incrnum);
			incrnum += 3;
		}
	}

// Export -- Done in segments -- Note:  Offset in comments going forward is 0 based like the DATA unencrypted file
// 0000-000D -- Name
	fprintf(
		SaveOut,
		"%04X,"	// Offset
		"14,"	// Size
		"Name,"	// Description
		"%s\n",	// Value
		0 + FileByteOffset,	// Offset
		SAVData				//Value
	);

// 000E-001D -- Unused
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"16,"		// Size
		"Unused,"	// Description
		"\n",		// Value
		14 + FileByteOffset	// Offset
	);

// 001E-003D -- Attributes/Stats
	for (int v = 30; v < 62; v++) {
		const char* Property;
		switch (v) {
			case 30:	Property = "Strength"; break;
			case 31:	Property = "Dexterity"; break;
			case 32:	Property = "Intelligence"; break;
			case 33:	Property = "Attack"; break;
			case 34:	Property = "Defense"; break;
			case 35:	Property = "Unarmed"; break;
			case 36:	Property = "Sword"; break;
			case 37:	Property = "Axe"; break;
			case 38:	Property = "Mace"; break;
			case 39:	Property = "Missile"; break;
			case 40:	Property = "Mana"; break;
			case 41:	Property = "Lore"; break;
			case 42:	Property = "Casting"; break;
			case 43:	Property = "Traps"; break;
			case 44:	Property = "Search"; break;
			case 45:	Property = "Track"; break;
			case 46:	Property = "Sneak"; break;
			case 47:	Property = "Repair"; break;
			case 48:	Property = "Charm"; break;
			case 49:	Property = "Picklock"; break;
			case 50:	Property = "Acrobat"; break;
			case 51:	Property = "Appraise"; break;
			case 52:	Property = "Swimming"; break;
			case 53:	Property = "Vitality (Current)"; break;
			case 54:	Property = "Vitality (Max)"; break;
			case 55:	Property = "Mana (Current)"; break;
			case 56:	Property = "Mana (Max)"; break;
			case 57:	Property = "Hunger"; break;
			case 58:	Property = "Fatigue"; break;
			case 59:	Property = "Unknown"; break;
			case 60:	Property = "Unknown"; break;
			case 61:	Property = "Level (Player)"; break;
			default:	Property = "Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X,"	// Offset
			"1,"	// Size
			"%s,"	// Description
			"%u\n",	// Value
			v + FileByteOffset,	// Offset
			Property,			// Description
			SAVData[v]			// Value
		);
	}

// 003E-0043 -- Active Spells
	{
	// Cancelled spells left in data and ignored based on another value in the file, so get that value to mark inactive spells
		int ActiveSpellCount = (SAVData[95] & 0xC0) >> 6;

		for (int s = 0; s < 3; s++) {
			int SpellID = ((SAVData[62 + (s * 2)] & 0xF0) >> 4) | ((SAVData[62 + (s * 2)] & 0x0F) << 4);

			std::string SpellName = CleanDisplayName(gs.get_string(6, SpellID == 0 ? 511 : SpellID).c_str(), true, false);
			if ((s + 1) > ActiveSpellCount && SpellID != 0) {
				SpellName += " (Inactive)";
			}

			fprintf(
				SaveOut,
				"%04X,"					// Offset
				"1,"					// Size
				"ActiveSpell%u_Name,"	// Description
				"%s\n",					// Value
				62 + FileByteOffset + (s * 2),	// Offset
				s + 1,							// Description
				SpellName.c_str()				// Value -- Note: Pushing SpellID out of range if 00
			);

			fprintf(
				SaveOut,
				"%04X,"						// Offset
				"1,"						// Size
				"ActiveSpell%u_Duration,"	// Description
				"%s\n",						// Value
				62 + FileByteOffset + (s * 2) + 1,	// Offset
				s + 1,								// Description
				SpellName == "" ? "" : std::to_string(SAVData[62 + (s * 2) + 1]).c_str()	// Value
			);
		}
	}

// 0044 -- RuneOwned (A-H)
	{
		std::string RuneList = "";
		for (int b = 7; b >= 0; b--) {
			if ((SAVData[68] & (0x01 << b)) == (0x01 << b)) {
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
			"RuneOwned_A-H,"	// Description
			"%s\n",				// Value
			68 + FileByteOffset,	// Offset
			RuneList.c_str()		// Value
		);
	}

// 0045 -- RuneOwned (I-P)
	{
		std::string RuneList = "";
		for (int b = 7; b >= 0; b--) {
			if ((SAVData[68] & (0x01 << b)) == (0x01 << b)) {
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
			"RuneOwned_I-P,"	// Description
			"%s\n",				// Value
			69 + FileByteOffset,	// Offset
			RuneList.c_str()		// Value
		);
	}

// 0046 -- RuneOwned (Q-Y) -- Note:  No X rune
	{
		std::string RuneList = "";
		for (int b = 7; b >= 0; b--) {
			if ((SAVData[68] & (0x01 << b)) == (0x01 << b)) {
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
			"RuneOwned_Q-Y,"	// Description
			"%s\n",				// Value
			70 + FileByteOffset,	// Offset
			RuneList.c_str()		// Value
		);
	}

// 0047-0049 -- RuneActive
	for (int i = 71; i < 74; i++) {
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
			"RuneActive%u,"	// Description
			"%s\n",			// Value
			i + FileByteOffset,	// Offset
			i - 70,				// Description
			RuneName.c_str()	// Value
		);
	}

// 004A-004B -- CurrentWeight --  Contrary documentation describes as number of items but weight matches exactly in my tests so going with that
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"2,"				// Size
		"CurrentWeight,"	// Description
		"%.1f\n",			// Value
		74 + FileByteOffset,						// Offset
		((SAVData[75] << 8) | SAVData[74]) * 0.1	// Value
	);

// 004C-004D -- MaxWeight
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"2,"			// Size
		"MaxWeight,"	// Description
		"%.1f\n",		// Value
		76 + FileByteOffset,						// Offset
		((SAVData[77] << 8) | SAVData[76]) * 0.1	// Value
	);

// 004E-0051 -- Experience
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"4,"			// Size
		"Experience,"	// Description
		"%.1f\n",		// Value
		78 + FileByteOffset,	// Offset
		((SAVData[81] << 24) | (SAVData[80] << 16) | (SAVData[79] << 8) | SAVData[78]) * 0.1	// Value
	);

// 0052 -- SkillPointAvail
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"SkillPointAvail,"	// Description
		"%u\n",				// Value
		82 + FileByteOffset,	// Offset
		SAVData[82]				// Value
	);

// 0053 -- Unknown (Other docs show this as a duplicate of SkillPointAvail but does not match value in new test save, could be u16 though seems unlikely you'd bank enough to need the extra byte - don't think you could earn enough legit either)
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Unknown,"	// Description
		"%02X\n",	// Value
		83 + FileByteOffset,	// Offset
		SAVData[83]				// Value
	);

// 0054 -- Unknown
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Unknown,"	// Description
		"%02X\n",	// Value
		84 + FileByteOffset,	// Offset
		SAVData[84]				// Value
	);

// 0055-005A -- XYZ
	for (int b = 85; b < 91; b++) {
		const char* Property;
		switch (b) {
			case 85:	Property = "TileX"; break;
			case 86:	Property = "PosX"; break;	// Making a guess that this is a coordinate on the tile itself, similar to objects
			case 87:	Property = "TileY"; break;
			case 88:	Property = "PosY"; break;	// Making a guess that this is a coordinate on the tile itself, similar to objects
			case 89:	Property = "TileZ"; break;	// Not sure if this is an offset from the tile's height, value doesn't match up
			case 90:	Property = "PosZ"; break;	// In a couple test saves this was the same value as above, not sure how this works so will just treat same as X/Y
			default:	Property = "Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X,"	// Offset
			"1,"	// Size
			"%s,"	// Description
			"%u\n",	// Value
			b + FileByteOffset,	// Offset
			Property,			// Description
			SAVData[b]			// Value
		);
	}

// 005B -- Heading -- Leaving out of above to add direction translation NSEW at some point																		<---- REMINDER
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Heading,"	// Description
		"%u\n",		// Value
		91 + FileByteOffset,	// Offset
		SAVData[91]				// Value
	);

// 005C -- Level (Dungeon)
	fprintf(
		SaveOut,
		"%04X,"				// Offset
		"1,"				// Size
		"Level_Dungeon,"	// Description
		"%u\n",				// Value
		92 + FileByteOffset,	// Offset
		SAVData[92]				// Value
	);

// 005D -- Unknown -- Possibly second byte of u16 value for dungeon level?
	fprintf(
		SaveOut,
		"%04X,"		// Offset
		"1,"		// Size
		"Unknown,"	// Description
		"%u\n",		// Value
		93 + FileByteOffset,	// Offset
		SAVData[93]				// Value
	);

// 005E.0-3 -- BitArray -- Moon Stone Level -- Note to self:  Lookup if there is a standard notation for a byte and bits that isn't just a mask
	fprintf(
		SaveOut,
		"%04X.0,"			// Offset
		"4,"				// Size
		"MoonStoneLevel,"	// Description
		"%u\n",				// Value
		94 + FileByteOffset,	// Offset
		SAVData[94] & 0x0F		// Value
	);

// 005E.4-7 -- BitArray -- Silver Tree Level
	fprintf(
		SaveOut,
		"%04X.4,"			// Offset
		"4,"				// Size
		"SilverTreeLevel,"	// Description
		"%u\n",				// Value
		94 + FileByteOffset,		// Offset
		(SAVData[94] & 0xF0) >> 4	// Value
	);

// 005F.0-1 -- BitArray -- Incense Dream Stage
	fprintf(
		SaveOut,
		"%04X.0,"			// Offset
		"2,"				// Size
		"IncenseDreamStage,"// Description
		"%u\n",				// Value
		95 + FileByteOffset,	// Offset
		SAVData[95] & 0x03		// Value
	);

// 005F.2-5 -- BitArray -- Play_Poison
	fprintf(
		SaveOut,
		"%04X.2,"		// Offset
		"4,"			// Size
		"Play_Poison,"	// Description
		"%u\n",			// Value
		95 + FileByteOffset,		// Offset
		(SAVData[95] & 0x3C) >> 2	// Value
	);

// 005F.6-7 -- BitArray -- Active Spell Count
	fprintf(
		SaveOut,
		"%04X.6,"				// Offset
		"2,"					// Size
		"ActiveSpell_Count,"	// Description
		"%u\n",					// Value
		95 + FileByteOffset,		// Offset
		(SAVData[95] & 0xC0) >> 6	// Value
	);

// 0060 -- BitArray -- For bit arrays, just going to go with a boolean row for any unknown bits
	for (int b = 0; b < 8; b++) {
			std::string Property;
			switch (b) {
				case 5:	Property = "OrbDestroyed"; break;
				case 7:	Property = "CupOfWonder"; break;
				default:	Property = "Unknown"; break;
			}
			fprintf(
				SaveOut,
				"%04X.%u,"	// Offset
				"1,"		// Size
				"%s,"		// Description
				"%u\n",		// Value
				96 + FileByteOffset, b,		// Offset
				Property.c_str(),			// Description
				(SAVData[96] >> b) & 0x01	// Value
			);
	}

// 0061-0062 -- Short value but just manually handling the overlap
	{
		for (int b = 0; b < 2; b++) {
			fprintf(
				SaveOut,
				"%04X.%u,"	// Offset
				"1,"		// Size
				"Unknown,"	// Description
				"%u\n",		// Value
				97 + FileByteOffset, b,		// Offset
				(SAVData[97] >> b) & 0x01	// Value
			);
		}

		fprintf(
			SaveOut,
			"%04X.2,"				// Offset
			"2,"					// Size
			"HallucinationLevel,"	// Description -- Assuming that's what shrooms is referring to?
			"%u\n",					// Value
			97 + FileByteOffset,		// Offset
			(SAVData[97] >> 2) & 0x03	// Value
		);

		fprintf(
			SaveOut,
			"%04X.4,"				// Offset
			"6,"					// Size
			"IntoxicationLevel,"	// Description -- Intoxication sounds classier than drunk :P
			"%u\n",					// Value
			97 + FileByteOffset,								// Offset
			(SAVData[97] >> 4) | ((SAVData[98] & 0x03) << 8)	// Value
		);

		fprintf(
			SaveOut,
			"%04X.A,"			// Offset
			"2,"				// Size
			"GaramonBuried,"	// Description -- So far as I know this is treated like a boolean, not sure why there's 2 bits
			"%u\n",				// Value
			97 + FileByteOffset,		// Offset
			(SAVData[98] >> 2) & 0x03	// Value
		);

		for (int b = 12; b < 16; b++) {
			fprintf(
				SaveOut,
				"%04X.%X,"	// Offset
				"1,"		// Size
				"Unknown,"	// Description
				"%u\n",		// Value
				97 + FileByteOffset, b,		// Offset
				(SAVData[98] >> b) & 0x01	// Value
			);
		}

	}

// 0063 -- LightLevel -- Don't know that the first nibble is used?
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"1,"			// Size
		"LightLevel,"	// Description
		"%u: %s\n",		// Value
		99 + FileByteOffset,	// Offset
		SAVData[99] >> 4, CleanDisplayName(gs.get_string(6, SAVData[99] >> 4).c_str(), true, false).c_str()	// Value
	);

// 0064 -- Generic character details
	{
	// Handedness
		fprintf(
			SaveOut,
			"%04X.0,"		// Offset
			"1,"			// Size
			"Handedness,"	// Description
			"%s\n",			// Value
			100 + FileByteOffset,								// Offset
			(SAVData[100] & 0x01) == 0 ? "0: Left" : "1: Right"	// Value
		);
	// Gender
		fprintf(
			SaveOut,
			"%04X.1,"	// Offset
			"1,"		// Size
			"Gender,"	// Description
			"%s\n",		// Value
			100 + FileByteOffset,								// Offset
			(SAVData[100] & 0x02) == 0 ? "0: Male" : "1: Female"// Value
		);
	// BodyType
		fprintf(
			SaveOut,
			"%04X.2,"	// Offset
			"3,"		// Size
			"BodyType,"	// Description
			"%u\n",		// Value
			100 + FileByteOffset,		// Offset
			(SAVData[100] >> 2) & 0x03	// Value
		);
	// Class
		fprintf(
			SaveOut,
			"%04X.5,"	// Offset
			"3,"		// Size
			"Class,"	// Description
			"%u: %s\n",	// Value
			100 + FileByteOffset,	// Offset
			SAVData[100] >> 5, CleanDisplayName(gs.get_string(2, (SAVData[100] >> 5) + 23).c_str(), true, false).c_str()	// Value
		);
	}

// 0065-0068 -- QuestFlag (Primary)
	{
		unsigned int QuestFlag = ((SAVData[104] << 24) | (SAVData[103] << 16) | (SAVData[102] << 8) | SAVData[101]);

		for (int b = 0; b < 32; b++) {
			std::string Property;
			switch (b) {
				case  0:	Property = "MurgoFree"; break;				// Set if freed by Sseetharee or Murgo talked to after door opened (breakout?)
				case  1:	Property = "HagbardMet"; break;				// Checked by Gulik
				case  2:	Property = "DrOwlMet"; break;				// Checked by Murgo
				case  3:	Property = "KetchavalPermission"; break;	// Set by Retichall -- Checked by Ketchaval
				case  4:	Property = "GazerDead"; break;				// Set by hardcode? -- Checked by mountain men -- Goldthirst gives reward
				case  5:	Property = "OrbDestroyed"; break;			// Checked to see if Avatar should question if Tyball's orb would have work if not destroyed (maybe :P)
				case  6:	Property = "LizardmanFriend"; break;		// Not sure where this is set/used?
				case  7:	Property = "MurgoCellOpen"; break;			// Set by door trigger/variable trap on cell door -- Checked by Murgo
				case  8:	Property = "BronusBookOpened"; break;		// Set if you lose book and he gives a second -- Checked if you lose it again -- not sure if if this soft locks you if you really did but probably should you dumbass
				case  9:	Property = "IllomoMet"; break;				// Set before Illomo talks, reads to me more as a met flag than a "find" flag -- Checked by Gurstang
				case 10:	Property = "ZakMet"; break;					// Set before Zak talks, reads to me more as a met flag than a "find" flag -- Checked by Delanrey
				case 11:	Property = "RodrickDead"; break;			// Set by hardcode? -- Checked by Biden/Dorna (maybe other knights, didn't check them all)
				default:	Property = "Unknown"; break;
			}

			fprintf(
				SaveOut,
				"%04X.%02X,"		// Offset
				"1,"				// Size
				"QuestFlag%02u_%s,"	// Description
				"%u\n",				// Value
				101 + FileByteOffset, b,	// Offset
				b, Property.c_str(),		// Description
				(QuestFlag >> b) & 0x01		// Value
			);
		}
	}

// 0069 -- QuestFlag (KnightOfTheCrux)
	{
		std::string Value;
		switch (SAVData[105]) {
			case  0:	Value = "0: Not started"; break;
			case  1:	Value = "1: Invited / Talk to Dorna"; break;		// Set by most knights -- Checked by Dorna
			case  2:	Value = "2: Esquire / Find writ of Lorne"; break;	// Set by Dorna after initiation -- Tasks you to find writ of Lorne -- Some knights check if quest at this level
			case  3:	Value = "3: Writ returned / Find Gold Plate"; break;// Set after writ returned to Dorna -- Immediately tasked to find gold plate
			case  4:	Value = "4: Knight / Armory open"; break;			// Set after plate returned to Dorna -- Full knight and armory opened -- Checked by some knights
			default:	Value = std::to_string(SAVData[105]) + ": Unknown"; break;	// Shouldn't happen
		}

		fprintf(
			SaveOut,
			"%04X,"							// Offset
			"1,"							// Size
			"QuestFlag32_KnightOfTheCrux,"	// Description
			"%s\n",							// Value
			105 + FileByteOffset,	// Offset
			Value.c_str()			// Value
		);
	}

// 006A-006C -- QuestFlag (Unknown)
	for (int b = 0; b < 3; b++) {
		fprintf(
			SaveOut,
			"%04X,"					// Offset
			"1,"					// Size
			"QuestFlag%u_Unknown,"	// Description
			"%02X\n",				// Value
			106 + FileByteOffset + b,	// Offset
			33 + b,						// Description
			SAVData[106 + b]			// Value
		);
	}

// 006D -- QuestFlag (TalismanLeft)
	fprintf(
		SaveOut,
		"%04X,"						// Offset
		"1,"						// Size
		"QuestFlag36_TalismanLeft,"	// Description
		"%u\n",						// Value
		109 + FileByteOffset,	// Offset
		SAVData[109]			// Value
	);

// 006E -- QuestFlag (GaramonDreamState)
	fprintf(
		SaveOut,
		"%04X,"							// Offset
		"1,"							// Size
		"QuestFlag37_GaramonDreamState,"// Description
		"%u\n",							// Value
		110 + FileByteOffset,	// Offset
		SAVData[110]			// Value
	);

// 006F -- QuestFlag (Unknown)
	fprintf(
		SaveOut,
		"%04X,"					// Offset
		"1,"					// Size
		"QuestFlag38_Unknown,"	// Description
		"%02X\n",				// Value
		111 + FileByteOffset,		// Offset
		SAVData[111]				// Value
	);

// 0070-00AF -- Variables
	for (int b = 0; b < 64; b++) {
		std::string Property;
		switch (b) {
			case 16:	Property = "_L3-DoorLockSwitch"; break;
			case 24:	Property = "_L4-BullfrogPuzzle"; break;
			case 27:	Property = "_L4-IngvarGraveRoomLock"; break;
			case 31:	Property = "_L5-MineTeleportSwitchLeft"; break;
			case 32:	Property = "_L5-MineTeleportSwitchCenter"; break;
			case 33:	Property = "_L5-MineTeleportSwitchRight"; break;
			case 34:	Property = "_L5-RingOfHumilitySwitchPuzzle"; break;	// Modified by NW/NE/SE switches and move triggers in center of room -- Checked by bottom left switch -- Note:  Move triggers basically scramble it
			case 50:	Property = "_L7-PrisonPortcullisLocked"; break;		// Set by move trigger on entering
			case 51:	Property = "_L7-PrisonPortcullisOpen"; break;		// Set by guard NPC 216 or 220
			default:	Property = ""; break;
		}

		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"1,"				// Size
			"Variable%02u%s,"	// Description
			"%02X\n",			// Value
			112 + FileByteOffset + b,	// Offset
			b, Property.c_str(),		// Description
			SAVData[112 + b]			// Value
		);
	}

// 00B0 -- Mana (Orb)
	fprintf(
		SaveOut,
		"%04X,"			// Offset
		"1,"			// Size
		"Mana_Max-Orb,"	// Description
		"%u\n",			// Value
		176 + FileByteOffset,	// Offset
		SAVData[176]			// Value
	);

// 00B1-00B3 -- Unknown
	for (int b = 177; b < 180; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Description
			"%02X\n",	// Value
			b + FileByteOffset,	// Offset
			SAVData[b]			// Value
		);
	}

// 00B4 -- Option-Difficulty
	{
		std::string Value;
		switch (SAVData[180]) {
			case 0:		Value = "0: Standard"; break;
			case 1:		Value = "1: Easy"; break;
			default:	Value = std::to_string(SAVData[180]) + ": Unknown"; break;
		}
	
		fprintf(
			SaveOut,
			"%04X,"				// Offset
			"1,"				// Size
			"Option-Difficulty,"// Description
			"%s\n",				// Value
			180 + FileByteOffset,	// Offset
			Value.c_str()			// Value
		);
	}

// 00B5 -- Option-Settings
	{
	// Sound
		fprintf(
			SaveOut,
			"%04X.0,"		// Offset
			"2,"			// Size	-- Think this is 2 bits even though it's just a boolean 1/0 value
			"Option_Sound,"	// Description
			"%s\n",			// Value
			181 + FileByteOffset,							// Offset
			(SAVData[181] & 0x01) == 1 ? "1: On" : "0: Off"	// Value
		);

	// Music
		fprintf(
			SaveOut,
			"%04X.2,"		// Offset
			"1,"			// Size	-- Think this is 2 bits even though it's just a boolean 1/0 value
			"Option_Music,"	// Description
			"%s\n",			// Value
			181 + FileByteOffset,							// Offset
			(SAVData[181] & 0x04) == 4 ? "1: On" : "0: Off"	// Value
		);

	// Detail
		std::string Value;
		switch (SAVData[181] >> 4) {
			case 0:		Value = "0: Low"; break;
			case 1:		Value = "1: Medium"; break;
			case 2:		Value = "2: High"; break;
			case 3:		Value = "3: Very High"; break;
			default:	Value = std::to_string(SAVData[180]) + ": Unknown"; break;
		}

		fprintf(
			SaveOut,
			"%04X.4,"			// Offset
			"4,"				// Size
			"Option_Detail,"	// Description
			"%s\n",				// Value
			181 + FileByteOffset,	// Offset
			Value.c_str()			// Value
		);
	}

// 00B6-00CD -- Unknown
	for (int b = 182; b < 206; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Description
			"%02X\n",	// Value
			b + FileByteOffset,	// Offset
			SAVData[b]			// Value
		);
	}

// 00CE-00D1 -- GameTime
	{
		int GameTime = SAVData[207] + (SAVData[208] << 8) + (SAVData[209] << 16);

		short int Day = GameTime / 86400;
		short int Hour = (GameTime - (Day * 86400)) / 3600;
		short int Minute = (GameTime - (Day * 86400) - (Hour * 3600)) / 60;
		short int Second = (GameTime - (Day * 86400) - (Hour * 3600) - (Minute * 60));
		short int Milli = SAVData[206] * (1000/255); // Think this is what this is but could be wrong on conversion - may have a max value of 249 so it's an even 4:1 ratio of game ticks to milli

		fprintf(
			SaveOut,
			"%04X,"								// Offset
			"4,"								// Size
			"GameTime,"							// Description
			"Day %u at %02u:%02u:%02u.%03u\n",	// Value
			206 + FileByteOffset,					// Offset
			Day + 1, Hour, Minute, Second, Milli	// Value -- In game it counts it as X day of imprisonment, so add 1 to day value to match
		);
	}

// 00D2-00DB -- Unknown
	for (int b = 210; b < 220; b++) {
	/***
		Note:	00D2/3 are documented as number of inventory items held plus 1 but that doesn't match up at all
				with my test on a new save with the starting bag next to you the only item picked up showing having
				a value of 8C/87 so I'm treating this as unknown
	***/
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Description
			"%02X\n",	// Value
			b + FileByteOffset,	// Offset
			SAVData[b]			// Value
		);
	}

// 00DC -- Vitality (Current/Unknown) -- Presumably used somewhere similar to 00B0 for mana
	fprintf(
		SaveOut,
		"%04X,"						// Offset
		"1,"						// Size
		"Vitality_Current-Unknown,"	// Description
		"%u\n",						// Value
		220 + FileByteOffset,	// Offset
		SAVData[220]			// Value
	);

// 00DD-00F6 -- Unknown
	for (int b = 221; b < 247; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Description
			"%02X\n",	// Value
			b + FileByteOffset,	// Offset
			SAVData[b]			// Value
		);
	}

// 00F7-011C -- ItemList -- Originally thought I'd put the item name here but since I think it makes more sense to have a separate inventory export file, I'll leave the index value to map to that
	for (int b = 0; b < 19; b++) {
	// ObjectID index looks to be handled the same as in LEV.ARK, where the index is bits 6-15 (0-5 is wall texture in LEV.ARK, I'm assuming this is just going to be 000000 for all of these)
		short int ItemIndex = (SAVData[247 + (b * 2)] | (SAVData[248 + (b * 2)] << 8)) >> 6;

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
			"1,"	// Size
			"%s,"	// Description
			"%s\n",	// Value
			247 + FileByteOffset + (b * 2),							// Offset
			Property.c_str(),										// Description
			ItemIndex == 0 ? "" : std::to_string(ItemIndex).c_str()	// Value -- Blank if nothing
		);
	}

// 011D-0136 -- Unknown
	for (int b = 285; b < 311; b++) {
		fprintf(
			SaveOut,
			"%04X,"		// Offset
			"1,"		// Size
			"Unknown,"	// Description
			"%02X\n",	// Value
			b + FileByteOffset,	// Offset
			SAVData[b]			// Value
		);
	}
	fclose(SaveOut);

// Get number of items
	fseek(SAVFile, 0, SEEK_END);
	int ItemCount = (ftell(SAVFile) - (311 + FileByteOffset)) / 8;
	fseek(SAVFile, 311 + FileByteOffset, SEEK_SET);

// Read in all inventory items
	std::vector<std::vector<unsigned short int>> ItemData(ItemCount, std::vector<unsigned short int>(4));
	for (int i = 0; i < ItemCount; i++) {
		unsigned short int ItemByte[4];
		fread(ItemByte, sizeof(short int), 4, SAVFile);
		for (int b = 0; b < 4; b++) {
			ItemData[i][b] = ItemByte[b];
		}
	}

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
			i + 1,							// ObjectID -- Inventory items start at 1
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

int SAVXtract(
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

				ProcessUW1SAV(SavePath, UWPath, OutPath);
			}
		}

		printf("Saved games extracted to %s\n", OutPath.c_str());
		return 0;
	}
	// Data only
	else if (ExportTarget == "d" || ExportTarget == "D") {
		// Create output folder (done here in case invalid parameter passed)
		CreateFolder(OutPath);
		ProcessUW1SAV("DATA", UWPath, OutPath);
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

		ProcessUW1SAV(SavePath, UWPath, OutPath);
	}

	printf("Saved game extracted to %s\n", OutPath.c_str());
	return 0;
}
