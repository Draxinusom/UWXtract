/*************
	UWXtract - Ultima Underworld Extractor

	Model decoder/extractor

	Minimally modified version to work and normalize some output of the "hacking" uwmdldec function by the
	Underworld Adventures Team to extract 3D model details

	I may eventually look into this more and clean it up/flush it out more but am largely uninterested in this one so probably not
*************/
#include "UWXtract.h"

// Underworld models are (unfortunately) buried deep inside the executable (from TSSHP)
struct {
	unsigned long table_offset;
	unsigned long value;          /* 4 bytes at table_offset */
	unsigned long base_offset;
}
UW_model_offset[] = {
	{ 0x0004e910, 0x40064ab6, 0x0004e99e },
	{ 0x0004ccd0, 0x40064ab6, 0x0004cd5e }, /* same models, different place */
	{ 0x0004e370, 0x40064ab6, 0x0004e3fe }, /* ditto (reported Gerd Bitzer) */
	{ 0x00054cf0, 0x59aa64d4, 0x00054d8a },
	{ 0x000550e0, 0x59aa64d4, 0x0005517a }, /* another UW2 build            */
	{ 0, 0, 0 }
};

// Prototypes
void parse_node(
	FILE* fd,
	FILE* log
);
double read_float(
	FILE* fd
);

int MDLXtract(
	const bool IsUW2,
	const std::string UWPath,
	const std::string OutPath
) {
// Reusable variable for setting target file names
	char TempPath[255];

	CreateFolder(OutPath);

	sprintf(TempPath,"%s\\Models.txt", OutPath.c_str());
	FILE* log = fopen(TempPath, "w");

	if (!IsUW2) {
		sprintf(TempPath, "%s\\UW.EXE", UWPath.c_str());
	}
	else {
		sprintf(TempPath, "%s\\UW2.EXE", UWPath.c_str());
	}
	FILE* fd = fopen(TempPath, "rb");

	unsigned int base = 0;
	unsigned short mdloffsets[32];

// search for proper model offset
	{
		for (int i = 0; i < (sizeof(UW_model_offset) / sizeof(UW_model_offset[0])); i++) {
			fseek(fd, UW_model_offset[i].table_offset, SEEK_SET);

			unsigned int data;
			fread(&data, 1, 4, fd);
			if (data == UW_model_offset[i].value) {
				base = UW_model_offset[i].base_offset;
				fseek(fd, UW_model_offset[i].table_offset, SEEK_SET);
				break;
			}
		}
	}

	if (base != 0) {
	// read model offset list
		fread(mdloffsets, 32, 2, fd);

	// print model offsets
		{
			fprintf(log, "Model List:");
			for (int i = 0; i < 32; i++) {
				if ((i % 8) == 0) {
					fprintf(log, "\n ");
				}
				fprintf(log, " %04X", mdloffsets[i]);
			}
			fprintf(log, "\n\n");
		}

	// parse every model
		for (int i = 0; i < 32; i++) {
		// seek to model
			fseek(fd, base + mdloffsets[i], SEEK_SET);
			fprintf(log, "Model %02X at offset %08X\n",
				i, base + mdloffsets[i]);

		// read header
			unsigned int unk1;
			fread(&unk1, 1, 4, fd);

			double ext1, ext2, ext3;
			ext1 = read_float(fd);
			ext2 = read_float(fd);
			ext3 = read_float(fd);

			fprintf(log, " Unk1 = %08X\n Extents = (%3.2f; %3.2f; %3.2f)\n",
				unk1, ext1, ext2, ext3);

		// parse root node (and all others recursively)
			parse_node(fd, log);

			fprintf(log, "Model %02X End.\n\n", i);
		}
	}
	fclose(fd);
	fclose(log);

	printf("Models extracted to %s\n", OutPath.c_str());
	return 0;
}

unsigned int read_int16(
	FILE* fd
) {
	unsigned short data;
	fread(&data, 1, 2, fd);
	return data;
}

double read_float(
	FILE* fd
) {
	short data;
	fread(&data, 1, 2, fd);
	return static_cast<double>(data) / 16.0;
}

int read_vertno(
	FILE* fd
) {
	unsigned short data;
	fread(&data, 1, 2, fd);

	if ((data & 7) != 0) {
		printf("Read_VertNo: Remainder %u at divison\n", data & 7);
	}
	return data / 8;
}

#define M3_UW_SORT_PLANE    0x0006
#define M3_UW_SORT_PLANE_ZY 0x000C
#define M3_UW_SORT_PLANE_XY 0x000E
#define M3_UW_SORT_PLANE_XZ 0x0010
#define M3_UW_FACE_PLANE    0x0058         /* define face */
#define M3_UW_FACE_PLANE_ZY 0x005E
#define M3_UW_FACE_PLANE_XY 0x0060
#define M3_UW_FACE_PLANE_XZ 0x0062
#define M3_UW_FACE_PLANE_X  0x0064
#define M3_UW_FACE_PLANE_Z  0x0066
#define M3_UW_FACE_PLANE_Y  0x0068
#define M3_UW_ORIGIN        0x0078
#define M3_UW_VERTEX        0x007A
#define M3_UW_FACE_VERTICES 0x007E
#define M3_UW_VERTICES      0x0082
#define M3_UW_VERTEX_X      0x0086
#define M3_UW_VERTEX_Z      0x0088
#define M3_UW_VERTEX_Y      0x008A
#define M3_UW_VERTEX_ROOF   0x008C
#define M3_UW_VERTEX_XZ     0x0090
#define M3_UW_VERTEX_XY     0x0092
#define M3_UW_VERTEX_YZ     0x0094
#define M3_UW_TEXTURE_FACE  0x00A8
#define M3_UW_TMAP_VERTICES 0x00B4
#define M3_UW_FACE_SHADE    0x00BC
#define M3_UW_VERTEX_DARK   0x00D4
#define M3_UW_FACE_GOURAUD  0x00D6

void parse_node(
	FILE* fd,
	FILE* log
) {
   fprintf(log, "  StartNode\n");

   // read all nodes
   bool loop = true;
   do
   {
	  unsigned short cmd;
	  fread(&cmd, 1, 2, fd);

	  switch (cmd)
	  {
	  case 0:
		 fprintf(log, "  EndNode\n");
		 loop = false;
		 break;

	  case M3_UW_SORT_PLANE_ZY:
	  {
		 double sz, z, sy, y;

		 sz = read_float(fd);
		 z = read_float(fd);
		 sy = read_float(fd);
		 y = read_float(fd);

		 unsigned int left = read_int16(fd);
		 unsigned int left_ofs = left + ftell(fd);
		 unsigned int right = read_int16(fd);
		 unsigned int right_ofs = right + ftell(fd);

		 fprintf(log, "  SortNodeZY N = (0.0, %3.2f, %3.2f) X = (0.0, %3.2f, %3.2f)"
			" Left: %02X Right: %02X\n",
			sy, sz, y, z, left, right);

		 long pos = ftell(fd);

		 fprintf(log, "  LeftList Start\n");
		 fseek(fd, left_ofs, SEEK_SET);
		 parse_node(fd, log);
		 fprintf(log, "  LeftList End\n");

		 fprintf(log, "  RightList Start\n");
		 fseek(fd, right_ofs, SEEK_SET);
		 parse_node(fd, log);
		 fprintf(log, "  RightList End\n");

		 fseek(fd, pos, SEEK_SET);
	  }
	  break;

	  case M3_UW_SORT_PLANE_XY:
	  {
		 double sx, x, sy, y;

		 sx = read_float(fd);
		 x = read_float(fd);
		 sy = read_float(fd);
		 y = read_float(fd);

		 unsigned int left = read_int16(fd);
		 unsigned int left_ofs = left + ftell(fd);
		 unsigned int right = read_int16(fd);
		 unsigned int right_ofs = right + ftell(fd);

		 fprintf(log, "  SortNodeXY N = (%3.2f, %3.2f, 0.0) X = (%3.2f, %3.2f, 0.0)"
			" Left: %02X Right: %02X\n",
			sx, sy, x, y, left, right);

		 long pos = ftell(fd);

		 fprintf(log, "  LeftList Start\n");
		 fseek(fd, left_ofs, SEEK_SET);
		 parse_node(fd, log);
		 fprintf(log, "  LeftList End\n");

		 fprintf(log, "  RightList Start\n");
		 fseek(fd, right_ofs, SEEK_SET);
		 parse_node(fd, log);
		 fprintf(log, "  RightList End\n");

		 fseek(fd, pos, SEEK_SET);
	  }
	  break;

	  case M3_UW_SORT_PLANE_XZ:
	  {
		 double sx, x, sz, z;

		 sx = read_float(fd);
		 x = read_float(fd);
		 sz = read_float(fd);
		 z = read_float(fd);

		 unsigned int left = read_int16(fd);
		 unsigned int left_ofs = left + ftell(fd);
		 unsigned int right = read_int16(fd);
		 unsigned int right_ofs = right + ftell(fd);

		 fprintf(log, "  SortNodeXZ N = (%3.2f, 0.0, %3.2f) X = (%3.2f, 0.0, %3.2f)"
			" Left: %02X Right: %02X\n",
			sx, sz, x, z, left, right);

		 long pos = ftell(fd);

		 fprintf(log, "  LeftList Start\n");
		 fseek(fd, left_ofs, SEEK_SET);
		 parse_node(fd, log);
		 fprintf(log, "  LeftList End\n");

		 fprintf(log, "  RightList Start\n");
		 fseek(fd, right_ofs, SEEK_SET);
		 parse_node(fd, log);
		 fprintf(log, "  RightList End\n");

		 fseek(fd, pos, SEEK_SET);
	  }
	  break;

	  case 0x14: // don't know what this is, might have to do with colour */
	  {
		 unsigned int vertno, vertno2;
		 unsigned int unk1, unk2;
		 vertno = read_int16(fd);
		 unk1 = fgetc(fd);
		 unk2 = fgetc(fd);
		 vertno2 = read_int16(fd);

		 fprintf(log, "  Color Vertex[%u] = {%02X, %02X ??} => Vertex[%u] ??\n",
			vertno, unk1, unk2, vertno2);
	  }
	  break;

	  case 0x0040:
		 fprintf(log, "  Unk0040\n");
		 break;

	  case M3_UW_FACE_PLANE:
	  {
		 unsigned int face_len = read_int16(fd);
		 double sx, sy, sz, x, y, z;
		 sx = read_float(fd);
		 x = read_float(fd);
		 sz = read_float(fd);
		 z = read_float(fd);
		 sy = read_float(fd);
		 y = read_float(fd);

		 fprintf(log, "  FacePlane {Len = %04X} S = (%3.2f, %3.2f, %3.2f) X = (%3.2f, %3.2f, %3.2f)\n",
			face_len, sx, sy, sz, x, y, z);
	  }
	  break;

	  case M3_UW_FACE_PLANE_ZY:
		 fprintf(log, "  FacePlaneXZ\n");
		 fseek(fd, 2 + 2 + 2 + 2 + 2, SEEK_CUR);
		 break;

	  case M3_UW_FACE_PLANE_XY:
		 fprintf(log, "  FacePlaneXZ\n");
		 fseek(fd, 2 + 2 + 2 + 2 + 2, SEEK_CUR);
		 break;

	  case M3_UW_FACE_PLANE_XZ:
		 fprintf(log, "  FacePlaneXZ\n");
		 fseek(fd, 2 + 2 + 2 + 2 + 2, SEEK_CUR);
		 break;

	  case M3_UW_VERTEX:
	  {
		 double vx, vy, vz;
		 vx = read_float(fd);
		 vy = read_float(fd);
		 vz = read_float(fd);
		 int vertno = read_vertno(fd);

		 fprintf(log, "  Vertex[%u] = (%3.2f, %3.2f, %3.2f)\n",
			vertno, vx, vy, vz);
	  }
	  break;

	  case M3_UW_ORIGIN:
	  {
		 int vertno = read_vertno(fd);
		 fprintf(log, "  Origin Vertex[%u] {", vertno);

		 unsigned short test;
		 fread(&test, 1, 2, fd); fprintf(log, "Unk1 = %04X,", test);
		 fread(&test, 1, 2, fd); fprintf(log, " Unk2 = %04X,", test);
		 fread(&test, 1, 2, fd); fprintf(log, " Unk3 = %04X,", test);
		 fread(&test, 1, 2, fd); fprintf(log, " Unk4 = %04X}\n", test);
	  }
	  break;

	  case M3_UW_VERTICES:
	  {
		 unsigned int nvert, unk1;
		 nvert = read_int16(fd);
		 unk1 = read_int16(fd);

		 fprintf(log, "  VertexList {NVert = %u, Unk1 = %u}", nvert, unk1);

		 for (unsigned int j = 0; j < nvert; j++)
		 {
			double x, y, z;
			x = read_float(fd);
			y = read_float(fd);
			z = read_float(fd);
			fprintf(log, " (%3.2f, %3.2f, %3.2f), ", x, y, z);
		 }
		 fprintf(log, "\n");
	  }
	  break;

	  case M3_UW_VERTEX_X:
	  {
		 unsigned int vertno = read_vertno(fd);
		 double vert_offs = read_float(fd);
		 unsigned int vertno2 = read_vertno(fd);

		 fprintf(log, "  Vertex[%u].X += %3.2f => Vertex[%u]\n",
			vertno, vert_offs, vertno2);
	  }
	  break;

	  case M3_UW_VERTEX_Y:
	  {
		 unsigned int vertno = read_vertno(fd);
		 double vert_offs = read_float(fd);
		 unsigned int vertno2 = read_vertno(fd);

		 fprintf(log, "  Vertex[%u].Y += %3.2f => Vertex[%u]\n",
			vertno, vert_offs, vertno2);
	  }
	  break;

	  case M3_UW_VERTEX_Z:
	  {
		 unsigned int vertno = read_vertno(fd);
		 double vert_offs = read_float(fd);
		 unsigned int vertno2 = read_vertno(fd);

		 fprintf(log, "  Vertex[%u].Z += %3.2f => Vertex[%u]\n",
			vertno, vert_offs, vertno2);
	  }
	  break;

	  case M3_UW_VERTEX_ROOF:
	  {
		 // I have a very sneaky suspicion that this one refers to variable size
		 // things ... that pillar, for example
		 unsigned int vertno = read_vertno(fd);
		 unsigned int unk1 = fgetc(fd);
		 unsigned int unk2 = fgetc(fd);
		 unsigned int vertno3 = read_vertno(fd);

		 fprintf(log, "  Vertex[%u].Z = Ceiling, {%u, %u ??},  => Vertex[%u]\n",
			vertno, unk1, unk2, vertno3);
	  }
	  break;

	  case M3_UW_VERTEX_XY:   /* vertex offset X, Y        */
	  {
		 double offs_x, offs_y;
		 offs_x = read_float(fd);
		 offs_y = read_float(fd);

		 unsigned int vertno = read_vertno(fd);
		 unsigned int vertno2 = read_vertno(fd);

		 fprintf(log, "  Vertex[%u].X += %3.2f, [].Y += %3.2f => Vertex[%u]\n",
			vertno, offs_x, offs_y, vertno2);
	  }
	  break;

	  case M3_UW_VERTEX_XZ:
	  {
		 double offs_x, offs_z;
		 offs_x = read_float(fd);
		 offs_z = read_float(fd);

		 unsigned int vertno = read_vertno(fd);
		 unsigned int vertno2 = read_vertno(fd);

		 fprintf(log, "  Vertex[%u].X += %3.2f, [].Z += %3.2f => Vertex[%u]\n",
			vertno, offs_x, offs_z, vertno2);
	  }
	  break;

	  case M3_UW_VERTEX_YZ:
	  {
		 double offs_y, offs_z;
		 offs_y = read_float(fd);
		 offs_z = read_float(fd);

		 unsigned int vertno = read_vertno(fd);
		 unsigned int vertno2 = read_vertno(fd);

		 fprintf(log, "  Vertex[%u].Y += %3.2f, [].Z += %3.2f => Vertex[%u]\n",
			vertno, offs_y, offs_z, vertno2);
	  }
	  break;

	  case M3_UW_FACE_VERTICES:
	  {
		 unsigned int nverts = read_int16(fd), vertno;

		 fprintf(log, "  FaceVertices [%u] =", nverts);

		 for (unsigned int i = 0; i < nverts; i++)
		 {
			vertno = read_vertno(fd);
			fprintf(log, " %02X", vertno);
		 }
		 fprintf(log, "\n");
	  }
	  break;

	  case M3_UW_FACE_PLANE_X:
	  {
		 unsigned int face_len = read_int16(fd);
		 //in_face = true;
		 double x = read_float(fd);
		 double face_dist = read_float(fd);

		 fprintf(log, "  FacePlane Normal = (%3.2f, 0.0, 0.0) {Len = %u, Dist = %3.2f}\n",
			x, face_len, face_dist);
	  }
	  break;

	  case M3_UW_FACE_PLANE_Y:
	  {
		 unsigned int face_len = read_int16(fd);
		 //in_face = true;
		 double y = read_float(fd);
		 double face_dist = read_float(fd);

		 fprintf(log, "  FacePlane Normal = (0.0, %3.2f, 0.0) {Len = %u, Dist = %3.2f}\n",
			y, face_len, face_dist);
	  }
	  break;

	  case M3_UW_FACE_PLANE_Z:
	  {
		 unsigned int face_len = read_int16(fd);
		 //in_face = true;
		 double z = read_float(fd);
		 double face_dist = read_float(fd);

		 fprintf(log, "  FacePlane Normal = (0.0, 0.0, %3.2f) {Len = %u, Dist = %3.2f}\n",
			z, face_len, face_dist);
	  }
	  break;

	  case M3_UW_VERTEX_DARK:
	  {
		 unsigned int nvert, c1, c2, i;
		 nvert = read_int16(fd);
		 c1 = fgetc(fd);
		 c2 = fgetc(fd);

		 fprintf(log, "  VertexShading {NVert = %u, Unk1 = %u, Unk2 = %u}",
			nvert, c1, c2);

		 for (i = 0; i < nvert; ++i)
		 {
			unsigned int v = read_vertno(fd);
			unsigned int val = fgetc(fd);

			fprintf(log, " %02X", val);
		 }

		 if (nvert & 1)
			fgetc(fd);

		 //default_shade = c2;
		 //face_shade = default_shade;

		 fprintf(log, "\n");
	  }
	  break;

	  case M3_UW_FACE_GOURAUD:
	  {
		 fprintf(log, "  FaceGouraud\n");
		 /*
		 printf ("%02x Gouraud face\n", command);
		 if (!in_face) Cant_happen ("model.c:decode_uw_model(): not in face");
		 ts.flags |= M3F_SHADED;
		 face_length -= 2;*/

	  }
	  break;

	  case M3_UW_TMAP_VERTICES:
	  {
		 unsigned int nverts = read_int16(fd);
		 fprintf(log, "  TMapVertices [%u] =", nverts);

		 for (unsigned int i = 0; i < nverts; i++)
		 {
			unsigned int vertno = read_vertno(fd);
			signed int u, v;
			u = read_int16(fd);
			v = read_int16(fd);
			fprintf(log, " (%3.2f, %3.2f)",
			   u / 65536.0, u / 65536.0);
		 }
		 fprintf(log, "\n");
	  }
	  break;

	  case M3_UW_FACE_SHADE:
	  {
		 unsigned int c1 = fgetc(fd);
		 unsigned int c2 = fgetc(fd);
		 unsigned int c3 = fgetc(fd);
		 unsigned unk1 = fgetc(fd);
		 //face_shade = c1;

		 fprintf(log, "  FaceShading {C1 = %02X, C2 = %02X, C3 = %02X, Unk1 = %02X}\n",
			c1, c2, c3, unk1);
		 //if (in_face) face_length -= 6;
	  }
	  break;

	  default:
		 fprintf(log, "!! Encountered unknown command %04X\n", cmd);
		 loop = false;
		 break;
	  }

   } while (loop);
}
