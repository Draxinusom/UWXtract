/*************
	UWXtract - Ultima Underworld Extractor

	LEV.ARK decoder/extractor

	Heavily modified version of the UWDump DumpLevelArchive function by the
	Underworld Adventures Team to extract the contents of the level archive file

	Todo:
		IsDiscovered flag on tile map (for saves)
		Add AutoMap tile type (bridge/water/lava/etc) to TileMap (not from automap, calculate from terrain/objects)
		Auto map notes
		Trigger/Trap detail export (maybe)
*************/
#include "common.hpp"
#include "LEVXtract.hpp"
#include "File.hpp"
#include "Bits.hpp"
#include "GameStrings.hpp"
#include "GameStringsImporter.hpp"
#include "ResourceManager.hpp"
#include "ArchiveFile.hpp"
#include <algorithm>
#include <bitset>
#include <filesystem>

extern std::string CleanDisplayName(const std::string InName, bool IsTitleCase, bool RemoveSpace);	// UWXtract.h -- Standard header conflicts with some of the other includes here so just including the function
extern bool AvailSaveGame(std::string UWPath, int SaveID);											// Util.cpp

using Import::GetBits;

void DumpLevArk(
	const std::string& filename,
	const std::string UWPath,
	const std::string OutPath,
	const GameStrings& gameStrings,
	bool isUw2
) {
   DumpLevelArchive dumper{ gameStrings, isUw2 };
   dumper.Start(filename, UWPath, OutPath);
}

void DumpLevelArchive::Start(
	const std::string& filename,
	const std::string UWPath,
	const std::string OutPath
) {
// Get target LEV.ARK path to set CSV names
	char* TargetName;
	if (filename.find("SAVE0") != std::string::npos) {
		TargetName = "SAVE0";
	}
	else if (filename.find("SAVE1") != std::string::npos) {
		TargetName = "SAVE1";
	}
	else if (filename.find("SAVE2") != std::string::npos) {
		TargetName = "SAVE2";
	}
	else if (filename.find("SAVE3") != std::string::npos) {
		TargetName = "SAVE3";
	}
	else if (filename.find("SAVE4") != std::string::npos) {
		TargetName = "SAVE4";
	}
	else {
		TargetName = "DATA";
	}

// Open file
	Base::File file{ filename.c_str(), Base::modeRead };

// Export the ARK file header info
	ExportLevelList(TargetName, UWPath, OutPath);

// Create output CSVs with header before looping through levels
	char TempPath[255];

	sprintf(TempPath, "%s\\%s_TileMap.csv", OutPath.c_str(), TargetName);
	FILE* OutLevMap = fopen(TempPath, "w");
	fprintf(OutLevMap, "LevelID,TileX,TileY,TileZ,TileType,FloorIndexID,WallIndexID,ObjectID,IsDoor,NoMagic,UW2Light\n");

	sprintf(TempPath, "%s\\%s_Object.csv", OutPath.c_str(), TargetName);
	FILE* OutLevObj = fopen(TempPath, "w");
	fprintf(OutLevObj, "LevelID,ObjectID,ItemID,ItemName,Condition,Enchantment,OwnedBy,TileX,TileY,NextObjectID,LinkID,Flags,IsEnchanted,IsDirection,IsInvisible,IsQuantity,Heading,PosX,PosY,PosZ,Quality,Owner,Quantity,Property,IsFree,IsActive,MobileData\n");

	sprintf(TempPath, "%s\\%s_TextureMap.csv", OutPath.c_str(), TargetName);
	FILE* OutLevTexture = fopen(TempPath, "w");
	fprintf(OutLevTexture, "LevelID,Type,TileID,TextureID\n");

	sprintf(TempPath, "%s\\%s_AnimationOverlay.csv", OutPath.c_str(), TargetName);
	FILE* OutLevAnimOverlay = fopen(TempPath, "w");
	fprintf(OutLevAnimOverlay, "LevelID,AnimationID,ObjectID,NumOfFrames,TileX,TileY\n");

// Get item properties (QualityType/CanBeOwned) - static for all levels so grab before loop
	sprintf(TempPath, "%s\\DATA\\COMOBJ.DAT", UWPath.c_str());
	FILE* COMOBJ = fopen(TempPath, "rb");
	fseek(COMOBJ, 0, SEEK_END);
	long COMOBJlen = ftell(COMOBJ);
	fseek(COMOBJ, 2, SEEK_SET);
	int ItemCount = (COMOBJlen - 2) / 11;

	for (int i = 0; i < ItemCount; i++) {
		unsigned char ItemBuffer[11];  // Easier to just read the full item in and pick out targets
		fread(ItemBuffer, 1, 11, COMOBJ);

		ItemProperty.push_back((ItemBuffer[10] & 0x0F) | (ItemBuffer[7] & 0x80));
	}
	fclose(COMOBJ);

// Set level counts for loop
	unsigned int numLevels = !m_isUw2 ? 9 : 80;
	unsigned int numDumpedLevels = 0;

// Loop and export each level
	for (m_currentLevel = 0; m_currentLevel < numLevels; m_currentLevel++) {
	// Reset for each level
		objinfos.clear();
		npcinfos.clear();
		tilemap.resize(0, 0);
		tilemap2.resize(0, 0);
		linkcount.clear();
		linkcount.resize(0x400, 0);
		linkref.clear();
		linkref.resize(0x400, 0);
		tilemap_links.clear();
		item_positions.clear();
		item_positions.resize(0x400, std::make_pair<Uint8, Uint8>(0xff, 0xff));
		freelist.clear();
		freelist.insert(0);
		activelist.clear();
		texmapping.resize(0, 0);
		doormapping.resize(0, 0);
		animoverlay.resize(0, 0);

	// UW2
		if (m_isUw2) {
		// Load file
			SDL_RWops* rwops = SDL_RWFromFile(filename.c_str(), "rb");
			Base::ArchiveFile ark{ Base::MakeRWopsPtr(rwops), m_isUw2 };

		// Skip if level not used
			if (!ark.IsAvailable(m_currentLevel)) {
				continue;
			}

		// Read in level
			Base::File levelFile = ark.GetFile(m_currentLevel);
			if (!levelFile.IsOpen()) {
				continue;
			}

		// Decode data
			std::vector<Uint8> decoded;
			decoded.resize(0x7e08, 0);
			levelFile.ReadBuffer(decoded.data(), 0x7e08);

		// TileMap
			{
				for (unsigned int i = 0; i < 64 * 64; i++) {
					Uint16 tileword1 = decoded[i * 4 + 0] | (decoded[i * 4 + 1] << 8);
					tilemap.push_back(tileword1);
			   
					Uint16 tileword2 = decoded[i * 4 + 2] | (decoded[i * 4 + 3] << 8);
					tilemap2.push_back(tileword2);

					Uint16 index = GetBits(tileword2, 6, 10);
					tilemap_links.push_back(index);
				}
			}

		// Object
			{
			// Get mobile objects
				for (unsigned int i = 0; i < 0x100; i++) {
				// Read object info
					objinfos.push_back(decoded[0x4000 + i * 27 + 0] | (decoded[0x4000 + i * 27 + 1] << 8));
					objinfos.push_back(decoded[0x4000 + i * 27 + 2] | (decoded[0x4000 + i * 27 + 3] << 8));
					objinfos.push_back(decoded[0x4000 + i * 27 + 4] | (decoded[0x4000 + i * 27 + 5] << 8));
					objinfos.push_back(decoded[0x4000 + i * 27 + 6] | (decoded[0x4000 + i * 27 + 7] << 8));

				// Read NPC extra info
					for (unsigned int j = 8; j < 27; j++) {
						npcinfos.push_back(decoded[0x4000 + i * 27 + j]);
					}
				}

			// Get static objects
				for (unsigned int n = 0; n < 0x300; n++) {
				// Read object info
					objinfos.push_back(decoded[0x5b00 + n * 8 + 0] | (decoded[0x5b00 + n * 8 + 1] << 8));
					objinfos.push_back(decoded[0x5b00 + n * 8 + 2] | (decoded[0x5b00 + n * 8 + 3] << 8));
					objinfos.push_back(decoded[0x5b00 + n * 8 + 4] | (decoded[0x5b00 + n * 8 + 5] << 8));
					objinfos.push_back(decoded[0x5b00 + n * 8 + 6] | (decoded[0x5b00 + n * 8 + 7] << 8));
				}
			}

		// FreeList
			{
			// Note: invalid offsets
				Uint16 mobile_items = decoded[0x7c02] | (decoded[0x7c03] << 8);
				Uint16 static_items = decoded[0x7c04] | (decoded[0x7c05] << 8);

				for (Uint16 i = 0; i < mobile_items + 1; i++) {
					Uint16 pos = decoded[0x7300 + i * 2] | (decoded[0x7300 + i * 2 + 1] << 8);
					freelist.insert(pos);
				}

				for (Uint16 n = 0; n < static_items + 1; n++) {
					Uint16 pos = decoded[0x74fc + n * 2] | (decoded[0x74fc + n * 2 + 1] << 8);
					freelist.insert(pos);
				}
			}

		// ActiveMobile
			{
				Uint16 active_items = decoded[0x7c00] | (decoded[0x7c01] << 8);

				for (Uint16 i = 0; i < active_items + 1; i++) {
					Uint16 pos = decoded[0x7AFC + i];
					activelist.insert(pos);
				}
			}

		// TextureMap
			{
				Base::File tex = ark.GetFile(80 + m_currentLevel);
				if (tex.IsOpen()) {
					std::vector<Uint8> origtexmap;
					origtexmap.resize(0x0086, 0);

					tex.ReadBuffer(origtexmap.data(), 0x0086);

				// Get floors/walls
					for (unsigned int i = 0; i < 64; i++) {
						Uint16 texid = origtexmap[i * 2] | (origtexmap[i * 2 + 1] << 8);
						texmapping.push_back(texid);
					}
				// Get doors
					for (unsigned int i = 0; i < 6; i++) {
						doormapping.push_back(origtexmap[128 + i]);
					}
				}
			}

		// AnimationOverlay
			{
				for (Uint16 i = 0; i < 64 + 1; i++) {
					Uint16 AnimOverlay1 = decoded[0x7C08 + (i * 6) + 0] | (decoded[0x7C08 + (i * 6) + 1] << 8);
					Uint16 AnimOverlay2 = decoded[0x7C08 + (i * 6) + 2] | (decoded[0x7C08 + (i * 6) + 3] << 8);
					Uint16 AnimOverlay3 = decoded[0x7C08 + (i * 6) + 4] | (decoded[0x7C08 + (i * 6) + 5] << 8);
					animoverlay.push_back(AnimOverlay1);
					animoverlay.push_back(AnimOverlay2);
					animoverlay.push_back(AnimOverlay3);
				}
			}

		}
	// UW1
		else {
		// Load file
			file.Seek(m_currentLevel * 4 + 2, Base::seekBegin);
			Uint32 offset = file.Read32();
			file.Seek(offset, Base::seekBegin);

		// TileMap
			{
				for (unsigned int i = 0; i < 64 * 64; i++) {
					Uint16 tileword1 = file.Read16();
					tilemap.push_back(tileword1);

					Uint16 tileword2 = file.Read16();
					tilemap2.push_back(tileword2);

					Uint16 index = GetBits(tileword2, 6, 10);
					tilemap_links.push_back(index);
				}
			}

		// Object
			for (unsigned int i = 0; i < 0x400; i++) {
			// Get standard data
				objinfos.push_back(file.Read16());
				objinfos.push_back(file.Read16());
				objinfos.push_back(file.Read16());
				objinfos.push_back(file.Read16());

			// Get mobile data where applicable
				if (i < 0x100) {
					for (unsigned int j = 0; j < 27 - 8; j++) {
						npcinfos.push_back(file.Read8());
					}
				}
			}

		// FreeList
			{
				file.Seek(offset + 0x7c02, Base::seekBegin);
				Uint16 mobile_items = file.Read16();
				Uint16 static_items = file.Read16();

				file.Seek(offset + 0x7300, Base::seekBegin);
				for (Uint16 i = 0; i < mobile_items + 1; i++) {
					freelist.insert(file.Read16());
				}

				file.Seek(offset + 0x74fc, Base::seekBegin);
				for (Uint16 n = 0; n < static_items + 1; n++) {
					freelist.insert(file.Read16());
				}
			}

		// ActiveMobile
			{
				file.Seek(offset + 0x7c00, Base::seekBegin);
				Uint16 active_items = file.Read16();

				file.Seek(offset + 0x7AFC, Base::seekBegin);
				for (Uint16 i = 0; i < active_items + 1; i++) {
					activelist.insert(file.Read8());
				}
			}

		// TextureMap
			{
				file.Seek((m_currentLevel + numLevels * 2) * 4 + 2, Base::seekBegin);
				offset = file.Read32();
				file.Seek(offset, Base::seekBegin);

			// Get walls
				for (unsigned int i = 0; i < 48; i++) {
					texmapping.push_back(file.Read16());
				}

			// Get floors
				for (unsigned int j = 0; j < 10; j++) {
					texmapping.push_back(file.Read16());
				}

			// Get doors
				for (unsigned int i = 0; i < 6; i++) {
					doormapping.push_back(file.Read8());
				}
			}

		// AnimationOverlay
			{
				file.Seek((m_currentLevel + numLevels) * 4 + 2, Base::seekBegin);
				offset = file.Read32();
				file.Seek(offset, Base::seekBegin);

				for (Uint16 i = 0; i < 64 + 1; i++) {
					animoverlay.push_back(file.Read16());
					animoverlay.push_back(file.Read16());
					animoverlay.push_back(file.Read16());
				}
			}
		}

	// Build object link chains
		ProcessLevel();

	// Export data
		ExportTileMap(OutLevMap);
		ExportObject(OutLevObj);
		ExportTexture(OutLevTexture);
		ExportAnimation(OutLevAnimOverlay);

		numDumpedLevels++;
	}

	fclose(OutLevMap);
	fclose(OutLevObj);
	fclose(OutLevTexture);
	fclose(OutLevAnimOverlay);
}

void DumpLevelArchive::ProcessLevel() {
// follow all links and special links, starting from the tilemap indices
	for (unsigned int tileIndex = 0; tileIndex < 64 * 64; tileIndex++) {
		Uint16 objectIndex = tilemap_links[tileIndex];

		if (objectIndex != 0) {
			m_currentFollowLevel = 0;
			linkref[objectIndex] = 0xffff; // from tilemap
			FollowLink(objectIndex, tileIndex, false);
		}
	}
}

void DumpLevelArchive::FollowLink(
	Uint16 link,
	unsigned int tilepos,
	bool special
) {
// Drop if recurse too deep -- likely circular loop (UW2 has a few)
	if (++m_currentFollowLevel > 32) {
		return;
	}

	do {
		Uint16* objptr = &objinfos[link * 4];

	// Ref count
		linkcount[link]++;

		if (!special) {
		// Store tilemap pos for that link
			item_positions[link] =
			std::make_pair<Uint8, Uint8>(tilepos & 63, tilepos >> 6);
		}

	// Don't recurse for some item types: "a_delete object trap"
		Uint16 item_id = GetBits(objptr[0], 0, 9);
		if (item_id == 0x018b) {
			break;
		}

	// Check if we should follow special links
		bool is_quantity = GetBits(objptr[0], 15, 1) != 0;
		if (!is_quantity) {
			Uint16 special_next = GetBits(objptr[3], 6, 10);
			if (special_next != 0) {
				linkref[special_next] = link;
				FollowLink(special_next, 0, true);
			}
		}

		Uint16 oldlink = link;

	// Get next item in chain
		link = GetBits(objptr[2], 6, 10);

		if (link != 0) {
			linkref[link] = oldlink;
		}
	} while (link != 0);

	--m_currentFollowLevel;
}

void DumpLevelArchive::ExportLevelList(
	char* TargetName,
	const std::string UWPath,
	const std::string OutPath
) {
	/***
		Note:
			In this file a dummy ID called "LevelID" is created that aligns with the tile map Block + 1
			and is used in all other export files instead of the block number

			The is done as the automap tiles/notes and texture/animation (UW1 only) maps are separate blocks
			and I think it's better to keep them aligned per level but don't want to confuse things by
			using the word "Block" wrong in the other files...

			So I'll make one up and, since I am, I'll add 1 to it so UW1 at least lines up with the level number in game :P
	***/
	char TempPath[255];

	sprintf(TempPath, "%s\\%s_Header.csv", OutPath.c_str(), TargetName);
	FILE* OutLevList = fopen(TempPath, "w");

	sprintf(TempPath, "%s\\%s\\LEV.ARK", UWPath.c_str(), TargetName);
	FILE* LEVARK = fopen(TempPath, "rb");

	// First two bytes is the number of blocks in the file
	unsigned short ActiveBlock;
	fread(&ActiveBlock, sizeof(unsigned short), 1, LEVARK);

	if (!m_isUw2) {
	// UW1 is much simpler than UW2 with only offsets stored in the header and no worlds
		fprintf(OutLevList, "Block,Type,Offset,DataSize,LevelID,LevelName\n");
	
	// However, makes getting the data size a pain as you have to calculate it off the next with a value or EOF
	// Grab file size to determine last block's size
		fseek(LEVARK, 0, SEEK_END);
		unsigned int LEVSize = ftell(LEVARK);
		fseek(LEVARK, 2, SEEK_SET);

	// Read in block offsets
		unsigned int HeaderData[135];
		fread(HeaderData, sizeof(unsigned int), 135, LEVARK);

	// Get data size
		unsigned int BlockSize[135];
		for (int b = 0; b < 135; b++) {
		// First initialize the value
			BlockSize[b] = 0;

		// Break out if empty block
			if (HeaderData[b] == 0) {
				continue;
			}

	// Need to loop through to find next active block
			for (int a = b + 1; a < 135; a++) {
				if (HeaderData[a] > 0) {
					BlockSize[b] = HeaderData[a] - HeaderData[b];
					break;
				}
			}
	// Catch last block
			if (BlockSize[b] == 0) {
				BlockSize[b] = LEVSize - HeaderData[b];
			}
		}

	// Now loop each block and export
		for (int b = 0; b < ActiveBlock; b++) {
			char BlockOffset[6] = "";	// Using this to set it blank instead of 000000 for unused blocks
			char* BlockType;
			int LevelID;
			char LevelName[13] = "";

		// Set BlockOffset if block in use
			if (HeaderData[b] != 0) {
				sprintf(BlockOffset, "%06X", HeaderData[b]);
			}


		// Identify block type and set level IDs
			if (b < 9) {
				BlockType = "Tile Map";
				LevelID = b + 1;
			}
			else if (b < 18) {
				BlockType = "Animation Overlay";
				LevelID = (b - 9) + 1;
			}
			else if (b < 27) {
				BlockType = "Texture Map";
				LevelID = (b - 18) + 1;
			}
			else if (b < 36) {
				BlockType = "Automap Tiles";
				LevelID = (b - 27) + 1;
			}
			else if (b < 45) {
				BlockType = "Automap Notes";
				LevelID = (b - 36) + 1;
			}
			else {
				BlockType = "Unknown";
				LevelID = b;	// Should never exists and no idea where it goes so just leave as is -- Actually, pretty sure this is where you can make notes on blank map pages if you scroll past L8 in the auto map - will test and alter if so
			}

		// Set LevelName
			if (LevelID < 9) {
				sprintf(LevelName, "%s", ("Level " + std::to_string(LevelID)).c_str());
			}
			else if (LevelID == 9) {
				sprintf(LevelName, "%s", "Ethereal Void");
			}

		// Export to CSV
			fprintf(
				OutLevList,
				"%u,"	// Block
				"%s,"	// Type
				"%s,"	// Offset
				"%s,"	// DataSize
				"%s,"	// LevelID
				"%s\n",	// LevelName
				b,														// Block
				BlockType,												// Type
				BlockOffset,											// Offset
				_stricmp("", BlockOffset) == 0 ? "" : std::to_string(BlockSize[b]).c_str(),	// DataSize -- Note, leave blank on unused blocks
				LevelID > 44 ? "" : std::to_string(LevelID).c_str(),	// LevelID -- Leave blank on Unknown blocks
				LevelName												// LevelName
			);
		}
	}
	else {
		fprintf(OutLevList, "Block,Type,Offset,DataSize,TotalSize,CanCompress,IsCompressed,IsPadded,LevelID,WorldName,WorldLevel,LevelName\n");

	// Skip over blank block (maybe a self reference to the header as a block?)
		fseek(LEVARK, 6, SEEK_SET);

		/***
			Storing all header data in 1 array
			0:	Offset		-- Starting position of block in LEV.ARK file
			1:	Flags		-- Contains flags indicating compression state and padding
			2:	DataSize	-- Size of the data in the block
			3:	TotalSize	-- Total size of the block, data + padding
		***/
		unsigned int HeaderData[320][4];

	// Get header data
		for (int v = 0; v < 4; v++) {
			for (int b = 0; b < ActiveBlock; b++) {
				unsigned int HeaderBuffer;
				fread(&HeaderBuffer, sizeof(unsigned int), 1, LEVARK);

				HeaderData[b][v] = HeaderBuffer;
			}
		}

	// Dump header details to CSV
		for (int b = 0; b < ActiveBlock; b++) {
			char BlockOffset[6] = "";	// Using this to set it blank instead of 000000 for unused blocks
			char* BlockType;
			int LevelID;
			char* LevelName;
			char* WorldName;
			char* WorldLevel;	// Using this to indicate which map this is in game on the Automap -- Note: Is char so can set blank on unused levels

		// Set BlockOffset if block in use
			if (HeaderData[b][0] != 0) {
				sprintf(BlockOffset, "%06X", HeaderData[b][0]);
			}

		// Identify block type and set level IDs
			if (b < 80) {
				BlockType = "Tile Map";
				LevelID = b + 1;
			}
			else if (b < 160) {
				BlockType = "Texture Map";
				LevelID = (b - 80) + 1;
			}
			else if (b < 240) {
				BlockType = "Automap Tiles";
				LevelID = (b - 160) + 1;
			}
			else if (b < 320) {
				BlockType = "Automap Notes";
				LevelID = (b - 240) + 1;
			}
			else {
				BlockType = "Unknown";
				LevelID = b + 1;	// Should never exists and no idea where it goes so just leave as is
			}

		// Set WorldName -- Not all block have a level but we'll go ahead and set it anyways as UW2 views worlds as blocks of 8
			if (LevelID < 9) {
				WorldName = "Britannia";
			}
			else if (LevelID < 17) {
				WorldName = "Prison Tower";
			}
			else if (LevelID < 25) {
				WorldName = "Killorn Keep";
			}
			else if (LevelID < 33) {
				WorldName = "Ice Caverns";
			}
			else if (LevelID < 41) {
				WorldName = "Talorus";
			}
			else if (LevelID < 49) {
				WorldName = "Scintillus Academy";
			}
			else if (LevelID < 57) {
				WorldName = "Tomb of Praecor Loth";
			}
			else if (LevelID < 65) {
				WorldName = "Pits of Carnage";
			}
			else if (LevelID < 73) {
				WorldName = "Ethereal Void";
			}
			else {
				WorldName = "Unknown";
			}

		// Set LevelName & WorldLevel
			switch (LevelID) {
			// Britannia
				case  1:	WorldLevel = "1"; LevelName = "Castle of Lord British"; break;
				case  2:	WorldLevel = "2"; LevelName = "Castle Basement"; break;
				case  3:	WorldLevel = "3"; LevelName = "Sewer 1"; break;
				case  4:	WorldLevel = "4"; LevelName = "Sewer 2"; break;
				case  5:	WorldLevel = "5"; LevelName = "Sewer 3"; break;
			// Prison Tower
				case  9:	WorldLevel = "1"; LevelName = "Basement"; break;
				case 10:	WorldLevel = "2"; LevelName = "First Floor"; break;
				case 11:	WorldLevel = "3"; LevelName = "Second Floor"; break;
				case 12:	WorldLevel = "4"; LevelName = "Third Floor"; break;
				case 13:	WorldLevel = "5"; LevelName = "Fourth Floor"; break;
				case 14:	WorldLevel = "6"; LevelName = "Fifth Floor"; break;
				case 15:	WorldLevel = "7"; LevelName = "Sixth Floor"; break;
				case 16:	WorldLevel = "8"; LevelName = "Seventh Floor"; break;
			// Killorn Keep
				case 17:	WorldLevel = "1"; LevelName = "Level 1"; break;
				case 18:	WorldLevel = "2"; LevelName = "Level 2"; break;
			// Ice Caverns
				case 25:	WorldLevel = "1"; LevelName = "Level 1"; break;
				case 26:	WorldLevel = "2"; LevelName = "Level 2"; break;
			// Talorus
				case 33:	WorldLevel = "1"; LevelName = "Level 1"; break;
				case 34:	WorldLevel = "2"; LevelName = "Level 2"; break;
			// Scintillus Academy -- These _do_ have a name from the guide but they're kinda stupid and long so they're levels now :)
				case 41:	WorldLevel = "1"; LevelName = "Level 1"; break;
				case 42:	WorldLevel = "2"; LevelName = "Level 2"; break;
				case 43:	WorldLevel = "3"; LevelName = "Level 3"; break;
				case 44:	WorldLevel = "4"; LevelName = "Level 4"; break;
				case 45:	WorldLevel = "5"; LevelName = "Level 5"; break;
				case 46:	WorldLevel = "6"; LevelName = "Level 6"; break;
				case 47:	WorldLevel = "7"; LevelName = "Level 7"; break;
				case 48:	WorldLevel = "8"; LevelName = "Level 8"; break;
			// Tomb of Praecor Loth
				case 49:	WorldLevel = "1"; LevelName = "Level 1"; break;
				case 50:	WorldLevel = "2"; LevelName = "Level 2"; break;
				case 51:	WorldLevel = "3"; LevelName = "Level 3"; break;
				case 52:	WorldLevel = "4"; LevelName = "Level 4"; break;
			// Pits of Carnage
				case 57:	WorldLevel = "1"; LevelName = "Prison"; break;
				case 58:	WorldLevel = "2"; LevelName = "Upper Dungeons"; break;
				case 59:	WorldLevel = "3"; LevelName = "Lower Dungeons"; break;
			// Ethereal Void -- Doesn't have levels
				case 65:	WorldLevel = ""; LevelName = "Ethereal Void - Color Zone"; break;	// Not really sure what to call this one, while it has all of blue in it (I think), it also has portions of all the other colors (I think?) in it too
				case 66:	WorldLevel = ""; LevelName = "Ethereal Void - Purple Zone"; break;
				case 67:	WorldLevel = ""; LevelName = "Scintillus Academy - Secure Vault"; break;
				case 68:	WorldLevel = ""; LevelName = "Ethereal Void - Yellow Zone"; break;
				case 69:	WorldLevel = ""; LevelName = "Ethereal Void"; break;
				case 71:	WorldLevel = ""; LevelName = "Ethereal Void - Red Zone"; break;
				case 72:	WorldLevel = ""; LevelName = "Killorn Deathtrap"; break;
			// Unknown
				case 73:	WorldLevel = ""; LevelName = "Tomb of Praecor Loth - Level 3-Alt"; break;	// Don't recall if you actually access this one?
				default:	WorldLevel = ""; LevelName = ""; break;
			}

		// Export to CSV
			fprintf(
				OutLevList,
				"%u,"	// Block
				"%s,"	// Type
				"%s,"	// Offset
				"%s,"	// DataSize
				"%s,"	// TotalSize
				"%u,"	// CanCompress
				"%u,"	// IsCompressed
				"%u,"	// HasExtraSpace
				"%u,"	// LevelID
				"%s,"	// WorldName
				"%s,"	// WorldLevel
				"%s\n",	// LevelName
				b,								// Block
				BlockType,						// Type
				BlockOffset,					// Offset
				_stricmp("", BlockOffset) == 0 ? "" : std::to_string(HeaderData[b][2]).c_str(),	// DataSize - Note, leave blank on unused blocks
				_stricmp("", BlockOffset) == 0 ? "" : std::to_string(HeaderData[b][3]).c_str(),	// TotalSize - Note, leave blank on unused blocks
				HeaderData[b][1] & 0x01,		// CanCompress
				(HeaderData[b][1] & 0x02) >> 1,	// IsCompressed
				(HeaderData[b][1] & 0x04) >> 2,	// IsPadded
				LevelID,						// LevelID
				WorldName,						// WorldName
				WorldLevel,						// WorldLevel
				LevelName						// LevelName
			);
		}
	}

	fclose(OutLevList);
	fclose(LEVARK);
}

void DumpLevelArchive::ExportTileMap(
	FILE* OutLevMap
) {
	for (unsigned int y = 0; y < 64; y++) {
		for (unsigned int x = 0; x < 64; x++) {
		// Get tile data
			Uint16 tileword1 = tilemap[(y * 64) + x];
			Uint16 tileword2 = tilemap2[(y * 64) + x];

		// Get TileType
			const char* TileType;
			switch (tileword1 & 0x000F) {
				case  0:	TileType = "Solid"; break;
				case  1:	TileType = "Open"; break;
				case  2:	TileType = "DiagonalSE"; break;
				case  3:	TileType = "DiagonalSW"; break;
				case  4:	TileType = "DiagonalNE"; break;
				case  5:	TileType = "DiagonalNW"; break;
				case  6:	TileType = "SlopeN"; break;
				case  7:	TileType = "SlopeS"; break;
				case  8:	TileType = "SlopeE"; break;
				case  9:	TileType = "SlopeW"; break;
				default:	TileType = "Unknown"; break;
			}

		// Export to CSV
			fprintf(
				OutLevMap,
				"%u,"    // LevelID
				"%u,"    // TileX
				"%u,"    // TileY
				"%u,"    // TileZ
				"%s,"    // TileType
				"%u,"    // FloorIndexID
				"%u,"    // WallIndexID
				"%u,"    // ObjectID
				"%u,"    // IsDoor
				"%u,"    // NoMagic
				"%u\n",  // UW2Light
				m_currentLevel + 1,           // LevelID -- Note: Add 1 to match header
				x,                            // TileX
				y,                            // TileY
				(tileword1 & 0x00F0) >> 1,    // TileZ - this is correct, only 4 bits stored for 7 bit value
				TileType,                     // TileType
				(tileword1 & 0x3C00) >> 10,   // FloorIndexID
				tileword2 & 0x003F,           // WallIndexID
				(tileword2 & 0xFFC0) >> 6,    // ObjectID
				(tileword1 & 0x8000) >> 15,   // IsDoor
				tileword1 >> 14,              // NoMagic
				(tileword1 & 0x0100) >> 8     // UW2Light
			);
		}
	}
}

void DumpLevelArchive::ExportObject(
	FILE* OutLevObj
) {
	for (unsigned short int n = 0; n < 0x400; n++) {
	// Flag "active" mobile objects - Believe this is indicating the NPC is actually on the map, not a template holding an object slot for a create object trap
		std::string IsActive = ""; // Initially setting blank as I think it may be confusing on the static objects
		if (n > 0 && n < 256) {
			IsActive = activelist.find(n) != activelist.end() ? "1" : "0";
		}

	// Flag "free" objects - Hardcoding null object 0 as free - not included in free list
		int IsFree = 0;
		if (n == 0 || freelist.find(n) != freelist.end()) {
			IsFree = 1;
		}

	// Get tile location (where applicable)
		char TileX[2] = "";
		char TileY[2] = "";
		if (item_positions[n].first != 255) {
			sprintf(TileX, "%u", item_positions[n].first);
			sprintf(TileY, "%u", item_positions[n].second);
		}

	// Get ItemID
		short int ItemID = objinfos[n * 4] & 0x01FF;

	// Set item name - done here so can add NPC name if exists
		char ItemName[64];
		sprintf(ItemName, CleanDisplayName(m_gameStrings.GetString(4, ItemID).c_str(), true, false).c_str());

	// Modified version of IsQuantity flag used for multiple things so just grab once - Note:  Hardcoded false for all triggers and delete object trap
		int IsQuantity = ((ItemID > 415 && ItemID < 448) || ItemID == 395) ? 0 : objinfos[n * 4] >> 15;

	// Want these to show blank based on values so define here (probably a better way to do this but ehh)
		char NextObjectID[10] = "";
		if (objinfos[n * 4 + 2] >> 6 > 0) {
			sprintf(NextObjectID, "%u", objinfos[n * 4 +2] >> 6);
		}

		char LinkID[10] = "";
		if (IsQuantity == 0 && objinfos[n * 4 + 3] >> 6 > 0) {
			sprintf(LinkID, "%u", objinfos[n * 4 + 3] >> 6);
		}

		char Quantity[10] = "";
		if (IsQuantity == 1 && objinfos[n * 4 + 3] >> 6 < 512) {
			sprintf(Quantity, "%u", objinfos[n * 4 + 3] >> 6);
		}

		char Property[10] = "";
		short int PropertyID = 512;
		if (IsQuantity == 1 && objinfos[n * 4 + 3] >> 6 >= 512) {
			PropertyID = (objinfos[n * 4 + 3] >> 6) - 512;
			sprintf(Property, "%u", PropertyID);
		}

	// Get mobile data
		char MobileData[38] = "";
		char NPCName[64] = "";

		if (n < 0x100) {
		// Mobile data is largely not deciphered and/or variable as to value meaning
		// so not going to bother parsing it just glob it together in case someone is interested -- May come back to this
			for (int m = 0; m < 19; m++){
				sprintf(MobileData, "%s%02X", MobileData, (unsigned int)npcinfos[(n * 19) + m]);
			}

		// For named NPCs, add their name to the display name
			if (IsFree == 0 && npcinfos[(n * 19) + 18] > 0 && ItemID > 63 && ItemID < 128) { // Ignore free items and ensure it's an NPC type
				char NPCTemp[64];
				sprintf(NPCTemp, "%s", m_gameStrings.GetString(7, npcinfos[(n * 19) + 18] + 16).c_str());

				if (strlen(NPCTemp) > 0) {
				// Put NPC name in paranthesis after item name for normal NPCs
					if (strlen(ItemName) > 0) {
						sprintf(NPCName, " (%s)", NPCTemp);
					}
				// EV creatures, SoV, & Tyball don't have an item name so don't put in ()
					else {
						sprintf(NPCName, "%s", CleanDisplayName(NPCTemp, true, false).c_str());
					}
				}
			}
		}

	// Get item condition (where applicable)
		char Condition[16] = "";
	  // First get the string block for the item type
		unsigned char QualityTypeID = (ItemProperty[objinfos[n * 4] & 0x01FF] & 0x0F);

		if (QualityTypeID < 15 && n > 255 && IsFree == 0) {	// Exclude out of range and Mobile/Free objects
			unsigned char QualityStringID = objinfos[n * 4 + 2] & 0x003F;	// Get the item's quality
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

			sprintf(Condition, "%s", m_gameStrings.GetString(5, (QualityTypeID * 6) + QualityStringID).c_str());
		}

	// Get enchantment (where applicable)
		char IsEnchanted = (objinfos[n * 4] >> 12) & 0x01;
		char Enchantment[32] = "";

	  // Exclude Critters/Doors/Triggers/Traps
		if (IsFree == 0 && ((IsEnchanted == 1 && n > 255 && ItemID < 320 && PropertyID < 512) || (ItemID >= 144 && ItemID <= 175 && IsEnchanted == 0 && LinkID != ""))) {
			short int EnchantedItemID = ItemID;
			short int EnchantedPropertyID = PropertyID;

			if (IsEnchanted == 0) {
				short int EnchantedObjectID = objinfos[n * 4 + 3] >> 6;
				EnchantedItemID = objinfos[EnchantedObjectID * 4] & 0x01FF;

			// Spell
				if (EnchantedItemID == 288) {
					EnchantedPropertyID = (objinfos[EnchantedObjectID * 4 + 3] >> 6) - 512;
				}
			// Push out of range if not spell
				else {
					EnchantedPropertyID = 367;
				}
			}

		// Weapon - Standard
			if (EnchantedItemID < 32 && EnchantedPropertyID >= 192 && EnchantedPropertyID <= 207) {
				sprintf(Enchantment, "%s", m_gameStrings.GetString(6, EnchantedPropertyID + 256).c_str());
			}
		// Armor - Standard
			else if (EnchantedItemID >= 32 && EnchantedItemID < 64 && EnchantedPropertyID >= 192 && EnchantedPropertyID <= 207) {
				sprintf(Enchantment, "%s", m_gameStrings.GetString(6, EnchantedPropertyID + 272).c_str());
			}
		// Weapon/Armor & Fountain - Other types
			else if (EnchantedItemID < 64 || EnchantedItemID == 302) {
				sprintf(Enchantment, "%s", m_gameStrings.GetString(6, EnchantedPropertyID).c_str());
			}
		// Other types
			else if (EnchantedPropertyID < 64) {
				sprintf(Enchantment, "%s", m_gameStrings.GetString(6, EnchantedPropertyID + 256).c_str());
			}
		// Other types
			else {
				sprintf(Enchantment, "%s", m_gameStrings.GetString(6, EnchantedPropertyID + 144).c_str());
			}
		}

	// Get owner (where applicable)
		char OwnedBy[24] = "";
		if (n > 255 && (ItemProperty[objinfos[n * 4] & 0x01FF] & 0x80) == 0x80 && IsFree == 0 && (objinfos[n * 4 + 3] & 0x003F) > 0) {
			sprintf(OwnedBy, "%s", CleanDisplayName(m_gameStrings.GetString(1, (objinfos[n * 4 + 3] & 0x003F) + 370), true, false).c_str());
		}

	// Export to CSV
		fprintf(
			OutLevObj,
			"%u,"		// LevelID
			"%u,"		// ObjectID
			"%u,"		// ItemID
			"%s%s,"		// ItemName (NPCName)
			"%s,"		// Condition
			"%s,"		// Enchantment
			"%s,"		// OwnedBy
			"%s,"		// TileX
			"%s,"		// TileY
			"%s,"		// NextObjectID
			"%s,"		// LinkID
			"%u,"		// Flags
			"%u,"		// IsEnchanted
			"%u,"		// IsDirection
			"%u,"		// IsInvisible
			"%u,"		// IsQuantity
			"%u,"		// Heading
			"%u,"		// PosX
			"%u,"		// PosY
			"%u,"		// PosZ
			"%u,"		// Quality
			"%u,"		// Owner
			"%s,"		// Quantity
			"%s,"		// Property
			"%u,"		// IsFree
			"%s,"		// IsActive
			"%s\n",		// MobileData
			m_currentLevel + 1,					// LevelID -- Note: Add 1 to match header
			n,									// ObjectID
			ItemID,								// ItemID
			ItemName, NPCName,					// ItemName
			Condition,							// Condition
			Enchantment,						// Enchantment
			OwnedBy,							// OwnedBy
			TileX,								// TileX
			TileY,								// TileY
			NextObjectID,						// NextObjectID
			LinkID,								// LinkID
			(objinfos[n * 4] >> 9) & 0x07,      // Flags
			(objinfos[n * 4] >> 12) & 0x01,     // IsEnchanted
			(objinfos[n * 4] >> 13) & 0x01,     // IsDirection
			(objinfos[n * 4] >> 14) & 0x01,     // IsInvisible
			objinfos[n * 4] >> 15,              // IsQuantity - using this instead of variable so original value is returned
			(objinfos[n * 4 + 1] >> 7) & 0x07,  // Heading
			objinfos[n * 4 + 1] >> 13,          // PosX
			(objinfos[n * 4 +1] >> 10) & 0x07,  // PosY
			objinfos[n * 4 + 1] & 0x007F,       // PosZ
			objinfos[n * 4 + 2] & 0x003F,       // Quality
			objinfos[n * 4 + 3] & 0x003F,       // Owner
			Quantity,							// Quantity
			Property,							// Property
			IsFree,								// IsFree
			IsActive.c_str(),					// IsActive
			MobileData							// MobileData
		);
	}
}

void DumpLevelArchive::ExportTexture(
	FILE* OutLevTexture
) {
	if (!m_isUw2) {
	// UW1 - Wall
		for (int i = 0; i < 48; i++) {
			fprintf(
				OutLevTexture,
				"%u,"    // LevelID
				"Wall,"  // Type
				"%u,"    // TileID		-- Maps to WallID in TileMap
				"%u\n",  // TextureID	-- Maps to TerrainID from TERRAIN.DAT and to the image in W64.TR
				m_currentLevel + 1,	// LevelID -- Note: Add 1 to match header
				// Type hardcoded
				i,					// TileID
				texmapping[i]		//TextureID
			);
		}
	// UW1 - Floor
		for (int i = 0; i < 10; i++) {
			fprintf(
				OutLevTexture,
				"%u,"    // LevelID
				"Floor,"  // Type
				"%u,"    // TileID		-- Maps to FloorID in TileMap
				"%u\n",  // TextureID	-- Maps to TerrainID from TERRAIN.DAT and to the image in F16/32.TR
				m_currentLevel + 1,	// LevelID -- Note: Add 1 to match header
				// Type -- hardcoded
				i,					// TileID
				texmapping[i + 48]	//TextureID
			);
		}
	// UW1 - Door
		for (int i = 0; i < 6; i++) {
			fprintf(
				OutLevTexture,
				"%u,"    // LevelID
				"Door,"  // Type
				"%u,"    // TileID		-- Maps to door item by ID + 0x0140 (320)
				"%u\n",  // TextureID	-- Maps to the image in DOORS.GR
				m_currentLevel + 1,	// LevelID -- Note: Add 1 to match header
				// Type -- hardcoded
				i,					// TileID
				doormapping[i]		//TextureID
			);
		}
	}
	else {
	// UW2 - Floor/Wall
		for (int i = 0; i < 16; i++) {
			fprintf(
				OutLevTexture,
				"%u,"    // LevelID
				"Floor/Wall,"  // Type
				"%u,"    // TileID		-- Maps to either FloorID or WallID in TileMap
				"%u\n",  // TextureID	-- Maps to TerrainID from TERRAIN.DAT and to the image in T64.TR
				m_currentLevel + 1,	// LevelID -- Note: Add 1 to match header
				// Type -- hardcoded
				i,					// TileID
				texmapping[i]		//TextureID
			);
		}
	// UW2 - Wall
		for (int i = 16; i < 64; i++) {
			fprintf(
				OutLevTexture,
				"%u,"    // LevelID
				"Wall,"  // Type
				"%u,"    // TileID		-- Maps to WallID in TileMap
				"%u\n",  // TextureID	-- Maps to TerrainID from TERRAIN.DAT and to the image in T64.TR
				m_currentLevel + 1,	// LevelID -- Note: Add 1 to match header
				// Type -- hardcoded
				i,					// TileID
				texmapping[i]		//TextureID
			);
		}
	// UW2 - Door
		for (int i = 0; i < 6; i++) {
			fprintf(
				OutLevTexture,
				"%u,"    // LevelID
				"Door,"  // Type
				"%u,"    // TileID		-- Maps to door item by ID + 0x0140 (320)
				"%u\n",  // TextureID	-- Maps to the image in DOORS.GR
				m_currentLevel + 1,	// LevelID -- Note: Add 1 to match header
				// Type -- hardcoded
				i,					// TileID
				doormapping[i]		//TextureID
			);
		}
	}
}

void DumpLevelArchive::ExportAnimation(
	FILE* OutLevAnimOverlay
) {
	for (int i = 0; i < 64; i++) {
	// Break if no more animations in block
		if (animoverlay[i * 3] == 0x0000 && animoverlay[i * 3 + 1] == 0x0000 && animoverlay[i * 3 + 2] == 0x0000) {
			break;
		}

		fprintf(
			OutLevAnimOverlay,
			"%u,"    // LevelID
			"%u,"    // AnimationID
			"%u,"    // ObjectID
			"%i,"    // NumOfFrames
			"%u,"    // TileX
			"%u\n",  // TileY
			m_currentLevel + 1,					// LevelID -- Note: Add 1 to match header
			i,									// AnimationID
			animoverlay[i * 3] >> 6,			// ObjectID
			animoverlay[i * 3 + 1] == 0xFFFF ? -1 : animoverlay[i * 3 + 1],	// NumOfFrames
			animoverlay[i * 3 + 2] & 0x00FF,	// TileX
			animoverlay[i * 3 + 2] >> 8			// TileY
		);
	}
}

// Main function
int LEVXtract(
	const bool IsUW2,
	std::string ExportTarget,
	const std::string UWPath,
	const std::string OutPath
) {
// Using this to get isdigit to behave for save check (I'm certainly doing something wrong but this works and tired of messing with it)
	char SaveID[1];
	sprintf(SaveID, "%s", ExportTarget.c_str());

	GameStrings gs;
	Import::GameStringsImporter importer(gs);
	importer.LoadStringsPakFile((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Export all
	if (ExportTarget == "*") {
	// Create output folder (done here in case invalid parameter passed) -- Note:  There is some conflict with the standard UWXtract header in this function so creating folders manually
		if (!(std::filesystem::exists(OutPath))) {
			std::filesystem::create_directories(OutPath);
		}

		DumpLevArk(UWPath + "\\DATA\\LEV.ARK", UWPath, OutPath, gs, IsUW2);

		// Loop through and export any saves that exist -- Note:  Skipping temp SAVE0 when exporting all (not really all I guess :P)
		for (int s = 1; s < 5; s++) {
			if (AvailSaveGame(UWPath, s)) {
				char SavePath[255];
				sprintf(SavePath, "%s\\SAVE%u\\LEV.ARK", UWPath.c_str(), s);

				DumpLevArk(SavePath, UWPath, OutPath, gs, IsUW2);
			}
		}

		printf("Level archives extracted to %s\n", OutPath.c_str());
		return 0;
	}
// Data only
	else if (ExportTarget == "d" || ExportTarget == "D") {
	// Create output folder (done here in case invalid parameter passed)
		if (!(std::filesystem::exists(OutPath))) {
			std::filesystem::create_directories(OutPath);
		}
		DumpLevArk(UWPath + "\\DATA\\LEV.ARK", UWPath, OutPath, gs, IsUW2);
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
		if (!(std::filesystem::exists(OutPath))) {
			std::filesystem::create_directories(OutPath);
		}
		char SavePath[255];
		sprintf(SavePath, "%s\\SAVE%u\\LEV.ARK", UWPath.c_str(), stoi(ExportTarget));

		DumpLevArk(SavePath, UWPath, OutPath, gs, IsUW2);
	}

	printf("Level archive extracted to %s\n", OutPath.c_str());
	return 0;
}
