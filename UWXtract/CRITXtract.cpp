/*************
	UWXtract - Ultima Underworld Extractor

	CRIT (NPC) frame image extractor

	Heavily modified version of the "hacking" critdec function by the
	Underworld Adventures Team to extract critter animation frames as images

	Todo:
		Redo animation log output -- largely left alone for now
			-- need to decide best way to represent data / don't like the current style would prefer CSV output like others
		Minor refactoring and flush out/clean up code comments
*************/
#include "UWXtract.h"

extern void GetPalette32(const std::string UWPath, const unsigned int PaletteIndex, char PaletteBuffer[256 * 4], bool IncludePartialTransparency);	// Util.cpp

void ImageDecode4BitRLE(
	FILE* fd,
	FILE* out,
	unsigned int bits,
	unsigned int datalen,
	unsigned char* auxpalidx
) {
// Bit extraction variables
	unsigned int bits_avail = 0;
	unsigned int rawbits = -1;
	unsigned int bitmask = ((1 << bits) - 1) << (8 - bits);
	unsigned int nibble;

// RLE decoding vars
	int pixcount = 0;
	int stage = 0; // we start in stage 0
	int count;
	int record = 0; // we start with record 0=repeat (3=run)
	int repeatcount = 0;

	while (pixcount < int(datalen)) {
	// Get new bits
		if (bits_avail < bits) {
		// Not enough bits available
			if (bits_avail > 0) {
				nibble = ((rawbits & bitmask) >> (8 - bits_avail));
				nibble <<= (bits - bits_avail);
			}
			else {
				nibble = 0;
			}
			rawbits = fgetc(fd);
			if (rawbits == EOF) {
				return;
			}

			unsigned int shiftval = 8 - (bits - bits_avail);
			nibble |= (rawbits >> shiftval);
			rawbits = (rawbits << (8 - shiftval)) & 0xFF;
			bits_avail = shiftval;
		}
		else {
		// We still have enough bits
			nibble = (rawbits & bitmask) >> (8 - bits);
			bits_avail -= bits;
			rawbits <<= bits;
		}

	// Now that we have a nibble
		switch (stage) {
			case 0: // We retrieve a new count
				if (nibble == 0) {
					stage++;
				}
				else {
					count = nibble;
					stage = 6;
				}
				break;
			case 1:
				count = nibble;
				stage++;
				break;
			case 2:
				count = (count << 4) | nibble;
				if (count == 0) {
					stage++;
				}
				else {
					stage = 6;
				}
				break;
			case 3:
			case 4:
			case 5:
				count = (count << 4) | nibble;
				stage++;
				break;
		}

		if (stage < 6) {
			continue;
		}

		switch (record) {
		// Repeat record stage 1
			case 0:
				if (count == 1) {
					record = 3; // Skip this record; a run follows
					break;
				}

				if (count == 2) {
					record = 2; // Multiple run records
					break;
				}

				record = 1; // Read next nibble; it's the color to repeat
				continue;
		// Repeat record stage 2
			case 1:
				{
				// Repeat 'nibble' color 'count' times
					for (int n = 0; n < count; n++) {
						fputc(auxpalidx[nibble], out);
						pixcount++;
					}
				}

				if (repeatcount == 0) {
					record = 3; // Next one is a run record
				}
				else {
					repeatcount--;
					record = 0; // Continue with repeat records
				}
				break;
		// Multiple repeat stage -- 'count' specifies the number of repeat record to appear
			case 2:
				repeatcount = count - 1;
				record = 0;
				break;
		// Run record stage 1 -- copy 'count' nibbles
			case 3:
				record = 4; // Retrieve next nibble
				continue;
		// Run record stage 2 -- now we have a nibble to write
			case 4:
				fputc(auxpalidx[nibble], out);
				pixcount++;

				if (--count == 0) {
					record = 0; // Next one is a repeat again
				}
				else {
					continue;
				}
				break;
		}
		stage = 0;
	}
}

std::string NPCNameFix(
	const std::string InName,
	bool IsUW2,
	const unsigned int anim,
	const unsigned int auxpal,
	bool IsDisplayName
) {
	std::string NPCName = InName;

// UW1
	if (!IsUW2) {
		switch (anim) {
			case 00:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Goblin (Club/Green/Brown)" : "GoblinClub_GN_B"; break;
					case 1:	NPCName = IsDisplayName ? "Goblin (Club/Gray/Green)" : "GoblinClub_GY_GN"; break;
					case 2:	NPCName = IsDisplayName ? "Goblin (Club/Green/Red)" : "GoblinClub_GN_R"; break;
					case 3:	NPCName = IsDisplayName ? "Goblin (Club/Gray/Brown)" : "GoblinClub_GY_B"; break;
				}
				break;
			case 04:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Mage (Male/White/Yellow)" : "Mage_M_W_Y"; break;
					case 1:	NPCName = IsDisplayName ? "Mage (Male/White/Red)" : "Mage_M_W_R"; break;
					case 2:	NPCName = IsDisplayName ? "Mage (Male/Black/Yellow)" : "Mage_M_B_Y"; break;
				}
				break;
			case 010: NPCName = IsDisplayName ? "Mage (Female/White/Blue)" : "Mage_F_W_B"; break;
			case 013:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Ghoul (Green/Brown)" : "Ghoul_GN_B"; break;
					case 2:	NPCName = IsDisplayName ? "Ghoul (Red/Gray)" : "Ghoul_R_GY"; break;
				}
				break;
			case 014: NPCName = IsDisplayName ? "The Slasher of Veils" : "SlasherOfVeils"; break;
			case 015:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Ghost" : "Ghost"; break;
					case 1:	NPCName = IsDisplayName ? "Ghost (White)" : "Ghost_W"; break;
					case 2:	NPCName = IsDisplayName ? "Ghost (Blue)" : "Ghost_B"; break;
				}
				break;
			case 016:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Goblin (Sword/Green/Brown)" : "GoblinSword_GN_B"; break;
					case 1:	NPCName = IsDisplayName ? "Goblin (Sword/Gray/Green)" : "GoblinSword_GY_GN"; break;
				}
				break;
			case 020:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Giant Tan Rat" : "GiantTanRat"; break;
					case 1:	NPCName = IsDisplayName ? "Giant Grey Rat" : "GiantGreyRat"; break;
				}
				break;
			case 021:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Fighter (Female/White/Blue)" : "Fighter_F_W_B"; break;
					case 1:	NPCName = IsDisplayName ? "Fighter (Female/Black/Red)" : "Fighter_F_B_R"; break;
				}
				break;
			case 025:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Mage (Staff/Male/White/Blue)" : "MageStaff_M_W_B"; break;
					case 1:	NPCName = IsDisplayName ? "Mage (Staff/Male/White/Red)" : "MageStaff_M_W_R"; break;
				}
				break;
			case 030: NPCName = IsDisplayName ? "Ethereal Void Creature" : "EVCreature"; break;
			case 032:
				switch (auxpal) {
					case 1:	NPCName = IsDisplayName ? "Fighter (Male/White/Red)" : "Fighter_M_W_R"; break;
					case 2:	NPCName = IsDisplayName ? "Fighter (Male/White/Brown)" : "Fighter_M_W_B"; break;
					case 3:	NPCName = IsDisplayName ? "Fighter (Male/Black/Green)" : "Fighter_M_B_GN"; break;
				}
				break;
			case 033:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Mountainman (Red/Green)" : "Mountainman_R_G"; break;
					case 1:	NPCName = IsDisplayName ? "Mountainman (White/Brown)" : "Mountainman_W_B"; break;
				}
				break;
			case 035: NPCName = "Tyball"; break;
			case 036: NPCName = IsDisplayName ? "Ethereal Void Creature" : "EVCreature"; break;
			case 037: NPCName = IsDisplayName ? "Ethereal Void Creature" : "EVCreature"; break;
		}
	}
// UW2
	else {
		switch (anim) {
			case 05:
				if (auxpal == 1) {
					NPCName = IsDisplayName ? "Liche Assassin" : "LicheAssassin";
				}
				break;
			case 06: NPCName = IsDisplayName ? "Goblin (Club)" : "GoblinClub"; break;
			case 07: NPCName = IsDisplayName ? "Goblin (Sword)" : "GoblinSword"; break;
			case 025:
				switch(auxpal) {
					case 0:	NPCName = "Lord"; break;
					case 1:	NPCName = IsDisplayName ? "Liche Wizard" : "LicheWizard"; break;
				}
				break;
			case 026:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Mage (Male/Yellow)" : "Mage_M_Y"; break;
					case 1:	NPCName = IsDisplayName ? "Mage (Male/Gray)" : "Mage_M_GY"; break;
				}
				break;
			case 027:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Mage (Female/Gray)" : "Mage_F_GY"; break;
					case 1:	NPCName = IsDisplayName ? "Mage (Female/Yellow)" : "Mage_F_Y"; break;
					case 2:	NPCName = IsDisplayName ? "Mage (Female/Red)" : "Mage_F_R"; break;
				}
				break;
			case 030:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Fighter (Male/Green)" : "Fighter_M_GN"; break;
					case 1:	NPCName = IsDisplayName ? "Fighter (Male/Purple)" : "Fighter_M_P"; break;
					case 2:	NPCName = IsDisplayName ? "Fighter (Male/Orange)" : "Fighter_M_O"; break;
				}
				break;
			case 031:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Fighter (Female/Green)" : "Fighter_F_GN"; break;
					case 1:	NPCName = IsDisplayName ? "Fighter (Female/Gray)" : "Fighter_F_GY"; break;
				}
				break;
			case 032:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Human (Male/Brown)" : "Human_M_B"; break;
					case 1:	NPCName = IsDisplayName ? "Human (Male/Gray)" : "Human_M_GY"; break;
					case 2:	NPCName = IsDisplayName ? "Human (Male/White)" : "Human_M_W"; break;
				}
				break;
			case 033:
				switch (auxpal) {
					case 0:	NPCName = IsDisplayName ? "Human (Female/Green)" : "Human_F_GN"; break;
					case 1:	NPCName = IsDisplayName ? "Human (Female/Gray)" : "Human_F_GY"; break;
				}
				break;
			case 036:
				if (auxpal == 0) {
					NPCName = IsDisplayName ? "Liche Warrior" : "LicheWarrior";
				}
				break;
			case 037: NPCName = IsDisplayName ? "Ethereal Void Creature" : "EVCreature"; break;
		}
	}

   return NPCName;
}

void CRITXtractUW1(
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

// Get palette 0 -- Auxillary palettes stored in critter page files and loaded later
	char palette[256 * 4];
	GetPalette32(UWPath, 0, palette, true);

// Read in game strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Create CSV export file and header
	sprintf(TempPath, "%s\\CRIT_Summary.csv", OutPath.c_str());
	FILE* CritSummary = fopen(TempPath, "w");
	fprintf(CritSummary, "PageFile,ItemID,Name,AnimationName,AnimationID,AuxPal,FolderName\n");

// Create animation log -- Note:  Leaving this until I get around to switching this data to a CSV format (may need to split it up a bit)
	sprintf(TempPath, "%s\\CRIT.log", OutPath.c_str());
	FILE* log = fopen(TempPath, "w");
	fprintf(log, "CRIT log file\n\n");

// Get animation and critter lists
	char critname[32][8] = {};
	unsigned char CritNPC[64][2] = {};
	{
		sprintf(TempPath, "%s\\CRIT\\ASSOC.ANM", UWPath.c_str());
		FILE* assoc = fopen(TempPath, "rb");

	// Get animation names
		fread(critname, 8, 32, assoc);
	// Get animation to critter mapping
		fread(CritNPC, 2, 64, assoc);

	// Export summary log and create output folders
		for (int i = 0; i < 64; i++) {
			unsigned int AnimID = CritNPC[i][0];
			unsigned int AuxPal = CritNPC[i][1];

		// For reasons that are unclear to me and I don't care to troubleshoot (have a guess but meh) the two names that are 8 characters long overflow so grabbing as string and substringing on output
			std::string AnimName = critname[CritNPC[i][0]];

			std::string DisplayName = NPCNameFix(CleanDisplayName(gs.get_string(4, i + 64).c_str(), true, false), false, AnimID, AuxPal, true);
			std::string FolderName = NPCNameFix(CleanDisplayName(gs.get_string(4, i + 64).c_str(), true, true), false, AnimID, AuxPal, false);

		// Sorta hacky workaround to hide PageFile/AnimID/FolderName for Adventurer -- Could just remove it but figured it better to show all 64 critter types
			char AnimIDText[2] = "";
			char PageFile[12] = "";
			char FolderSummary[64] = "";
			if (i < 63) {
				sprintf(AnimIDText, "%o", AnimID);
				sprintf(PageFile, "CR%02oPAGE.N0X", AnimID);
				sprintf(FolderSummary, "CR%02o_%02X_%s", AnimID, AuxPal, FolderName.c_str());
			}

			if (i == 63) {
				DisplayName = "Adventurer";	// Fix display name for player character in log -- Note:  Shares AnimID/AuxPal with Outcast
			}

			fprintf(
				CritSummary,
				"%s,"		// PageFile
				"%u,"		// ItemID
				"%s,"		// Name
				"%s,"		// AnimationName
				"%s,"		// AnimationID
				"%s,"		// AuxPal
				"%s\n",		// FolderName
				PageFile,										// PageFile
				i + 64,											// ItemID
				DisplayName.c_str(),							// Name
				i == 63 ? "" : AnimName.length() > 8 ? (AnimName.substr(0, 8)).c_str() : AnimName.c_str(),// AnimationName -- Note:  Extremely lazy trim of overflow
				AnimIDText,										// AnimationID
				i == 63 ? "" : std::to_string(AuxPal).c_str(),	// AuxPal
				FolderSummary									// FolderName
			);

		// Create critter output folder -- Skip Adventurer
			if (i < 63) {
				sprintf(TempPath, "%s\\CR%02o_%02X_%s", OutPath.c_str(), AnimID, AuxPal, FolderName.c_str());
				CreateFolder(TempPath);
			}
		}
		fclose(CritSummary);
		fclose(assoc);
	}

// Loop all NPCs/palette swaps
	for (int NPC = 0; NPC < 63; NPC++) {
		int crit = CritNPC[NPC][0];
		int TargetPal = CritNPC[NPC][1];

		std::string FolderName = NPCNameFix(CleanDisplayName(gs.get_string(4, NPC + 64).c_str(), true, true), false, crit, TargetPal, false);

	// Test first page file
		{
			char buffer[256];
			sprintf(buffer, "%s\\CRIT\\CR%02oPAGE.N%02o", UWPath.c_str(), crit, 0);

			FILE* pfile = fopen(buffer, "rb");
			if (pfile == NULL) {
				continue;
			}
			fclose(pfile);
		}

		fprintf(log, "Anim 0%02o: \"%-8.8s\"\n", crit, critname[crit]);

		long offsetTableOffsetUw2 = 0x80;

	// Read in all pages
		for (int page = 0;; page++) {
			char buffer[256];
			sprintf(buffer, "%s\\CRIT\\CR%02oPAGE.N%02o", UWPath.c_str(), crit, page);
			FILE* pfile = fopen(buffer, "rb");

			if (pfile == NULL) {
				break; // No more page files
			}

		// Read in slot list
			int slotbase = fgetc(pfile);
			int nslots = fgetc(pfile);

			if (nslots == 0) {
				fprintf(log, "%s: no slots in file\n", buffer);
				continue;
			}

			unsigned char* segoffsets = new unsigned char[nslots];
			fread(segoffsets, 1, nslots, pfile);

			int nsegs = fgetc(pfile);

			fprintf(log, "cr%02opage.n%02o\n SlotBase = %2u, NSlots = %3u, NSegs = %2u\n",
			crit, page, slotbase, nslots, nsegs);

		// Print slot list
			{
				fprintf(log, " Slot List:");

				for (int i = 0; i < nslots; i++) {
					if ((i & 15) == 0) {
						fprintf(log, "\n  %04X:", i + slotbase);
					}
					fprintf(log, " %02X", segoffsets[i]);
				}

				fprintf(log, "\n");
			}

		// Print segment list
			for (int i = 0; i < nsegs; i++) {
				fprintf(log, " Segment #%u:", i);

				for (int n = 0; n < 8; n++)	{
					fprintf(log, " %02X", fgetc(pfile));
				}
				fprintf(log, "\n");
			}

			int nauxpals = fgetc(pfile);
			fprintf(log, " Number of auxillery palettes: %u\n", nauxpals);

			unsigned char* auxpals = new unsigned char[32 * nauxpals];
			fread(auxpals, 32, nauxpals, pfile);

			int noffsets = fgetc(pfile);
			int unknown1 = fgetc(pfile);


			unsigned short* alloffsets = new unsigned short[noffsets];
			fread(alloffsets, 2, noffsets, pfile);

		// Remember offset for next page
			offsetTableOffsetUw2 = ftell(pfile);

		// Print all file offsets
			{
				fprintf(log, " NOffsets = %u, Unk1 = %02X", noffsets, unknown1);

				for (unsigned int i = 0; i < unsigned(noffsets); i++) {
					if ((i & 15) == 0) {
						fprintf(log, "\n ");
					}
					fprintf(log, " %04X", alloffsets[i]);

				}
				fprintf(log, "\n\n");
			}

			unsigned int maxleft, maxright, maxtop, maxbottom;
			maxleft = maxright = maxtop = maxbottom = 0;

		// Decode all frames
			if (nauxpals >= TargetPal) {
				for (int frame = 0; frame < noffsets; frame++) {
					fseek(pfile, alloffsets[frame], SEEK_SET);
					int width, height, hotx, hoty, type;

					width = fgetc(pfile);
					height = fgetc(pfile);
					hotx = fgetc(pfile);
					hoty = fgetc(pfile);
					type = fgetc(pfile);

					int wsize = 5;
					if (type != 6) {
						wsize = 4;
					}

					fprintf(log, " Frame #%02X, %ux%u, Hot:(%u,%u), Type=%u\n",
					frame, width, height, hotx, hoty, type);

					unsigned int left, right, top, bottom;

					right = width - hotx;
					left = hotx;
					top = hoty;
					bottom = height - hoty;

					if (right > maxright) {
						maxright = right;
					}
					if (left > maxleft) {
						maxleft = left;
					}
					if (top > maxtop) {
						maxtop = top;
					}
					if (bottom > maxbottom) {
						maxbottom = bottom;
					}

					unsigned short datalen;
					fread(&datalen, 1, 2, pfile);

					sprintf(TempPath, "%s\\CR%02o_%02X_%s", OutPath.c_str(), crit, TargetPal, FolderName.c_str());

					if ((std::filesystem::exists(TempPath))) {
						sprintf(buffer, "%s\\CR%02o_%02u_%02o_%02u.tga", TempPath, crit, TargetPal, page, frame);
						FILE* tga = fopen(buffer, "wb");

					// Write header
						TGAWriteHeader(tga, width, height);

					// Write palette
						fwrite(palette, 1, 256 * 4, tga);

					// Write data
						ImageDecode4BitRLE(pfile, tga, wsize, width * height, auxpals + (32 * (TargetPal)));
						fclose(tga);
					}
				}
			}

			fprintf(log, " X/Y range for all frames: %u x %u - "
			"new hot spot: (%u,%u)\n\n",
			maxleft + maxright, maxtop + maxbottom,
			maxleft, maxtop);

			delete[] alloffsets;
			delete[] auxpals;
			delete[] segoffsets;

			fclose(pfile);
		}
		fprintf(log, "\n");
	}
	fclose(log);
}

void CRITXtractUW2(
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

// Get palette 0 -- Auxillary palettes stored in critter page files and loaded later
	char palette[256 * 4];
	GetPalette32(UWPath, 0, palette, true);

// Read in game strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Create CSV export file and header
	sprintf(TempPath, "%s\\CRIT_Summary.csv", OutPath.c_str());
	FILE* CritSummary = fopen(TempPath, "w");
	fprintf(CritSummary, "PageFile,ItemID,Name,AnimationID,AuxPal,FolderName\n");

// Create animation log -- Note:  Leaving this until I get around to switching this data to a CSV format (may need to split it up a bit)
	sprintf(TempPath, "%s\\CRIT.log", OutPath.c_str());
	FILE* log = fopen(TempPath, "w");
	fprintf(log, "CRIT log file\n\n");

// Get animation and critter lists
	unsigned char CritNPC[64][2] = {};
	{
	// Get animation to critter mapping
		sprintf(TempPath, "%s\\CRIT\\AS.AN", UWPath.c_str());
		FILE* assoc = fopen(TempPath, "rb");
		fread(CritNPC, 2, 64, assoc);

	// Export summary log and create output folders
		for (int i = 0; i < 64; i++) {
			unsigned int AnimID = CritNPC[i][0];
			unsigned int AuxPal = CritNPC[i][1];

			std::string DisplayName = NPCNameFix(CleanDisplayName(gs.get_string(4, i + 64).c_str(), true, false), true, AnimID, AuxPal, true);
			std::string FolderName = NPCNameFix(CleanDisplayName(gs.get_string(4, i + 64).c_str(), true, true), true, AnimID, AuxPal, false);

		// Sorta hacky workaround to hide PageFile/AnimID/FolderName for Adventurer -- Could just remove it but figured it better to show all 64 critter types
			char AnimIDText[2] = "";
			char PageFile[12] = "";
			char FolderSummary[64] = "";
			if (i < 63) {
				sprintf(AnimIDText, "%o", AnimID);
				sprintf(PageFile, "CR%02oPAGE.N0X", AnimID);
				sprintf(FolderSummary, "CR%02o_%02X_%s", AnimID, AuxPal, FolderName.c_str());
			}

			fprintf(
				CritSummary,
				"%s,"				// PageFile
				"%u,"				// ItemID
				"%s,"				// Name
				"%s,"				// AnimationID
				"%s,"				// AuxPal
				"%s\n",	// FolderName
				PageFile,										// PageFile
				i + 64,											// ItemID
				DisplayName.c_str(),							// Name
				AnimIDText,										// AnimationID
				i == 63 ? "" : std::to_string(AuxPal).c_str(),	// AuxPal
				FolderSummary									// FolderName
			);

		// Create file output folder
			if (AnimID != 255) {   // Skip invalid "an_adventurer"
				sprintf(TempPath, "%s\\CR%02o_%02X_%s", OutPath.c_str(), AnimID, AuxPal, FolderName.c_str());
				CreateFolder(TempPath);
			}
		}
		fclose(CritSummary);
		fclose(assoc);
	}


// Read slots for all critters
	sprintf(TempPath, "%s\\CRIT\\CR.AN", UWPath.c_str());
	FILE* fd = fopen(TempPath, "rb");

	unsigned char slotListUw2[32][512];

	fread(slotListUw2, 1, sizeof(slotListUw2), fd);
	for (unsigned int critterIndex = 0; critterIndex < 32; critterIndex++) {
		fprintf(log, "%s\n", gs.get_string(4, critterIndex + 64).c_str());

		for (unsigned int j = 0; j < 512; j++) {
			fprintf(log, "%02X%c",
			slotListUw2[critterIndex][j], (j & 7) == 7 ? '\n' : ' ');
		}

		fprintf(log, "\n");
	}

	fclose(fd);

	sprintf(TempPath, "%s\\CRIT\\PG.MP", UWPath.c_str());
	FILE* pgmp = fopen(TempPath, "rb");

// Loop all NPCs/palette swaps
	for (int NPC = 0; NPC < 64; NPC++) {
		int crit = CritNPC[NPC][0];
		int TargetPal = CritNPC[NPC][1];

		std::string FolderName = NPCNameFix(CleanDisplayName(gs.get_string(4, NPC + 64).c_str(), true, true), true, crit, TargetPal, false);

		unsigned char pageMap[8] = {};
		fseek(pgmp, crit * 8, SEEK_SET);
		fread(pageMap, 1, 8, pgmp);

	// Check the page map
		if (pageMap[0] == 0xFF) {
			continue;
		}

		fprintf(log, "Crit %u Anim 0%02o:\n", NPC + 64, crit);

		long offsetTableOffsetUw2 = 0x80;

	// Read in all pages
		for (int page = 0;; page++) {
		// Check the page map
			if (pageMap[page] == 0xFF) {
				break;
			}

			char buffer[256];
			sprintf(buffer, "%s\\CRIT\\CR%02o.%02o", UWPath.c_str(), crit, page);
			FILE* pfile = fopen(buffer, "rb");

			if (pfile == NULL) {
				break; // No more page files
			}

			int noffsets = pageMap[page] + 1;

			if (page > 0) {
				noffsets -= pageMap[page - 1] + 1;
			}

			int nauxpals = 4;	// UW2 page files always have 4 auxpals

			unsigned char* auxpals = new unsigned char[32 * nauxpals];
			fread(auxpals, 32, nauxpals, pfile);
			fseek(pfile, offsetTableOffsetUw2, SEEK_SET);

			unsigned short* alloffsets = new unsigned short[noffsets];
			fread(alloffsets, 2, noffsets, pfile);

		// Remember offset for next page
			offsetTableOffsetUw2 = ftell(pfile);

		// Print all file offsets
			{
				fprintf(log, " NOffsets = %u", noffsets);

				for (unsigned int i = 0; i < unsigned(noffsets); i++) {
					if ((i & 15) == 0) {
						fprintf(log, "\n ");
					}
					fprintf(log, " %04X", alloffsets[i]);
				}
				fprintf(log, "\n\n");
			}

			unsigned int maxleft, maxright, maxtop, maxbottom;
			maxleft = maxright = maxtop = maxbottom = 0;

		// Decode all frames
			if (nauxpals >= TargetPal) {
				for (int frame = 0; frame < noffsets; frame++) {
					fseek(pfile, alloffsets[frame], SEEK_SET);
					int width, height, hotx, hoty, type;

					width = fgetc(pfile);
					height = fgetc(pfile);
					hotx = fgetc(pfile);
					hoty = fgetc(pfile);
					type = fgetc(pfile);

					int wsize = 5;
					if (type != 6) {
						wsize = 4;
					}

					fprintf(log, " Frame #%02X, %ux%u, Hot:(%u,%u), Type=%u\n",
					frame, width, height, hotx, hoty, type);

					unsigned int left, right, top, bottom;

					right = width - hotx;
					left = hotx;
					top = hoty;
					bottom = height - hoty;

					if (right > maxright) {
						maxright = right;
					}
					if (left > maxleft) {
						maxleft = left;
					}
					if (top > maxtop) {
						maxtop = top;
					}
					if (bottom > maxbottom) {
						maxbottom = bottom;
					}

					unsigned short datalen;
					fread(&datalen, 1, 2, pfile);

					sprintf(TempPath, "%s\\CR%02o_%02X_%s", OutPath.c_str(), crit, TargetPal, FolderName.c_str());

					char buffer[256];
					sprintf(buffer, "%s\\CR%02o_%02u_%02o_%02u.tga", TempPath, crit, TargetPal, page, frame);
					FILE* tga = fopen(buffer, "wb");

				// Write header
					TGAWriteHeader(tga, width, height);

				// Write palette
					fwrite(palette, 1, 256 * 4, tga);

				// Write data
					ImageDecode4BitRLE(pfile, tga, wsize, width * height, auxpals + (32 * (TargetPal)));
					fclose(tga);
				}
			}

			fprintf(log, " X/Y range for all frames: %u x %u - "
			"new hot spot: (%u,%u)\n\n",
			maxleft + maxright, maxtop + maxbottom,
			maxleft, maxtop);

			delete[] alloffsets;
			delete[] auxpals;

			fclose(pfile);
		}
		fprintf(log, "\n");
	}
	fclose(pgmp);
	fclose(log);
}

// Kind of a mess between the two so easier to just split them up
int CRITXtract(
	const bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
	CreateFolder(OutPath);

	if (!IsUW2) {
		CRITXtractUW1(UWPath, OutPath);
	}
	else {
		CRITXtractUW2(UWPath, OutPath);
	}

	printf("Critter animation frames extracted to %s\n", OutPath.c_str());
	return 0;
}
