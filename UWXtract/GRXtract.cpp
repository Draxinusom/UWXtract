/*************
	UWXtract - Ultima Underworld Extractor

	GR (Objects/UI) image extractor

	Heavily modified version of the "hacking" grdecode function by the
	Underworld Adventures Team to extract images from GR files
*************/
#include "UWXtract.h"

extern void ImageDecode4BitRLE(FILE* fd, FILE* out, unsigned int bits, unsigned int datalen, unsigned char* auxpalidx);	// CRITXtract.cpp
extern void GetPalette(const std::string UWPath, const unsigned int PaletteIndex, char PaletteBuffer[256 * 4]);			// Util.cpp

void ImageDecode4BitUncompressed(
	int datalen,
	FILE* fd,
	FILE* tga,
	unsigned char auxpalidx[16]
) {
	int nib_avail = 0;
	int rbyte;
	int nibble;

	while (datalen >= 0) {
	// Get new byte
		if (nib_avail == 0) {
			rbyte = fgetc(fd);
			nib_avail = 2;
			datalen--;
		}

	// Get next nibble
		nibble = nib_avail == 2 ? rbyte >> (4 * (nib_avail - 1)) : rbyte & 0x0f;
		nib_avail--;

		fputc(auxpalidx[nibble], tga);
	}
}

int GRXtract(
	const bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[256];

	CreateFolder(OutPath);

// Load game strings
	ua_gamestrings gs;
	gs.load((UWPath + "\\DATA\\STRINGS.PAK").c_str());

// Create CSV export file and header
   sprintf(TempPath, "%s\\GR.csv", OutPath.c_str());
   FILE* log = fopen(TempPath, "w");
   fprintf(log, "FileName,Index,Width,Height,Type,Palette,Description\n");

   char fname[256];

// Get palette 0
	char palette[4][256 * 4];
	GetPalette(UWPath, 0, palette[0]);
// Also get 1-3, there's a few GR files that use them
	GetPalette(UWPath, 1, palette[1]);
	GetPalette(UWPath, 2, palette[2]);
	GetPalette(UWPath, 3, palette[3]);

// Get all AuxPals
	unsigned char auxpalidx[32][16];
	{
		sprintf(TempPath, "%s\\DATA\\ALLPALS.DAT", UWPath.c_str());
		FILE* pal = fopen(TempPath, "rb");

		fread(auxpalidx, 1, 32 * 16, pal);
		fclose(pal);
	}

	_finddata_t find;
	intptr_t hnd = _findfirst((UWPath + "\\DATA\\*.GR").c_str(), &find);

	if (hnd == -1) {
		printf("No GR files found!\n");
		return -1;
	}

	do {
	// get basename
		char basename[64];
		strcpy(basename, find.name);

		char* pos = strrchr(basename, '.');
		if (pos != NULL) {
			*pos = 0;
		}

	// Set output file name
		sprintf(fname, "%s\\DATA\\%s.GR", UWPath.c_str(), basename);
		FILE* fd = fopen(fname, "rb");

	// Flag if GENHEAD.GR & UW2 - it's obviously a leftover file and UW2's palette is wrong
		bool SkipFile = false;
		if (IsUW2 && _stricmp("genhead", basename) == 0) {
			SkipFile = true;
		// Still log it so reason it's missing is known
			fprintf(log,
				"%s,"	// FileName
				",,,Leftover file from UW1,,Skipped\n",	// All other columns
				basename	// FileName
			);
		}

	// Create file output folder
		sprintf(TempPath, "%s\\%s", OutPath.c_str(), basename);
		CreateFolder(TempPath);

		fseek(fd, 0, SEEK_END);
		unsigned long flen = ftell(fd);
		fseek(fd, 1, SEEK_SET);	// Skip file format byte (1=GR)

		unsigned short entries;
		fread(&entries, 1, 2, fd);

	// read in all offsets
		unsigned int* offsets = new unsigned int[entries];
		fread(offsets, sizeof(unsigned int), entries, fd);

		for (int img = 0; img < entries && !SkipFile; img++) {
		// Skip if invalid
			if (offsets[img] >= flen) {
				fprintf(
					log,
					"%s,"	// FileName
					"%u,"	// Index
					",,Invalid Offset,,\n",	// All other columns
					basename,	// FileName
					img			// Index
				);
				continue;
			}

		// Image slot is unused if offset points to same as next slot
			if (offsets[img] == offsets[img + 1]) {
				fprintf(
					log,
					"%s,"	// FileName
					"%u,"	// Index
					",,Unused,,\n",	// All other columns
					basename,	// FileName
					img			// Index
				);
				continue;
			}

		// Seek to image offset
			fseek(fd, offsets[img], SEEK_SET);

			int type, width, height;

		// Hardcode type and dimensions for PANELS.GR
			if (_stricmp("panels", basename) == 0) {
				type = 4;
				if (!IsUW2) {
					if (img == 3) {
						width = 3;
						height = 120;
					}
					else {
						width = 83;
						height = 114;
					}
				}
				else {
					if (img == 3) {
						width = 3;
						height = 112;
					}
					else {
						width = 79;
						height = 112;
					}
				}
			}
		// Otherwise read from header
			else {
				type = fgetc(fd);
				width = fgetc(fd);
				height = fgetc(fd);
			}

			int auxpal = 0;

		// Set the base/primary palette -- Mostly 0
			int BasePaletteID = 0;
			if (!IsUW2) {
				if (_stricmp("opbtn", basename) == 0) {
					BasePaletteID = 2;
				}
				else if (_stricmp("chrbtns", basename) == 0) {
					BasePaletteID = 3;
				}
			}
			else {
				if (_stricmp("gempt", basename) == 0) {
					BasePaletteID = 1;
				}
				else if (_stricmp("cursors", basename) == 0 && img > 11 && img < 15) { // Automap cursor/button use palette 1
					BasePaletteID = 1;
				}
			}

		// Set image type name for log and get auxillary palette (where applicable)
			const char* TypeName;
			switch (type) {
				case 0x4:
				TypeName = "8-bit uncompressed";
				break;
			case 0x8:
				auxpal = fgetc(fd);
				TypeName = "4-bit run-length";
				break;
			case 0xa:
				auxpal = fgetc(fd);
				TypeName = "4-bit uncompressed";
				break;
			default:
				TypeName = "Unknown";
			}

		// Skip if invalid AuxPal
			if (auxpal > 0x1f) {
				fprintf(
					log,
					"%s,"	// FileName
					"%u,"	// Index
					",,,,Invalid auxilliary palette\n",	// All other columns
					basename,	// FileName
					img			// Index
				);
				continue;
			}

		// Set lenth of image data -- Hardcode for PANELS.GR
			unsigned short datalen;
			if (_stricmp("panels", basename) == 0) {
				datalen = width * height;
			}
			else {
				fread(&datalen, 1, 2, fd);
			}

		// Get item name/description
			std::string ImageDescription = "";
			std::string FileNameDescription = "";
		// ARMOR_F/M.GR
			if (_stricmp("armor_f", basename) == 0 || _stricmp("armor_m", basename) == 0) {
				unsigned short int DescriptionOffset = 0;	// ARMOR_X loops to show wear so use this to fix the string pointer
				std::string QualityDesc = "";
				std::string QualityFile = "";

				if (img < 15) {
					DescriptionOffset = 0;
					QualityDesc = "Badly Worn/";
					QualityFile = "_BadlyWorn";
				}
				else if (img < 30) {
					DescriptionOffset = 15;
					QualityDesc = "Worn/";
					QualityFile = "_Worn";
				}
				else if (img < 45) {
					DescriptionOffset = 30;
					QualityDesc = "Servicable/";
					QualityFile = "_Servicable";
				}
				else if (img < 60) {
					DescriptionOffset = 45;
					QualityDesc = "Excellent/";
					QualityFile = "_Excellent";
				}
				else {
					DescriptionOffset = 45;
				}

				ImageDescription = CleanDisplayName(gs.get_string(4, 32 + img - DescriptionOffset), true, false);
				FileNameDescription = "_" + CleanDisplayName(gs.get_string(4, 32 + img - DescriptionOffset), true, true) + QualityFile;

				if (_stricmp("armor_f", basename) == 0) {
					ImageDescription = ImageDescription + " (" + QualityDesc + "Female)";
				}
				else {
					ImageDescription = ImageDescription + " (" + QualityDesc + "Male)";
				}
			}
		// BODIES.GR
			else if (_stricmp("bodies", basename) == 0) {
				ImageDescription = std::string(img < 5 ? "Male" : "Female") + " Paperdoll " + std::to_string(img < 5 ? img + 1 : img - 4);
				FileNameDescription = "_Paperdoll_" + std::string(img < 5 ? "M" : "F") + std::to_string(img < 5 ? img + 1 : img - 4);
			}
		// CHRBTNS.GR
			else if (_stricmp("chrbtns", basename) == 0 && img > 6) {	// Only label heads/paperdolls
				if (img < 12) {
					ImageDescription = "Male Character Creation Portrait " + std::to_string(img - 6);
					FileNameDescription = "_CharCreatePortrait_M" + std::to_string(img - 6);
				}
				else if (img < 17) {
					ImageDescription = "Female Character Creation Portrait " + std::to_string(img - 11);
					FileNameDescription = "_CharCreatePortrait_F" + std::to_string(img - 11);
				}
				else if (img < 22) {
					ImageDescription = "Male Character Creation Paperdoll " + std::to_string(img - 16);
					FileNameDescription = "_CharCreatePaperdoll_M" + std::to_string(img - 16);
				}
				else {
					ImageDescription = "Female Character Creation Paperdoll " + std::to_string(img - 21);
					FileNameDescription = "_CharCreatePaperdoll_F" + std::to_string(img - 21);
				}
			}
		// CHARHEAD.GR
			else if (_stricmp("charhead", basename) == 0) {
				ImageDescription = CleanDisplayName(gs.get_string(7, 17 + img), true, false);
				FileNameDescription = "_" + CleanDisplayName(gs.get_string(7, 17 + img), true, true);
				if (ImageDescription == "") {
				// All of UW1 _should_ hit
					if (!IsUW2) {
						ImageDescription = "Unknown";
						FileNameDescription = "_Unknown";
					}
					if (IsUW2) {
						switch (img) {
							case  58:	ImageDescription = "Trilkhai"; FileNameDescription = "_Trilkhai"; break;
							case  82:	ImageDescription = "Flip"; FileNameDescription = "_Flip"; break;
							case 106:	ImageDescription = "Soldier"; FileNameDescription = "_Soldier"; break;
							case 107:	ImageDescription = "Soldier"; FileNameDescription = "_Soldier"; break;
							case 143:	ImageDescription = "Moglop Goblin"; FileNameDescription = "_MoglopGoblin"; break;
							default:	ImageDescription = "Unknown"; FileNameDescription = "_Unknown";
						}
					}
				}
			}
		// GENHEAD.GR -- UW1
			else if (_stricmp("genhead", basename) == 0) {
				switch (img) {
					case  6:	ImageDescription = "Goblin (Club/Green/Brown)"; FileNameDescription = "_GoblinClub_GN_B"; break;
					case  7:	ImageDescription = "Goblin (Sword/Green/Brown)"; FileNameDescription = "_GoblinSword_GN_B"; break;
					case 12:	ImageDescription = "Goblin (Club/Gray/Green)"; FileNameDescription = "_GoblinClub_GY_GN"; break;
					case 14:	ImageDescription = "Goblin (Club/Gray/Brown)"; FileNameDescription = "_GoblinClub_GY_B"; break;
					case 16:	ImageDescription = "Goblin (Sword/Gray/Green)"; FileNameDescription = "_GoblinSword_GY_GN"; break;
					case 20:	ImageDescription = "Mountainman (Red/Green)"; FileNameDescription = "_Mountainman_R_G"; break;
					case 22:	ImageDescription = "Mountainman (White/Brown)"; FileNameDescription = "_Mountainman_W_B"; break;
					case 29:	ImageDescription = "Fighter (Female/White/Blue)"; FileNameDescription = "_Fighter_F_W_B"; break;
					case 30:	ImageDescription = "Fighter (Female/Black/Red)"; FileNameDescription = "_Fighter_F_B_R"; break;
					case 31:	ImageDescription = "Fighter (Male/Black/Green)"; FileNameDescription = "_Fighter_M_B_GN"; break;
					case 34:	ImageDescription = "Fighter (Male/White/Red)"; FileNameDescription = "_Fighter_M_W_R"; break;
					case 35:	ImageDescription = "Ghoul (Green/Brown)"; FileNameDescription = "_Ghoul_GN_B"; break;
					case 39:	ImageDescription = "Mage (Male/White/Yellow)"; FileNameDescription = "_Mage_M_W_Y"; break;
					case 40:	ImageDescription = "Fighter (Male/White/Brown)"; FileNameDescription = "_Fighter_M_W_B"; break;
					case 42:	ImageDescription = "Mage (Staff/Male/White/Blue)"; FileNameDescription = "_MageStaff_M_W_B"; break;
					case 43:	ImageDescription = "Mage (Female/White/Blue)"; FileNameDescription = "_Mage_F_W_B"; break;
					case 44:	ImageDescription = "Mage (Male/White/Red)"; FileNameDescription = "_Mage_M_W_R"; break;
					case 45:	ImageDescription = "Mage (Male/Black/Yellow)"; FileNameDescription = "_Mage_M_B_Y"; break;
					case 46:	ImageDescription = "Ghoul (Red/Gray)"; FileNameDescription = "_Ghoul_R_GY"; break;
					case 51:	ImageDescription = "Mage (Staff/Male/White/Red)"; FileNameDescription = "_MageStaff_M_W_R"; break;
					case 59:	ImageDescription = "Tyball"; FileNameDescription = "_Tyball"; break;
					default:	ImageDescription = CleanDisplayName(gs.get_string(4, 64 + img), true, false); FileNameDescription = "_" + CleanDisplayName(gs.get_string(4, 64 + img), true, true);
				}
			}
		// GHED.GR -- UW2
			else if (_stricmp("ghed", basename) == 0) {
				switch (img) {
					case  0:	ImageDescription = "Guard (Undercover)"; FileNameDescription = "_Undercover"; break;
					case  9:	ImageDescription = "Goblin (Club)"; FileNameDescription = "_GoblinClub"; break;
					case 10:	ImageDescription = "Goblin (Sword)"; FileNameDescription = "_GoblinSword"; break;
					case 46:	ImageDescription = "Fighter (Male/Purple)"; FileNameDescription = "_Fighter_M_P"; break;
					case 48:	ImageDescription = "Human (Male/Brown)"; FileNameDescription = "_Human_M_B"; break;
					case 50:	ImageDescription = "Human (Male/Gray)"; FileNameDescription = "_Human_M_GY"; break;
					case 53:	ImageDescription = "Mage (Male/Gray)"; FileNameDescription = "_Mage_M_GY"; break;
					case 54:	ImageDescription = "Mage (Female/Gray)"; FileNameDescription = "_Mage_F_GY"; break;
					case 55:	ImageDescription = "Mage (Female/Red)"; FileNameDescription = "_Mage_F_R"; break;
					case 56:	ImageDescription = "Fighter (Female/Green)"; FileNameDescription = "_Fighter_F_GN"; break;
					case 57:	ImageDescription = "Fighter (Male/Green)"; FileNameDescription = "_Fighter_M_GN"; break;
					case 59:	ImageDescription = "Fighter (Female/Gray)"; FileNameDescription = "_Fighter_F_GY"; break;
					default:	ImageDescription = CleanDisplayName(gs.get_string(4, 64 + img), true, false); FileNameDescription = "_" + CleanDisplayName(gs.get_string(4, 64 + img), true, true);
				}
			}
		// HEADS.GR
			else if (_stricmp("heads", basename) == 0) {
				ImageDescription = std::string(img < 5 ? "Male" : "Female") + " Portrait " + std::to_string(img < 5 ? img + 1 : img - 4);
				FileNameDescription = "_Portrait_" + std::string(img < 5 ? "M" : "F") + std::to_string(img < 5 ? img + 1 : img - 4);
			}
		// OBJECTS.GR
			else if (_stricmp("objects", basename) == 0) {
				ImageDescription = CleanDisplayName(gs.get_string(4, img), true, false);
				FileNameDescription = "_" + CleanDisplayName(gs.get_string(4, img), true, true);
				if (ImageDescription == "") {
					ImageDescription = "Unknown";
					FileNameDescription = "Unknown";
				}
			}
		// SPELLS.GR
			else if (_stricmp("spells", basename) == 0) {
				ImageDescription = CleanDisplayName(gs.get_string(6, 384 + img), true, false);
				FileNameDescription = "_" + CleanDisplayName(gs.get_string(6, 384 + img), true, true);
				if (ImageDescription == "") {
					ImageDescription = "Unknown";
					FileNameDescription = "Unknown";
				}
			}
		// WEAPONS.GR -- UW1
			else if (_stricmp("weapons", basename) == 0) {
				std::string WeaponName;
				std::string AttackType;
			// Sword
				if (img <= 27 || (img >= 112 && img <= 139)) {
					WeaponName = "Sword";
					if (img <= 8 || (img >= 112 && img <= 120)) {
						AttackType = "Slash";
					}
					else if (img <= 17 || (img >= 112 && img <= 129)) {
						AttackType = "Bash";
					}
					else if (img <= 26 || (img >= 112 && img <= 138)) {
						AttackType = "Stab";
					}
					else {
						AttackType = "Ready";
					}
				}
			// Axe
				else if ((img >= 28 && img <= 55) || (img >= 140 && img <= 167)) {
					WeaponName = "Axe";
					if (img <= 36 || (img >= 140 && img <= 148)) {
						AttackType = "Slash";
					}
					else if (img <= 45 || (img >= 140 && img <= 157)) {
						AttackType = "Bash";
					}
					else if (img <= 54 || (img >= 140 && img <= 166)) {
						AttackType = "Stab";
					}
					else {
						AttackType = "Ready";
					}
				}
			// Mace
				else if ((img >= 56 && img <= 83) || (img >= 168 && img <= 195)) {
					WeaponName = "Mace";
					if (img <= 64 || (img >= 168 && img <= 176)) {
						AttackType = "Slash";
					}
					else if (img <= 73 || (img >= 168 && img <= 185)) {
						AttackType = "Bash";
					}
					else if (img <= 82 || (img >= 168 && img <= 194)) {
						AttackType = "Stab";
					}
					else {
						AttackType = "Ready";
					}
				}
			// Unarmed
				else {
					WeaponName = "Unarmed";
					if (img <= 92 || (img >= 196 && img <= 204)) {
						AttackType = "Slash";
					}
					else if (img <= 101 || (img >= 196 && img <= 213)) {
						AttackType = "Bash";
					}
					else if (img <= 110 || (img >= 196 && img <= 222)) {
						AttackType = "Stab";
					}
					else {
						AttackType = "Ready";
					}
				}

				ImageDescription = WeaponName + " " + AttackType + " (" + (img < 112 ? "Right)" : "Left)");
				FileNameDescription = "_" + WeaponName + "_" + AttackType + "_" + (img < 112 ? "Right" : "Left");
			}
		// WEAP.GR -- UW2
			else if (_stricmp("weap", basename) == 0) {
				std::string WeaponName;
				std::string AttackType;

			// Sword
				if (img <= 30 || (img >= 124 && img <= 154)) {
					WeaponName = "Sword";
					if (img <= 8 || (img >= 124 && img <= 132)) {
						AttackType = "Slash";
					}
					else if (img <= 17 || (img >= 124 && img <= 141)) {
						AttackType = "Bash";
					}
					else if (img == 27 || img == 151) {
						AttackType = "Ready";
					}
					else {
						AttackType = "Stab";
					}
				}
			// Axe
				else if ((img >= 31 && img <= 61) || (img >= 155 && img <= 185)) {
					WeaponName = "Axe";
					if (img <= 39 || (img >= 155 && img <= 163)) {
						AttackType = "Slash";
					}
					else if (img <= 48 || (img >= 155 && img <= 172)) {
						AttackType = "Bash";
					}
					else if (img == 58 || img == 182) {
						AttackType = "Ready";
					}
					else {
						AttackType = "Stab";
					}
				}
			// Mace
				else if ((img >= 62 && img <= 92) || (img >= 186 && img <= 216)) {
					WeaponName = "Mace";
					if (img <= 70 || (img >= 186 && img <= 194)) {
						AttackType = "Slash";
					}
					else if (img <= 79 || (img >= 186 && img <= 203)) {
						AttackType = "Bash";
					}
					else if (img == 89 || img == 213) {
						AttackType = "Ready";
					}
					else {
						AttackType = "Stab";
					}
				}
			// Unarmed
				else {
					WeaponName = "Unarmed";
					if (img == 102 || img == 226) {
						AttackType = "Ready";
					}
					else {
						AttackType = "Bash";	// They're sitting partially in the slash and bash range, though spaced like stab, but I _think_ unarmed is considered bash damage (should be if it isn't :P)
					}
				}

				ImageDescription = WeaponName + " " + AttackType + " (" + (img < 124 ? "Right)" : "Left)");
				FileNameDescription = "_" + WeaponName + "_" + AttackType + "_" + (img < 124 ? "R" : "L");
			}

		// Export to CSV
			fprintf(
				log,
				"%s,"		// FileName
				"%u,"		// Index
				"%u,"		// Width
				"%u,"		// Height
				"%s,"		// Type
				"%u,"		// Palette
				"%s\n",		// Description
				basename,							// FileName
				img,								// Index
				width,								// Width
				height,								// Height
				TypeName,							// Type
				type == 4 ? BasePaletteID : auxpal,	// Palette
				ImageDescription.c_str()			// Description
			);

		// Set file name - Add description to name where exists
			sprintf(fname, "%s\\%s\\%03u%s.tga", OutPath.c_str(), basename, img, FileNameDescription.c_str());
			FILE* tga = fopen(fname, "wb");

		// Write header
			TGAWriteHeader(tga, width, height);

		// Write palette
			fwrite(palette[BasePaletteID], 1, 256 * 4, tga);

		// Write image data
			switch (type) {
			// 8-bit uncompressed
				case 4:
					{
						char buffer[1024];
						while (datalen > 0) {
							int size = datalen > 1024 ? 1024 : datalen;
							fread(buffer, 1, size, fd);
							fwrite(buffer, 1, size, tga);
							datalen -= size;
						}
					}
					break;
			// 4-bit uncompressed
				case 0xa:
					ImageDecode4BitUncompressed(datalen, fd, tga, auxpalidx[auxpal]);
					break;
			// 4-bit run-length
				case 8:
					ImageDecode4BitRLE(fd, tga, 4, width * height, auxpalidx[auxpal]);
					break;
			 }

			 fclose(tga);
		}
		delete[] offsets;
		fclose(fd);
	} while (0 == _findnext(hnd, &find));
	_findclose(hnd);

	printf("GR images extracted to %s\n", OutPath.c_str());
	return 0;
}
