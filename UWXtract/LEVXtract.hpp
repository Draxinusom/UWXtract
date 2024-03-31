/*************
	UWXtract - Ultima Underworld Extractor

	LEV.ARK decoder/extractor header

	Moderately modified version of the UWDump DumpLevelArchive header by the
	Underworld Adventures Team to extract the contents of the level archive file

	Todo:
		Normalize names
		Clean out unused functions/variables
*************/
#pragma once
#include <set>
#include <bitset>

class GameStrings;

// Dumps level archives
class DumpLevelArchive {
	public:
	// ctor
		DumpLevelArchive(const GameStrings& gameStrings, bool isUw2)
			:m_isUw2(isUw2),
			m_gameStrings(gameStrings),
			m_currentLevel(0),
			m_currentFollowLevel(0)
			{
			}

	void Start(
		const std::string& filename,
		const std::string UWPath,
		const std::string OutPath
	);

	private:
		void FixLevelData();
		void DrawLevel();
		void ProcessLevel();
		void DumpObjectInfos();
		void DumpSpecialLinkChain(
			std::bitset<0x400>& visited,
			unsigned int pos,
			unsigned int indent = 0
		);
		void DumpObject(
			Uint16 pos
		);
		void DumpNPCInfos(
			Uint16 pos
		);
		void FollowLink(
			Uint16 link,
			unsigned int tilepos,
			bool special
		);
	// Export functions
		void ExportLevelList(
			char* TargetName,
			const std::string UWPath,
			const std::string OutPath
		);
		void ExportTileMap(
			FILE* OutLevMap
		);
		void ExportObject(
			FILE* OutLevObj
		);
		void ExportTexture(
			FILE* OutLevTexture
		);
		void ExportAnimation(
			FILE* OutLevAnimOverlay
		);

	private:
	// UW2 flag
		bool m_isUw2;

	// Game strings
		const GameStrings& m_gameStrings;

	// Current level
		unsigned int m_currentLevel;

	// Tilemap first byte
		std::vector<Uint16> tilemap;

	// Tilemap second byte
		std::vector<Uint16> tilemap2;

	// Recursion level for follow_link()
		unsigned int m_currentFollowLevel;

	// Link chain index from tilemap, 64*64 bytes long
		std::vector<Uint16> tilemap_links;

	// Set with free object list positions
		std::set<Uint16> freelist;

	// Common object info bytes, 0x400*8 bytes long
		std::vector<Uint16> objinfos;

	// NPC extra info bytes, 0x100*(0x1b-8) bytes long
		std::vector<Uint8> npcinfos;

	// Reference count for all objects, 0x400 long
		std::vector<unsigned int> linkcount;

	// Link reference index, 0x400 long
		std::vector<Uint16> linkref;

	// Tilemap position for all objects, 0x400 long
		std::vector<std::pair<Uint8, Uint8>> item_positions;

	// Texture mapping table
		std::vector<Uint16> texmapping;

	// Set with active mobile object list positions
		std::set<char> activelist;

	// Door texture mapping table
		std::vector<Uint8> doormapping;

	// Animation overlay
		std::vector<Uint16> animoverlay;

	// Contains addition item properties for object export
		std::vector<Uint8> ItemProperty;

};
