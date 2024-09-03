/*************
	UWXtract - Ultima Underworld Extractor

	DAT decoder/extractor

	Ridiculously modified version of the "hacking" miscdecode function by the
	Underworld Adventures Team to extract data from DAT files

	Includes the following files
	CMB.DAT
	COMOBJ.DAT
	DL.DAT (UW2 only)
	OBJECTS.DAT
	SHADES.DAT
	SKILLS.DAT
	SOUNDS.DAT
	TERRAIN.DAT
	WEAP.DAT (UW2 only)
	WEAPONS.DAT (UW1 only)

	Note:  Palette DAT files are handled by PALXtract & PLAYER.DAT by SAVXtract
*************/
#include "UWXtract.h"

int DATXtract(
	const bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
/*** Reusable variable for setting target file names
	Only going to bother noting this once and I use it the most here so why not
	But I had semi-random (at least to me :P) errors/issues when trying to combine
	the path parameter with a hardcode value in fopen to open the input/export files

	I know it _does_ work, I'm doing that in the GetPalette functions and string loading but whatever

	Almost certainly doing something wrong but just setting as a variable first
	then opening that works fine everytime so there we go
***/
	char TempPath[256];
	CreateFolder(OutPath);

// Get strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// CMB.DAT
	{
		sprintf(TempPath, "%s\\DATA\\CMB.DAT", UWPath.c_str());
		FILE* fd = fopen(TempPath, "rb");

	// Create CSV export file and header
		sprintf(TempPath, "%s\\CMB.csv", OutPath.c_str());
		FILE* out = fopen(TempPath, "w");
		fprintf(out, "Index,InItemID1,InItemName1,InItemDestroyed1,InItemID2,InItemName2,InItemDestroyed2,OutItemID,OutItemName\n");

	// Get number of records -- Should always be 10 (60 bytes)
		fseek(fd, 0, SEEK_END);
		int RecordCount = ftell(fd) / 6;
		fseek(fd, 0, SEEK_SET);

		for (int i = 0; i < RecordCount; i++) {
			unsigned short CMBBuffer[3];
			fread(CMBBuffer, sizeof(short int), 3, fd);

		// Skip if empty -- _should_ only occur in UW2
			if ((CMBBuffer[0] == 0 && CMBBuffer[1] == 0 && CMBBuffer[2] == 0) || (CMBBuffer[0] == 0xFFFF && CMBBuffer[1] == 0xFFFF && CMBBuffer[2] == 0xFFFF)) {
				fprintf(out, "%u,,,,,,,,\n", i);	// Log the empty record
				continue;
			}

		// Export to CSV
			fprintf(
				out,
				"%u,"	// Index
				"%u,"	// InItemID1
				"%s,"	// InItemName1
				"%u,"	// InItemDestroyed1
				"%u,"	// InItemID2
				"%s,"	// InItemName2
				"%u,"	// InItemDestroyed2
				"%u,"	// OutItemID
				"%s\n",	// OutItemName
				i,										// Index
				CMBBuffer[0] & 0x1ff,					// InItemID1
				CleanDisplayName(gs.get_string(4, CMBBuffer[0] & 0x1ff).c_str(), true, false).c_str(),	// InItemName1
				(CMBBuffer[0] & 0x8000) == 0 ? 0 : 1,	// InItemDestroyed1
				CMBBuffer[1] & 0x1ff,					// InItemID2
				CleanDisplayName(gs.get_string(4, CMBBuffer[1] & 0x1ff).c_str(), true, false).c_str(),	// InItemName2
				(CMBBuffer[1] & 0x8000) == 0 ? 0 : 1,	// InItemDestroyed2
				CMBBuffer[2],							// OutItemID
				CleanDisplayName(gs.get_string(4, CMBBuffer[2]).c_str(), true, false).c_str()			// OutItemName
			);
		}
		fclose(fd);
		fclose(out);
	}

// COMOBJ.DAT
	{
		sprintf(TempPath, "%s\\DATA\\COMOBJ.DAT", UWPath.c_str());
		FILE* fd = fopen(TempPath, "rb");

	// Create CSV export file and header
		sprintf(TempPath, "%s\\COMOBJ.csv", OutPath.c_str());
		FILE* out = fopen(TempPath, "w");
		fprintf(out, "ItemID,ClassID,SubClassID,SubClassIndex,ItemName,ItemString,Height,Radius,IsAnimated,Mass,x3b0,x3b1,IsUseable,IsTemp,IsDecal,x3b5,CanLink,CanStack,IsContainer,MonetaryValue,x6b0,x6b1,QualityClass,x6b4,x6b5,x6b6,x6b7,x7b0,DestroyChance,CanPickup,x7b6,CanBeOwned,ResistMagic,ResistPhysical,ResistFire,ResistPoison,ResistCold,ResistMissile,IsUndead,RenderType,CullingPriority,QualityType,LookAtDetail,xAb5,xAb6,xAb7\n");

	// Get number of records
		fseek(fd, 0, SEEK_END);
		int RecordCount = (ftell(fd) - 2) / 11;
		fseek(fd, 2, SEEK_SET);

	// Loop and export each record
		for (int i = 0; i < RecordCount; i++) {
		// Read in record data
			unsigned char buffer[11];
			fread(buffer, 1, 11, fd);

		// Get item name
			std::string ItemString = gs.get_string(4, i).c_str();
			std::string DisplayName = CleanDisplayName(ItemString, true, false).c_str();

		// Hardcode ItemName for some NPC types
			if (i > 63 && i < 128 && DisplayName == "") {
				if (!IsUW2) {
					if (i == 123) {
						DisplayName = "Tyball";
					}
					else if (i == 124) {
						DisplayName = "Slasher of Veils";
					}
					else {
						DisplayName = "Ethereal Void Creature";
					}
				}
				else {   // Only EV creatures don't have a name in 2
					DisplayName = "Ethereal Void Creature";
				}
			}

		// Several refs, easier to hit once
			unsigned int B12 = buffer[1] | (buffer[2] << 8);

		// Translate item rendering type
			const char* RenderType;
			switch (buffer[9] & 3) {
				case  0:	RenderType = "Sprite"; break;
				case  1:	RenderType = "NPC"; break;
				case  2:	RenderType = "3D Model"; break;
				case  3:	RenderType = "Texture"; break;
				default:	RenderType = "Unknown"; break;
			}

		// Hardcode RenderType to None if null item
			if (ItemString == "") {
				RenderType = "None";
			}

		// Export to CSV
			fprintf(
				out,
				"%u,"	// ItemID
				"%u,"	// ClassID
				"%u,"	// SubClassID
				"%u,"	// SubClassIndex
				"%s,"	// ItemName
				"%s,"	// ItemString
				"%u,"	// Height
				"%u,"	// Radius
				"%u,"	// IsAnimated
				"%.1f,"	// Mass
				"%u,"	// x3b0
				"%u,"	// x3b1
				"%u,"	// IsUseable
				"%u,"	// IsTemp
				"%u,"	// IsDecal
				"%u,"	// x3b5
				"%u,"	// CanLink
				"%u,"	// CanStack
				"%u,"	// IsContainer
				"%u,"	// MonetaryValue
				"%u,"	// x6b0
				"%u,"	// x6b1
				"%u,"	// QualityClass
				"%u,"	// x6b4
				"%u,"	// x6b5
				"%u,"	// x6b6
				"%u,"	// x6b7
				"%u,"	// x7b0
				"%u,"	// DestroyChance
				"%u,"	// CanPickup
				"%u,"	// x7b6
				"%u,"	// CanBeOwned
				"%u,"	// ResistMagic
				"%u,"	// ResistPhysical
				"%u,"	// ResistFire
				"%u,"	// ResistPoison
				"%u,"	// ResistCold
				"%u,"	// ResistMissile
				"%u,"	// IsUndead
				"%s,"	// RenderType
				"%u,"	// CullingPriority
				"%u,"	// QualityType
				"%u,"	// LookAtDetail
				"%u,"	// xAb5
				"%u,"	// xAb6
				"%u\n",	// xAb7
				i,									// ItemID
				(i & 0x01C0) >> 6,					// ClassID
				(i & 0x0030) >> 4,					// SubClassID
				i & 0x000F,							// SubClassIndex
				DisplayName.c_str(),				// ItemName
				ItemString.c_str(),					// ItemString
			// 00
				buffer[0] * 2,						// Height
			// 02/1
				B12 & 0x0007,						// Radius
				(B12 & 0x0008) >> 3,				// IsAnimated
				((B12 & 0xFFF0) >> 4) * 0.1,		// Mass
			// 03
				buffer[3] & 0x01,					// x3b0
				(buffer[3] & 0x02) >> 1,			// x3b1 -- Can stand on maybe?
				(buffer[3] & 0x04) >> 2,			// IsUseable
				(buffer[3] & 0x08) >> 3,			// IsTemp
				(buffer[3] & 0x10) >> 4,			// IsDecal
				(buffer[3] & 0x20) >> 5,			// x3b5    -- Documented as CanPickup but think this is wrong, x07 B5 has all same items _except_ the odd items included here that can't be picked up (campfire/etc) / maybe interact flag of some sort
				(buffer[3] & 0x40) >> 6,			// CanLink
				(buffer[3] & 0x40) == 0x40 ? 0 : 1,	// CanStack
				(buffer[3] & 0x80) >> 7,			// IsContainer
			// 05/4
				buffer[4] | (buffer[5] << 8),		// MonetaryValue
			// 06
				buffer[6] & 0x01,					// x6b0  -- Think may be collision flag?
				(buffer[6] & 0x02) >> 1,			// x6b1
				(buffer[6] & 0x0C) >> 2,			// QualityClass
				(buffer[6] & 0x10) >> 4,			// x6b4
				(buffer[6] & 0x20) >> 5,			// x6b5
				(buffer[6] & 0x40) >> 6,			// x6b6
				(buffer[6] & 0x80) >> 7,			// x6b7
			// 07
				buffer[7] & 0x01,					// x7b0
				(buffer[7] & 0x1E) >> 1,			// DestroyChance
				(buffer[7] & 0x20) >> 5,			// CanPickup
				(buffer[7] & 0x40) >> 6,			// x7b6
				(buffer[7] & 0x80) >> 7,			// CanBeOwned
			// 08
				(buffer[8] & 0x03) == 0x03 ? 1 : 0,	// ResistMagic -- Not sure why this is 2 bits - curious if 1 is damage & 1 is status but all usage has both set if not 00 so kinda moot
				(buffer[8] & 0x04) >> 2,			// ResistPhysical
				(buffer[8] & 0x08) >> 3,			// ResistFire
				(buffer[8] & 0x10) >> 4,			// ResistPoison
				(buffer[8] & 0x20) >> 5,			// ResistCold
				(buffer[8] & 0x40) >> 6,			// ResistMissile
				(buffer[8] & 0x80) >> 7,			// IsUndead
			// 09
				RenderType,							// RenderType
				(buffer[9] & 0xFC) >> 2,			// CullingPriority
			// 0A
				buffer[10] & 0x0F,					// QualityType
				(buffer[10] & 0x10) >> 4,			// LookAtDetail
				(buffer[10] & 0x20) >> 5,			// xAb5
				(buffer[10] & 0x40) >> 6,			// xAb6
				(buffer[10] & 0x80) >> 7			// xAb7
			);
		}
		fclose(fd);
		fclose(out);
	}

// DL.DAT -- UW2 only
	if (IsUW2) {
		sprintf(TempPath, "%s\\DATA\\DL.DAT", UWPath.c_str());
		FILE* fd = fopen(TempPath, "rb");

	// Create CSV export file and header
		sprintf(TempPath, "%s\\DL.csv", OutPath.c_str());
		FILE* out = fopen(TempPath, "w");
		fprintf(out, "LevelID,LightLevel\n");

		for (unsigned int i = 0; i < 80; i++) {
		// Export to CSV
			fprintf(
				out,
				"%u,"	// LevelID
				"%u\n",	// LightLevel
				i,			// LevelID
				fgetc(fd)	// LightLevel
			);
		}
		fclose(fd);
		fclose(out);
	}

// OBJECTS.DAT
	{
   // Contains multiple datasets for various distinct object types so exported as multiple files
		sprintf(TempPath, "%s\\DATA\\OBJECTS.DAT", UWPath.c_str());
		FILE* fd = fopen(TempPath, "rb");

	// MELEE
		{
		// Seek to target data set
			fseek(fd, 0x02, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_MELEE.csv", OutPath.c_str());
			FILE* OBJECT_MELEE = fopen(TempPath, "w");
			fprintf(OBJECT_MELEE, "ItemID,Name,SkillID,SkillName,Slash,Bash,Stab,Durability,Speed,MinCharge,MaxCharge\n");

			unsigned char buffer[8];
			for (int i = 0; i < 0x10; i++) {
				fread(buffer, 1, 8, fd);

			// Translate weapon skill name
				const char* SkillName;
				switch (buffer[6]) {
					case  3:	SkillName = "Sword"; break;
					case  4:	SkillName = "Axe"; break;
					case  5:	SkillName = "Mace"; break;
					case  6:	SkillName = "Unarmed"; break;
					default:	SkillName = "Unknown"; break;
				}

			// Export to CSV
				fprintf(
					OBJECT_MELEE,
					"%u,"	// ItemID
					"%s,"	// Name
					"%u,"	// SkillID
					"%s,"	// SkillName
					"%u,"	// Slash
					"%u,"	// Bash
					"%u,"	// Stab
					"%u,"	// Durability
					"%u,"	// Speed
					"%u,"	// MinCharge
					"%u\n",	// MaxCharge
					i,			// ItemID
					CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str(),	// Name
					buffer[6],	// SkillID
					SkillName,	// SkillName
					buffer[0],	// Slash
					buffer[1],	// Bash
					buffer[2],	// Stab
					buffer[7],	// Durability
					buffer[4],	// Speed
					buffer[3],	// MinCharge
					buffer[5]	// MaxCharge
				);
			}
			fclose(OBJECT_MELEE);
		}

	// RANGED
		{
		// Seek to target data set
			fseek(fd, 0x82, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_RANGED.csv", OutPath.c_str());
			FILE* OBJECT_RANGED = fopen(TempPath, "w");
			fprintf(OBJECT_RANGED, "ItemID,Name,Durability,x01,x02\n");

			unsigned char buffer[3];
			for (int i = 0x10; i < 0x20; i++) {
				fread(buffer, 1, 3, fd);

			// Export to CSV
				fprintf(
					OBJECT_RANGED,
					"%u,"		// ItemID
					"%s,"		// Name
					"%u,"		// Durability
					"%02X,"		// x01 -- Unknown
					"%02X\n",	// x02 -- Unknown
					i,				// ItemID
					CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str(),	// Name
					buffer[0],		// Durability
					buffer[1],		// x01 -- Unknown
					buffer[2]		// x02 -- Unknown
				);
			}
			fclose(OBJECT_RANGED);
		}

	// ARMOR
		{
		// Seek to target data set
			fseek(fd, 0xB2, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_ARMOR.csv", OutPath.c_str());
			FILE* OBJECT_ARMOR = fopen(TempPath, "w");
			fprintf(OBJECT_ARMOR, "ItemID,Name,CategoryID,CategoryName,Protection,Durability,x02\n");

			unsigned char buffer[4];
			for (int i = 0x20; i < 0x40; i++) {
				fread(buffer, 1, 4, fd);

			// Translate armor category type
				const char* CategoryName;
				switch (buffer[3]) {
					case  0:	CategoryName = "Shield"; break;
					case  1:	CategoryName = "Armor"; break;
					case  3:	CategoryName = "Leggings"; break;
					case  4:	CategoryName = "Gloves"; break;
					case  5:	CategoryName = "Boots"; break;
					case  8:	CategoryName = "Head"; break;
					case  9:	CategoryName = "Ring"; break;
					default:	CategoryName = "Unknown"; break;
				}

			// Export to CSV
				fprintf(
					OBJECT_ARMOR,
					"%u,"		// ItemID
					"%s,"		// Name
					"%u,"		// CategoryID
					"%s,"		// CategoryName
					"%u,"		// Protection
					"%u,"		// Durability
					"%02X\n",	// x02 -- Unknown
					i,				// ItemID
					CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str(),	// Name
					buffer[3],		// CategoryID
					CategoryName,	// CategoryyName
					buffer[0],		// Protection
					buffer[1],		// Durability
					buffer[2]		// x02 -- Unknown
				);
			}
			fclose(OBJECT_ARMOR);
		}

	// CRITTER
		{
		// Seek to target data set
			fseek(fd, 0x0132, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_CRITTER.csv", OutPath.c_str());
			FILE* OBJECT_CRITTER = fopen(TempPath, "w");
			fprintf(OBJECT_CRITTER, "ItemID,Name,BaseArmor,Armor1,Armor2,Armor3,HP,STR,DEX,INT,DeathVOC,BloodOnHit,BloodOnDeath,Race,x0Ab0,x0Ab1,Corpse,x0Ab5,Swim,Fly,x0B,MoveSpeed,ConvLevel,TradeAbility,TradeThreshold,TradePatience,PoisonDamage,Category,Attack,Defense,SlashSkill,SlashDamage,SlashChance,BashSkill,BashDamage,BashChance,StabSkill,StabDamage,StabChance,x1C,SearchDistance,Ears,Eyes,x1F,DropItem1Name,DropItem1Spawn,DropItem2Name,DropItem2Spawn,DropItem3Name,DropItem3Chance,DropItem4Name,DropItem4Chance,DropCoinCalc,DropCoinChance,DropFoodName,DropFoodChance,EXP,Spell1,Spell2,Spell3,IsCaster,StudyMonster,LockPicking,x2F\n");

			for (int i = 0x40; i < 0x80; i++) {
				unsigned char buffer[48];
				fread(buffer, 1, 48, fd);

			/***  Pulling this, pretty sure it's just a true/false on showing blood on hit
				// Translate blood type
				const char* BloodType;
				switch ((buffer[8] & 0x18) >> 3) {
					case 0: BloodType = "Dust"; break;  // Not sure if dust is right, kinda a small explosion star but whatever
					case 1: BloodType = "Blood"; break;
					default: BloodType = "Unknown"; break;
				}
			***/

			// Get critter's item name
				std::string DisplayName = CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str();

			// Hardcode missing critter names
				if (DisplayName == "") {
					if (!IsUW2) {
						if (i == 123) {
							DisplayName = "Tyball";
						}
						else if (i == 124) {
							DisplayName = "Slasher of Veils";
						}
						else {
							DisplayName = "Ethereal Void Creature";
						}
					}
					else {   // Only EV creatures don't have a name in 2
						DisplayName = "Ethereal Void Creature";
					}
				}

			// Hardcode critter's remains item -- Note: Val + 217 maps to ItemID but hardcoded so Red/Green Blood Stain can be called out (itemname for both is just Blood Stain)
				const char* Remains;
				switch (buffer[8] >> 5) {
					case  0:	Remains = "Nothing"; break;
					case  2:	Remains = "Rubble"; break;
					case  3:	Remains = "Pile of Wood Chips"; break;
					case  4:	Remains = "Pile of Bones"; break;
					case  5:	Remains = "Blood Stain (Green)"; break;
					case  6:	Remains = "Blood Stain (Red)"; break;
					default:	Remains = "Unknown"; break;
				}

			// Translate critter category type
				const char* Category;
				switch (buffer[16]) {
					case  0:	Category = "Ethereal"; break;
					case  1:	Category = "Humanoid"; break;
					case  2:	Category = "Flying"; break;
					case  3:	Category = "Swimming"; break;
					case  4:	Category = "Creeping"; break;
					case  5:	Category = "Crawling"; break;
					case 17:	Category = "Golem"; break;
					case 81:	Category = "Human"; break;
					default:	Category = "Unknown"; break;
				}

			// Get corpse offset
				unsigned int CorpseOffset = 192;
				if ((buffer[10] & 0x1C) >> 2 == 0) {
					CorpseOffset += 319; // Push to end of list so blank if none
				}

			// Get loot
				std::string Item1Name = "";
				if (buffer[32] > 0x00 && buffer[32] < 0xFF) {
					Item1Name = CleanDisplayName(gs.get_string(4, ((buffer[32] >> 1) & 0x0F) + ((buffer[32] >> 5))).c_str(), true, false);
				}
				std::string Item2Name = "";
				if (buffer[33] > 0x00 && buffer[33] < 0xFF) {
					Item2Name = CleanDisplayName(gs.get_string(4, ((buffer[33] >> 1) & 0x0F) + ((buffer[33] >> 5))).c_str(), true, false);
				}
				std::string Item3Name = "";
				if ((buffer[34] | (buffer[35] << 8)) != 0x00) {
					Item3Name = CleanDisplayName(gs.get_string(4, ((buffer[34] | (buffer[35] << 8)) >> 4)).c_str(), true, false);
				}
				std::string Item4Name = "";
				if ((buffer[36] | (buffer[37] << 8)) != 0x00) {
					Item4Name = CleanDisplayName(gs.get_string(4, ((buffer[36] | (buffer[37] << 8)) >> 4)).c_str(), true, false);
				}
				std::string FoodName = "";
				if (buffer[39] > 0x00 && buffer[32] < 0xFF) {
					FoodName = CleanDisplayName(gs.get_string(4, (buffer[39] >> 4) + 0xB0).c_str(), true, false);
				}

			// Get spell offset in string table 6
			// Spell1
				unsigned int Spell1Offset = 256;
			  // Push no value to last string value so blank
				if (buffer[42] == 0x00) {
					Spell1Offset = 511;
				}
			  // Yeti's snowball is off by 1?
				else if (buffer[42] == 0x49) {
					Spell1Offset = 255;
				}
			  // Following same logic as enchantments to guess this, appears to be correct (except for snowball above)
				else if (buffer[42] > 0x3F && buffer[42] < 0xFF) {
					Spell1Offset = 144;
				}

			// Spell2
				unsigned int Spell2Offset = 256;
			  // Push no value to last string value so blank
				if (buffer[43] == 0x00) {
					Spell2Offset = 511;
				}
			  // Yeti's snowball is off by 1?
				else if (buffer[43] == 0x49) {
					Spell2Offset = 255;
				}
			  // Following same logic as enchantments to guess this, appears to be correct (except for snowball above)
				else if (buffer[43] > 0x3F && buffer[43] < 0xFF) {
					Spell2Offset = 144;
				}

			// Spell3
				unsigned int Spell3Offset = 256;
			  // Push no value to last string value so blank
				if (buffer[44] == 0x00) {
					Spell3Offset = 511;
				}
			  // Yeti's snowball is off by 1?
				else if (buffer[44] == 0x49) {
					Spell3Offset = 255;
				}
			  // Following same logic as enchantments to guess this, appears to be correct (except for snowball above)
				else if (buffer[44] > 0x3F && buffer[44] < 0xFF) {
					Spell3Offset = 144;
				}
				/***
					Note:  UW2 Liche Wizard
					In the strategy guide, this creature type is listed with 7 spells; however, only the 3 above are available for any 1 critter
					After poking at this a bit to figure out how that worked, I'm reasonably sure that it really only has the 3 linked here (Fireball/Flame Wind/Heal)

					The missing ones are explained/spoofed by:
						Iron Flesh:	Has very high base defense for wizard type
						Flameproof:	ResistFire flag set in COMOBJ
						Fly:		Mistake in the guide and is meant for Liche Assassin who has the Fly flag set (they _did_ make several other errors)
						Open:		Door/Lockpicking skill is set at a very high value (highest of all critters) so it's really just picking it

					Additionally, the guide notes:  "The wizard liche's high armor score reflects the ability to cast Iron Flesh."
					That could be read as they were inflating the number in their chart to show what it would be when they cast it
					however, it _is_ set that high which indicates it's always on/doesn't need cast (and probably shouldn't be as it'd be ludicrously high (13))

					I could be wrong but I doubt they'd bother hardcoding in additional spells and the AI logic to cast them
					for 1 critter if they've already covered those spells' effects by other means (and/or messed up the guide)
				***/

			// Export to CSV
				fprintf(
					OBJECT_CRITTER,
					"%u,"		// ItemID
					"%s,"		// Name
					"%u,"		// BaseArmor
					"%s,"		// Armor1
					"%s,"		// Armor2
					"%s,"		// Armor3
					"%u,"		// HP
					"%u,"		// STR
					"%u,"		// DEX
					"%u,"		// INT
					"%u,"		// DeathVOC
					"%u,"		// BloodOnHit
					"%s,"		// BloodOnDeath
					"%s,"		// Race
					"%u,"		// x0Ab0 -- Unknown
					"%u,"		// x0Ab1 -- Unknown
					"%s,"		// Corpse
					"%u,"		// x0Ab5 -- Unknown
					"%u,"		// Swim
					"%u,"		// Fly
					"%02X,"		// x0B -- Unknown
					"%u,"		// MoveSpeed
					"%u,"		// ConvLevel
					"%u,"		// TradeAbility
					"%u,"		// TradeThreshold
					"%u,"		// TradePatience
					"%u,"		// PoisonDamage
					"%s,"		// Category
					"%u,"		// Attack
					"%u,"		// Defense
					"%u,"		// SlashSkill
					"%u,"		// SlashDamage
					"%u,"		// SlashChance
					"%u,"		// BashSkill
					"%u,"		// BashDamage
					"%u,"		// BashChance
					"%u,"		// StabSkill
					"%u,"		// StabDamage
					"%u,"		// StabChance
					"%02X,"		// x1C -- Unknown
					"%u,"		// SearchDistance
					"%u,"		// Ears
					"%u,"		// Eyes
					"%02X,"		// x1F -- Unknown
					"%s,"		// DropItem1Name
					"%u,"		// DropItem1Spawn
					"%s,"		// DropItem2Name
					"%u,"		// DropItem2Spawn
					"%s,"		// DropItem3Name
					"%u,"		// DropItem3Chance
					"%s,"		// DropItem4Name
					"%u,"		// DropItem4Chance
					"%u,"		// DropCoinCalc
					"%u,"		// DropCoinChance
					"%s,"		// DropFoodName
					"%u,"		// DropFoodChance
					"%u,"		// EXP
					"%s,"		// Spell1
					"%s,"		// Spell2
					"%s,"		// Spell3
					"%u,"		// IsCaster
					"%u,"		// StudyMonster
					"%u,"		// LockPicking
					"%02X\n",	// x2F -- Unknown
					i,								// ItemID
					DisplayName.c_str(),			// Name
					buffer[0],						// BaseArmor
					buffer[1] == 255 ? "" : std::to_string(buffer[1]).c_str(),	// Armor1 -- Not sure if 1 is hand/legs/both or other so leaving it and 2/3 with generic name
					buffer[2] == 255 ? "" : std::to_string(buffer[2]).c_str(),	// Armor2 -- Pretty sure this is body
					buffer[3] == 255 ? "" : std::to_string(buffer[3]).c_str(),	// Armor3 -- Pretty sure this is head
					buffer[4],						// HP
					buffer[5],						// STR
					buffer[6],						// DEX
					buffer[7],						// INT
					buffer[8] & 0x07,				// DeathVOC
					(buffer[8] & 0x18) >> 3,		// BloodOnHit
					Remains,						// BloodOnDeath (8 >> 5)
					CleanDisplayName(gs.get_string(1, buffer[9] + 370).c_str(), true, false).c_str(),	// Race
					//buffer[10],					// Passiveness -- Nope
					buffer[10] & 0x01,				// x0Ab0 -- Unknown
					(buffer[10] & 0x02) >> 1,		// x0Ab1 -- Unknown -- Only set on UW1 for Ethereal Void creatures, Wisp, and Player
					CleanDisplayName(gs.get_string(4, ((buffer[10] & 0x1C) >> 2) + CorpseOffset).c_str(), true, false).c_str(),	// Corpse
					(buffer[10] & 0x20) >> 5,		// x0Ab5 -- Unknown
					(buffer[10] & 0x40) >> 6,		// Swim
					(buffer[10] & 0x80) >> 7,		// Fly
					buffer[11],						// x0B -- Unknown
					buffer[12],						// MoveSpeed
					buffer[13] & 0x0F,				// ConvLevel
					buffer[13] >> 4,				// TradeAbility
					buffer[14] & 0x0F,				// TradeThreshold
					buffer[14] >> 4,				// TradePatience
					buffer[15],						// PoisonDamage
					Category,						// Category (16)
					buffer[17],						// Attack
					buffer[18],						// Defense
					buffer[19],						// SlashSkill
					buffer[20],						// SlashDamage
					buffer[21],						// SlashChance
					buffer[22],						// BashSkill
					buffer[23],						// BashDamage
					buffer[24],						// BashChance
					buffer[25],						// StabSkill
					buffer[26],						// StabDamage
					buffer[27],						// StabChance
					buffer[28],						// x1C -- Unknown
					buffer[29],						// SearchDistance
					(buffer[30] & 0x0F) << 3,		// Ears
					(buffer[30] & 0xF0) >> 1,		// Eyes
					buffer[31],						// x1F -- Unknown
					Item1Name.c_str(),				// DropItem1Name
					buffer[32] & 0x01,				// DropItem1Spawn
					Item2Name.c_str(),				// DropItem2Name
					buffer[33] & 0x01,				// DropItem2Spawn
					Item3Name.c_str(),				// DropItem3Name
					buffer[34] & 0x0F,				// DropItem3Chance
					Item4Name.c_str(),				// DropItem4Name
					buffer[36] & 0x0F,				// DropItem4Chance
					buffer[38] & 0x0F,				// DropCoinCalc
					buffer[38] >> 4,				// DropCoinChance
					FoodName.c_str(),				// DropFoodName
					buffer[39] & 0x0F,				// DropFoodChance
					buffer[40] | (buffer[41] << 8),	// EXP
					CleanDisplayName(gs.get_string(6, buffer[42] + Spell1Offset).c_str(), true, false).c_str(),	// Spell1
					CleanDisplayName(gs.get_string(6, buffer[43] + Spell2Offset).c_str(), true, false).c_str(),	// Spell2
					CleanDisplayName(gs.get_string(6, buffer[44] + Spell3Offset).c_str(), true, false).c_str(),	// Spell3
					buffer[45] & 0x01,				// IsCaster
					buffer[45] >> 1,				// StudyMonster -- Need to look into this, not sure I buy this as it's populated in UW1
					buffer[46],						// LockPicking
					buffer[47]						// x2F -- Unknown
				);
			}
			fclose(OBJECT_CRITTER);
		}

	// CONTAINER
		{
		// Seek to target data set
			fseek(fd, 0x0D32, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_CONTAINER.csv", OutPath.c_str());
			FILE* OBJECT_CONTAINER = fopen(TempPath, "w");
			fprintf(OBJECT_CONTAINER, "ItemID,Name,Capacity,AcceptID,AcceptName,Slots\n");

			unsigned char buffer[3];
			for (int i = 0x80; i < 0x90; i++) {
				fread(buffer, 1, 3, fd);

			// Translate type of items container accepts
				const char* AcceptName;
				switch (buffer[1]) {
					case   0:	AcceptName = "Runes"; break;
					case   1:	AcceptName = "Arrows"; break;
					case   2:	AcceptName = "Scrolls"; break;
					case   3:	AcceptName = "Edibles"; break;
					case   4:	AcceptName = "Keys"; break;
					case 255:	AcceptName = "Any"; break;
					default:	AcceptName = "Unknown"; break;
				}

			// Export to CSV
				fprintf(
					OBJECT_CONTAINER,
					"%u,"	// ItemID
					"%s,"	// Name
					"%.1f,"	// Capacity
					"%u,"	// AcceptID
					"%s,"	// AcceptName
					"%u\n",	// Slots
					i,					// ItemID
					CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str(),	// Name
					buffer[0] * 0.1,	// Capacity
					buffer[1],			// AcceptID
					AcceptName,			// AcceptName
					buffer[2]			// Slots
				);
			}
			fclose(OBJECT_CONTAINER);
		}

	// LIGHTING
		{
		// Seek to target data set
			fseek(fd, 0x0D62, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_LIGHTING.csv", OutPath.c_str());
			FILE* OBJECT_LIGHTING = fopen(TempPath, "w");
			fprintf(OBJECT_LIGHTING, "ItemID,Name,Brightness,Duration\n");

			for (int i = 0x90; i < 0xA0; i++) {
			// Export to CSV
				fprintf(
					OBJECT_LIGHTING,
					"%u,"	// ItemID
					"%s,"	// Name
					"%u,"	// Brightness
					"%u\n",	// Duration
					i,			// ItemID
					CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str(),	// Name
					fgetc(fd),	// Brightness
					fgetc(fd)	// Duration
				);
			}
			fclose(OBJECT_LIGHTING);
		}

	// FOOD
		{
		// Seek to target data set
			fseek(fd, 0x0D82, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_FOOD.csv", OutPath.c_str());
			FILE* OBJECT_FOOD = fopen(TempPath, "w");
			fprintf(OBJECT_FOOD, "ItemID,Name,Nutrition\n");

			for (int i = 0xB0; i < 0xC0; i++) {
			// Export to CSV
				fprintf(
					OBJECT_FOOD,
					"%u,"	// ItemID
					"%s,"	// Name
					"%u\n",	// Nutrition
					i,			// ItemID
					CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str(),	// Name
					fgetc(fd)	// Nutrition
				);
			}
			fclose(OBJECT_FOOD);
		}

	// TRIGGER
		{
		// Seek to target data set
			fseek(fd, 0x0D92, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_TRIGGER.csv", OutPath.c_str());
			FILE* OBJECT_TRIGGER = fopen(TempPath, "w");
			fprintf(OBJECT_TRIGGER, "ItemID,Name,Mode\n");

			for (int i = 0x01A0; i < 0x1B0; i++) {
			// Export to CSV
				fprintf(
					OBJECT_TRIGGER,
					"%u,"	// ItemID
					"%s,"	// Name
					"%u\n",	// Mode
					i,			// ItemID
					CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str(),	// Name
					fgetc(fd)	// Mode
				);
			}
			fclose(OBJECT_TRIGGER);
		}

	// ANIMATION
		{
		// Seek to target data set
			fseek(fd, 0x0DA2, SEEK_SET);

		// Create CSV export file and header
			sprintf(TempPath, "%s\\OBJECTS_ANIMATION.csv", OutPath.c_str());
			FILE* OBJECT_ANIMATION = fopen(TempPath, "w");
			fprintf(OBJECT_ANIMATION, "ItemID,Name,x00,StartFrame,FrameCount\n"); // Not bothering with 01 as it's always 00 (probably a u16 with 00)

			unsigned char buffer[4];
			for (int i = 0x01C0; i < 0x01D0; i++) {
				fread(buffer, 1, 4, fd);

			// Export to CSV
				fprintf(
					OBJECT_ANIMATION,
					"%u,"		// ItemID
					"%s,"		// Name
					"%02X,"		// x00 -- Unknown
					"%u,"		// StartFrame
					"%u\n",		// FrameCount
					i,				// ItemID
					CleanDisplayName(gs.get_string(4, i).c_str(), true, false).c_str(),	// Name
					buffer[0],		// x00 -- Unknown
					buffer[2],		// StartFrame
					buffer[3]		// FrameCount
				);
			}
			fclose(OBJECT_ANIMATION);
		}
		fclose(fd);
	}

// SHADES.DAT
	{
		sprintf(TempPath, "%s\\DATA\\SHADES.DAT", UWPath.c_str());
		FILE* fd = fopen(TempPath, "rb");

	// Create CSV export file and header
		sprintf(TempPath, "%s\\SHADES.csv", OutPath.c_str());
		FILE* out = fopen(TempPath, "w");
		fprintf(out, "Index,LightLevel,NearOffset,PalIndex,Step,ViewDistance,TextureDistance,x0A\n");

	// Get shade count (should be 8 but doc notes up to 16 is supported so we'll check)
		fseek(fd, 0, SEEK_END);
		int ShadeCount = ftell(fd) / 12;
		fseek(fd, 0, SEEK_SET);

		unsigned char ShadeBuffer[12];

		for (int s = 0; s < ShadeCount; s++) {
			fread(ShadeBuffer, 1, 12, fd);

		// Export to CSV
			fprintf(
				out,
				"%u,"		// Index
				"%s,"		// LightLevel -- Assuming this matches to the light level
				"%u,"		// NearOffset	-- This and bytes 1-3 are documented as u8 but looks to me like they're u16 so treating as such
				"%u,"		// PalIndex -- Believe this is the index to the palette in LIGHT.DAT
				"%04X,"		// Step -- Documented as this but the values don't make sense to me, leaving as hex since int looks very obviously wrong
				"%u,"		// ViewDistance	-- Tiles until full dark
				"%u,"		// TextureDistance -- Distance until flat texturing (not sure how it measures that?)
				"%04X\n",	// x0A -- Unknown - Maybe Z distance?
				s,											// Index
				gs.get_string(6, s).c_str(),				// LightLevel -- Using the magic spell light description, though they don't really match well with things like candles (Burning Match), torches (Candlelight), etc
				ShadeBuffer[0] | (ShadeBuffer[1] << 8),		// NearOffset
				ShadeBuffer[2] | (ShadeBuffer[3] << 8),		// PalIndex
				ShadeBuffer[4] | (ShadeBuffer[4] << 8),		// Step
				ShadeBuffer[6] | (ShadeBuffer[7] << 8),		// ViewDistance
				ShadeBuffer[8] | (ShadeBuffer[9] << 8),		// TextureDistance
				ShadeBuffer[10] | (ShadeBuffer[11] << 8)	// x0A -- Unknown
			);
		}
		fclose(fd);
		fclose(out);
	}

// SKILLS.DAT
	{
	// Contains two datasets, one for class base attributes and one for starting skills, so exported as two files
		sprintf(TempPath, "%s\\DATA\\SKILLS.DAT", UWPath.c_str());
		FILE* fd = fopen(TempPath, "rb");

	// ATTRIBUTE
		{
		/***
			This wasn't documented so thought I'd add my observations, mostly from messing with UW1:
			Class attributes are controlled by the initial 0x20 (32) bytes, with 4 bytes per class ordered by the class' ID number
			00:	Base STR
			01: Base DEX
			02: Base INT
			03: Additional point pool

			Additional points are divied out based on some algorithm that uses the number of questions answered as a modifier or seed
			i.e. ESCing back to break early instead of completing the character and selecting not to keep it alters the results

			Base attributes can be between 0-255; however, the logic that assigns the additional points from the pool will cap
			each attribute at 30.  Interestingly, it does _not_ just set it back to 30, it removes the points that are over
			the limit and adds them back to the pool to redistribute to other attributes *but* will only do this if whatever logic it uses
			tried to change the invalid value.
			Example:
			Fighter set to 00 20 00 01
			Normally with a bonus pool of 1 (for fighters at least), it will set the point to INT on the first pass and DEX on the second
			First pass:
			STR:  0
			DEX:  32
			INT:  1
			Second pass:
			STR:  3
			DEX:  30
			INT:  0

			This _will_ lock the game if it ends up in a position where it can't find an attribute it can distribute (or redistribute) the points to
			making the max total between all 4 values 90.  But, this is only if it has to distribute points.
			A setting of FF FF FF 00 will work to set all three to 255
			But a setting of 00 00 00 5B or 01 00 00 5A will lock the game as it tries and fails to divy out 91 points

			Skills follow somewhat similar logic, at least so far as they are adjusted based on the number of questions answered
			They have a range of -1 to 30 (they disappear from the list if < 1).  Unlike attributes, over 30 is lost.

			The value for each appears to have a question count based modifier that (from my observations) range from -1 to 4
			which is altered by the pool value (not specifically increased, just changed or shifted) and maybe the base values as well.

			That modifier is combined (not sure if straight addition as the modifier changes based on pool (and maybe base)) with a value derived
			from the skill's primary attribute (i.e. STR affects Attack, DEX affects Acrobat, etc) that looks to be ~20%.

			UW2 works similarly; however, it no longer uses the number of questions answered as a modifier/seed and appears to just use
			a random function for both attributes and skills
		***/
		// Create CSV export file and header
			sprintf(TempPath, "%s\\SKILLS_ATTRIBUTE.csv", OutPath.c_str());
			FILE* out = fopen(TempPath, "w");
			fprintf(out, "ClassID,ClassName,STR,DEX,INT,Pool\n");

			// Loop each class
			for (unsigned int c = 0; c < 8; c++) {
				unsigned char AttributeBuffer[4];
				fread(AttributeBuffer, 1, 4, fd);

			// Export to CSV
				fprintf(
					out,
					"%u,"	// ClassID
					"%s,"	// ClassName
					"%u,"	// STR
					"%u,"	// DEX
					"%u,"	// INT
					"%u\n",	// Pool
					c,									// ClassID
					gs.get_string(2, c + 23).c_str(),	// ClassName
					AttributeBuffer[0],					// STR
					AttributeBuffer[1],					// DEX
					AttributeBuffer[2],					// INT
					AttributeBuffer[3]					// Pool
				);

			}
			fclose(out);
		}

	// GROUP
		{
		// Create CSV export file and header
			sprintf(TempPath, "%s\\SKILLS_GROUP.csv", OutPath.c_str());
			FILE* out = fopen(TempPath, "w");
			fprintf(out, "ClassID,ClassName,Group,Index,Autoset,SkillID,SkillName\n");

		// Loop each class
			for (unsigned int c = 0; c < 8; c++) {
			// Loop each group
				for (unsigned int g = 0; g < 5; g++) {
				// Get group skill count
					unsigned int SkillCount = fgetc(fd);

				// Loop group skills
					for (unsigned int s = 0; s < SkillCount; s++) {
					// Get skill ID
						unsigned int SkillID = fgetc(fd);

					// Export to CSV
						fprintf(
							out,
							"%u,"	// ClassID
							"%s,"	// ClassName
							"%u,"	// Group
							"%u,"	// Index
							"%u,"	// Autoset
							"%u,"	// SkillID
							"%s\n",	// SkillName
							c,										// ClassID
							gs.get_string(2, c + 23).c_str(),		// ClassName
							g,										// Group
							s,										// Index
							SkillCount == 1 ? 1 : 0,				// Autoset
							SkillID,								// SkillID
							gs.get_string(2, SkillID + 31).c_str()	// SkillName
						);
					}
				}
			}
			fclose(out);
		}
		fclose(fd);
	}

// SOUNDS.DAT
	{
	/***
		Note:  Bytes x03/04
			I'm honestly not sure internally how it is handling bytes x03/4.  They definitely control how long before a note off message is sent
			after a note is played but if that is tracked in seconds/milliseconds, full quarter notes/partial notes (ticks), etc, I have no idea.
			It's worth noting that (on the MT32) until that note off message is sent, while the note will not actually play, it will also not release
			the partial (note) on the channel it played on (each channel has 4 partials/notes it can play) which will eventually cause you to run out
			of available channels for sound effects (UW1/2 appear to reserve a partial on 4 channels for SEs (the remaining 28 for music) and will
			not play any SEs if all 4 are locked up).

			UW1/2 doesn't appear to set a tempo anywhere and is defaulting to 500 ticks per quarter which equates to 1000 microseconds per tick
			at 120 bpm or 1 millisecond, making each quarter note .5 seconds long.

			From testing byte x03 _appears_ to equal the number of full seconds before the note off message is sent, making a value of 01 equate to
			2 quarter notes or 1000 ticks.  Byte x04 appears to be milliseconds (val * (1000/256)), with the two values that are seen (x80/x40)
			equating to 500 and 250 milliseconds, or 1/0.5 quarter notes or 500/250 ticks.

			I'll note that there is a fair amount of variability, with any of these values tending to be somewhere between 16-72 milliseconds/ticks
			less than the values I'm guessing them to be in the midi logs.  Not sure if latency and, if so, if that's a midi, DosBox, or UW thing.
			This variance is not from sound to sound (though they _will_ be different), but the same sound played once then again will have some
			small difference in the number of ticks before the note off is sent.  Again, no idea, but assuming some latency being adjusted for.

			While time could be used for the returned value (i.e. 0.5 seconds) I thought that would be more likely to be misread as how long the note
			should play for, which it is not, so opted for ticks to avoid confusion or at least confuse in a different direction.  Also because I spent
			a stupid amount of time researching midi crap to try to figure out what was going on and I may as well toss some of that in.

			Additionally, while looking into this I learned that timbre is not pronounced like timber but more like tamborine (taembor).
			An interesting fact that reconfirms for us all that those involved in music are incredibly pretentious.
			Also, my work here does not qualify as being involved in music, I'm better than that... wait?
	***/
		sprintf(TempPath, "%s\\SOUND\\SOUNDS.DAT", UWPath.c_str());
		FILE* fd = fopen(TempPath, "rb");

	// Read in MT32 sysex file to extract sound names (only partial in UW2)
		sprintf(TempPath, "%s\\SOUND\\UW.MT", UWPath.c_str());
		FILE* SysEx = fopen(TempPath, "rb");

	// Get sound effect names
		std::vector<std::string> SoundEffect(28);
		fseek(SysEx, 388, SEEK_SET);

		for (int i = 0; i < 28; i++) {
			char TimbreName[10];
			fread(&TimbreName, 10, 1, SysEx);

		// Stupid name trim fix
			std::string CleanName = TimbreName;
			CleanName = CleanName.substr(0, 10);
			SoundEffect[i] = CleanName.substr(0, 10 - (10 - CleanName.find_last_not_of(" ")) +1);

		// Jump to next effect
			fseek(SysEx, 238, SEEK_CUR);
		}

		fclose(SysEx);

	// Get record count
		int RecordCount = fgetc(fd);

	// Create CSV export file and header
		sprintf(TempPath, "%s\\SOUNDS.csv", OutPath.c_str());
		FILE* out = fopen(TempPath, "w");

	// File format differs between UW1/2 and easier to split
		if (!IsUW2) {
			fprintf(out, "EffectID,TimbreID,TimbreName,Note,Velocity,Ticks\n");	// TimbreID might not be best name as UW2 has effects without a timbre in its file (I assume those are for either AdLib or SoundBlaster) but not sure what else to call it

			for (int i = 0; i < RecordCount; i++) {
				unsigned char SoundData[5];
				fread(SoundData, 1, 5, fd);

			// Export to CSV
				fprintf(
					out,
					"%u,"		// EffectID
					"%u,"		// TimbreID
					"%s,"		// TimbreName
					"%u,"		// Note
					"%u,"		// Velocity
					"%1.f\n",	// Ticks
					i,										// EffectID
					SoundData[0],							// TimbreID
					SoundEffect[SoundData[0]].c_str(),		// TimbreName
					SoundData[1],							// Note
					SoundData[2],							// Velocity
					(SoundData[3] * 1000) + (SoundData[4] * (static_cast<float>(1000) / 256))	// Ticks
				);
			}
		}
		else {
			fprintf(out, "EffectID,TimbreID,TimbreName,Note,Velocity,Ticks,HasVOC,x06,x07\n");	// TimbreID might not be best name as UW2 has effects without a timbre in its file (I assume those are for either AdLib or SoundBlaster) but not sure what else to call it

			for (int i = 0; i < RecordCount; i++) {
				unsigned char SoundData[8];
				fread(SoundData, 1, 8, fd);

			// UW2 has additional effects that are not includedfdhjsalsfda
				std::string TimbreName = "";
				if (SoundData[0] < 28) {
					TimbreName = SoundEffect[SoundData[0]];
				}

			// Export to CSV
				fprintf(
					out,
					"%u,"		// EffectID
					"%u,"		// TimbreID
					"%s,"		// TimbreName
					"%u,"		// Note
					"%u,"		// Velocity
					"%1.f,"		// Ticks
					"%u,"		// HasVOC
					"%02X,"		// x06
					"%02X\n",	// x07
					i,											// EffectID
					SoundData[0],								// TimbreID
					TimbreName.c_str(),							// TimbreName
					SoundData[1],								// Note
					SoundData[2],								// Velocity
					(SoundData[3] * 1000) + (SoundData[4] * (static_cast<float>(1000) / 256)),	// Ticks
					SoundData[5],								// HasVOC
					SoundData[6],								// x06
					SoundData[7]								// x07
				);
			}
		}

		/***
			Note: Bytes x06/07
				Mixing up the values or throwing in ones that are far out of range (80, FF, etc) gives no noticeable affect for any sound card type.
				There is clearly a logic to them as values are generally grouped up with similar sounds or with the same Timbre.

				Maybe one of them works as a grouping to do some sort of prioritization to drop sounds if too much is going on at the same time?

				No idea
		***/

		fclose(fd);
		fclose(out);
	}

// TERRAIN.DAT
	{
		sprintf(TempPath, "%s\\DATA\\TERRAIN.DAT", UWPath.c_str());
		FILE* fd = fopen(TempPath, "rb");

	// Create CSV export file and header
		sprintf(TempPath, "%s\\TERRAIN.csv", OutPath.c_str());
		FILE* out = fopen(TempPath, "w");
	// Terrain is handled differently between UW1 & 2
		if (!IsUW2) {
		// UW1 -- Floor & Wall terrains are segregated so include indicator flag in export
			fprintf(out, "TerrainID,IsFloor,Type\n");
		}
		else {
		// UW2 -- Terrain is merged and can be used for either though _tend_ to be used primarily for one type with some being exclusive (i.e. fireplace is always a wall, etc.)
			fprintf(out, "TerrainID,Type\n");
		}

		fseek(fd, 0, SEEK_END);
		int TerrainCount = ftell(fd) / 2;
		fseek(fd, 0, SEEK_SET);

		unsigned short int* TerrainBuffer = new unsigned short int[TerrainCount];
		fread(TerrainBuffer, sizeof(short int), TerrainCount, fd);

		for (int i = 0; i < TerrainCount; i++) {
			const char* TerrainType;
			switch (TerrainBuffer[i]) {
				case 0x00:	TerrainType = "Normal"; break;
				case 0x02:	TerrainType = "AnkhMural_Shrine"; break;
				case 0x03:	TerrainType = "StairUp"; break;
				case 0x04:	TerrainType = "StairDown"; break;
				case 0x05:	TerrainType = "Pipe"; break;
				case 0x06:	TerrainType = "Grate"; break;
				case 0x07:	TerrainType = "Drain"; break;
				case 0x08:	TerrainType = "ChainedPrincess"; break;
				case 0x09:	TerrainType = "Window"; break;
				case 0x0A:	TerrainType = "Tapestry"; break;
				case 0x0B:	TerrainType = "TexturedDoor_KoI"; break;
				case 0x10:	TerrainType = "Water"; break;
				case 0x20:	TerrainType = "Lava"; break;
				case 0x40:	TerrainType = "Waterfall"; break;
				case 0x48:	TerrainType = "WaterFlowSouth"; break;
				case 0x50:	TerrainType = "WaterFlowNorth"; break;
				case 0x58:	TerrainType = "WaterFlowWest"; break;
				case 0x60:	TerrainType = "WaterFlowEast"; break;
				case 0x80:	TerrainType = "Lavafall"; break;
				case 0xC0:	TerrainType = "IceWall"; break;
				case 0xC8:	TerrainType = "IceHole"; break;
				case 0xD8:	TerrainType = "IceWallGap"; break;
				case 0xE8:	TerrainType = "IceWallCrack"; break;
				case 0xF8:	TerrainType = "IceNonSlip"; break;
				default:	TerrainType = "Unknown"; break;
			}

		// Export to CSV -- Change export depending on game
			if (!IsUW2) {
				fprintf(
					out,
					"%u,"	// TerrainID
					"%u,"	// IsFloor
					"%s\n",	// TerrainType
					i > 255 ? i - 256 : i,	// TerrainID -- Renumber the floor tiles starting at 256
					i > 255 ? 1 : 0,		// IsFloor
					TerrainType				// TerrainType
				);
			}
			else {
				fprintf(
					out,
					"%u,"	// TerrainID
					"%s\n",	// TerrainType
					i,			// TerrainID
					TerrainType	// TerrainType
				);
			}
		}
		fclose(fd);
		fclose(out);
	}

// WEAPONS/WEAP.DAT
	{
	/***
		UW2 file has a 35 byte long header, guessing byte 0 (1F) is telling how many frames per type (31)
		But have no idea what the rest are don't really care to figure it out :P

		I did notice a bit of a pattern with two sequential values then a jump of 0x21 (33) (i.e. 21 22 44 / 64 65 86)
		Probably meaningful but, again, meh

		Additionally, there are 245 extra bytes at the end which is room for 3 more weapons and nearly enough for a 4th
		Possibly 1 byte with additional info for each frame, however, it's 3 bytes short for that
		and no pattern really stood out when lining it up with the frames

		Honestly have no clue why I spent this much time on this
	***/
		unsigned short int FrameCount = !IsUW2 ? 28 : 31;
		unsigned short int StartOffset = !IsUW2 ? 0 : 35;
		std::string WEAPFileName = !IsUW2 ? "WEAPONS" : "WEAP";

	// Open WEAPONS/WEAP.DAT file twice as it is far easier to just offset the Y file to get X/Y paired up
		sprintf(TempPath, "%s\\DATA\\%s.DAT", UWPath.c_str(), WEAPFileName.c_str());
		FILE* fdx = fopen(TempPath, "rb");
		FILE* fdy = fopen(TempPath, "rb");

	// Seek to starting X/Y offsets
		fseek(fdx, StartOffset, SEEK_SET);
		fseek(fdy, StartOffset + FrameCount, SEEK_SET);

	// Read in GR file to identify which frames actually have an image
		sprintf(TempPath, "%s\\DATA\\%s.GR", UWPath.c_str(), WEAPFileName.c_str());
		FILE* GRFile = fopen(TempPath, "rb");

		fseek(GRFile, 0, SEEK_END);
		unsigned int GRLen = ftell(GRFile);
		fseek(GRFile, 1, SEEK_SET);

		unsigned short ImageCount;
		fread(&ImageCount, 1, 2, GRFile);

		unsigned int* GROffset = new unsigned int[ImageCount];
		fread(GROffset, sizeof(unsigned int), ImageCount, GRFile);

	// Create CSV export file and header
		sprintf(TempPath, "%s\\%s.csv", OutPath.c_str(), WEAPFileName.c_str());
		FILE* out = fopen(TempPath, "w");
		fprintf(out, "Weapon,Hand,Type,FrameNumber,ImageIndex,OffsetX,OffsetY\n");

		for (unsigned int j = 0; j < 8; j++) {
		// File is 2 blocks (Right/Left) of 4 weapon types (Sword/Axe/Mace/Unarmed)
			char* WeaponName;
			char* Hand;
			switch (j) {
				case  0:	WeaponName = "Sword"; Hand = "Right"; break;
				case  1:	WeaponName = "Axe"; Hand = "Right"; break;
				case  2:	WeaponName = "Mace"; Hand = "Right"; break;
				case  3:	WeaponName = "Unarmed"; Hand = "Right"; break;	// Originally had this as "Fist" but decided it was better to match the skill name
				case  4:	WeaponName = "Sword"; Hand = "Left"; break;
				case  5:	WeaponName = "Axe"; Hand = "Left"; break;
				case  6:	WeaponName = "Mace"; Hand = "Left"; break;
				case  7:	WeaponName = "Unarmed"; Hand = "Left"; break;
				default:	WeaponName = "Unknown"; Hand = "Unknown"; break;
			}

			for (int i = 0; i < FrameCount; i++) {
			// Get frame image index - Index is always the position in the file, really just checking if an image exists
				std::string ImageIndex = "";
				if (GROffset[(j * FrameCount) + i] < GRLen && GROffset[(j * FrameCount) + i] != GROffset[(j * FrameCount) + i + 1]) {
					ImageIndex = std::to_string((j * FrameCount) + i);
				}

			// Get frame type
				char* Type;
			// Identify unused frames
				if (ImageIndex == "") {
					Type = "Unused";
				}
			// UW1 All Types & UW2 Weapons
				else if (!IsUW2 || (j != 3 && j != 7)) {
					if (i == 27) {
						Type = "Ready";
					}
					else if (i < 9) {
						Type = "Slash";
					}
					else if (i < 18) {
						Type = "Bash";
					}
					else {
						Type = "Stab";
					}
				}
			// UW2 Unarmed
				else {
					if (i == 9) {
						Type = "Ready";
					}
					else {
						Type = "Bash";	// They're sitting partially in the slash and bash range, though spaced like stab, but I _think_ unarmed is (or at least should be) considered bash damage
					}
				}

			// Export to CSV
				fprintf(
					out,
					"%s,"	// WeaponName
					"%s,"	// Hand
					"%s,"	// Type
					"%u,"	// FrameNumber
					"%s,"	// ImageIndex
					"%u,"	// OffsetX
					"%u\n",	// OffsetY
					WeaponName,			// WeaponName
					Hand,				// Hand
					Type,				// Type
					i,					// FrameNumber
					ImageIndex.c_str(),	// ImageIndex
					fgetc(fdx),			// OffsetX
					fgetc(fdy)			// OffsetY
				);
			}

		// Skip to next weapon block
			fseek(fdx, FrameCount, SEEK_CUR);
			fseek(fdy, FrameCount, SEEK_CUR);
		}
		fclose(fdx);
		fclose(fdy);
		fclose(out);
		fclose(GRFile);
	}

	printf("DAT files extracted to %s\n", OutPath.c_str());
	return 0;
}
