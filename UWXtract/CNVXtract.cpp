/*************
	UWXtract - Ultima Underworld Extractor

	CNV.ARK decoder/extractor header

	Minimally modified version of the "hacking" cnvdec function by the
	Underworld Adventures Team to extract conversation data

	Don't think is useful so never bothered getting path variable working as I'm leaving it out
	May replace at some point
*************/
#include "UWXtract.h"

static const char* fname = "DATA\\CNV.ARK";

void dump_import_funcs(FILE* fd, FILE* dest)
{
   // read number of imported funcs
   unsigned short funcs;
   fread(&funcs,1,2,fd);

   fprintf(dest,"; imported functions list, %u entries:\n;\n",funcs);

   for(int i=0; i<funcs; i++)
   {
      // length of function name
      unsigned short fname_len;
      fread(&fname_len,1,2,fd);

      std::string funcname;
      funcname.resize(fname_len + 1);
      fread(funcname.data(), 1, fname_len, fd);

      // function ID
      unsigned short func_id;
      fread(&func_id,1,2,fd);

      unsigned short unk1;
      fread(&unk1,1,2,fd);

      unsigned short import_type;
      fread(&import_type,1,2,fd);

      unsigned short ret_type;
      fread(&ret_type,1,2,fd);

      if (import_type == 0x010F)
      {
         fprintf(dest,"; var mem=%04x (ret_type=%04x, unk1=%04x) name=\"%s\" \n",
            func_id, ret_type, unk1, funcname.c_str());
      }
      else
      if (import_type == 0x0111)
      {
         fprintf(dest,"; func id=%04x ret_type=%s (unk1=%04x) name=\"%s\"\n",
            func_id,
            ret_type==0 ? "void" : ret_type==0x0129? "int" :
               ret_type==0x012b ? "string" : "unknown",
            unk1, funcname.c_str());
      }
      else
      {
         fprintf(dest,"; unknown import record: import type=%04x\n",import_type);
      }
   }

   fprintf(dest,";\n\n");
}

// instruction list
struct
{
   const char *mnemonic;
   int args;
   const char *argcode;
   const char *comment;
} instructions[0x2a] =
{
   { "NOP", 0,  "", "" },
   { "OPADD", 0, "", "pushes s[0]+s[1]" },
   { "OPMUL", 0, "", "pushes s[0]*s[1]" },
   { "OPSUB", 0, "", "pushes s[1]-s[0]" },
   { "OPDIV", 0, "", "pushes s[1]/s[0]" },
   { "OPMOD", 0, "", "pushes s[1]%s[0]" },
   { "OPOR", 0, "", "pushes s[1] || s[0]" },
   { "OPAND", 0, "", "pushes s[1] && s[0]" },
   { "OPNOT", 0, "", "pushes !s[0]" },
   { "TSTGT", 0, "", "" },
   { "TSTGE", 0, "", "" },
   { "TSTLT", 0, "", "" },
   { "TSTLE", 0, "", "" },
   { "TSTEQ", 0, "", "" },
   { "TSTNE", 0, "", "" },
   { "JMP", 1, " $%04x", "absolute jump" },
   { "BEQ", 1, " +%04x", "branch when s[0]==0" },
   { "BNE", 1, " +%04x", "branch when s[0]!=0" },
   { "BRA", 1, " +%04x", "branch always" },
   { "CALL", 1, " $%04x", "call subroutine" },
   { "CALLI", 1, " $%04x", "call imported subroutine" },
   { "RET", 0, "", "return; s[0] is the next instruction pointer\n" },
   { "PUSHI", 1, " #%04x", "push immediate value" },
   { "PUSHI_EFF", 1, " $%04x", "" },
   { "POP", 0, "", "" },
   { "SWAP", 0, "", "" },
   { "PUSHBP", 0, "", "" },
   { "POPBP", 0, "", "" },
   { "SPTOBP", 0, "", "" },
   { "BPTOSP", 0, "", "" },
   { "ADDSP", 0, "", "" },
   { "FETCHM", 0, "", "" },
   { "STO", 0, "", "" },
   { "OFFSET", 0, "", "" },
   { "START", 0, "", "start program" },
   { "SAVE_REG", 0, "", "" },
   { "PUSH_REG", 0, "", "" },
   { "STRCMP", 0, "", "string compare (??)" },
   { "EXIT_OP", 0, "", "end program (?)" },
   { "SAY_OP", 0, "", "NPC says string s[0]" },
   { "RESPOND_OP", 0, "", "respond (?)" },
   { "OPNEG", 0, "", "negate, s[0] = -s[0]" },
};

void decode_instructions(unsigned short* allcodes, FILE* dest,unsigned int codesize)
{
   fprintf(dest,";\n; code section\n;\n");
   for(unsigned int i=0; i<codesize; i++)
   {
      unsigned short code = allcodes[i];
      if (code>0x29)
      {
         fprintf(dest,"; !!! unknown code %04x encountered! !!!\n",code);
         continue; // try to continue
      }

      // output memory adress and code words
      fprintf(dest,"%04x\t%04x",i,code);
      if (instructions[code].args>0)
      {
         for(int n=0; n<instructions[code].args; n++)
            fprintf(dest," %04x",allcodes[i+n+1]);
      }
      else
         fprintf(dest,"\t");

      // output mnemonic and possible operand
      int arg = allcodes[i+1];
      fprintf(dest,"\t%s",instructions[code].mnemonic);
      fprintf(dest,instructions[code].argcode,arg);

      if (instructions[code].args==0)
         fprintf(dest,"\t");

      // output comment
      fprintf(dest,"\t\t; %s\n",instructions[code].comment);

      i += instructions[code].args;
   }
}


int cnvdec_main()
{
	CreateFolder(".\\UWXtract");

   FILE* fd = fopen(fname,"rb");

   if (fd==NULL)
   {
      printf("Could not open file %s\n",fname);
      return 1;
   }

   // open destination file
 //  FILE* dest = fopen("uw1-cnvdec.txt","w");
   FILE* dest = fopen(".\\UWXtract\\CNV_ARK.txt", "w");
   fprintf(dest,";\n; decoded conversations\n");

   // read number of entries
   unsigned short entries;
   fread(&entries,1,2,fd);

   fprintf(dest,";%u conversations.\n;\n\n",entries);

   // read offsets
   unsigned int *offsets = new unsigned int[entries];
   fread(offsets,4,entries,fd);

   unsigned int convcount = 0;

   for(int i=0; i<entries; i++)
   {
      if (offsets[i]==0)
      {
//         fprintf(dest,"; conversation #%u: n/a\n\n",i);
         continue;
      }

      fseek(fd,offsets[i],SEEK_SET);

//#ifdef HAVE_UW2
      if (std::filesystem::exists(".\\UW2.exe")) {

         unsigned int length;
         fread(&length, 1, 4, fd);

         fprintf(dest, "uncompressed length: %04x bytes\n", length);

         unsigned char* buffer = new unsigned char[length];

         for (unsigned int n = 0; n < length;)
         {
            unsigned char bits = fgetc(fd);
            //printf("new block: %02x\n",bits);

            for (int b = 0; b < 8; b++, bits >>= 1)
            {
               if (bits & 1)
               {
                  // bit set, copy one byte
                  buffer[n++] = fgetc(fd);
                  //printf("put: %02x (target=%04x)\n", buffer[n-1],n-1);
               }
               else
               {
                  // bit cleared, "repeat run"
                  int m1 = fgetc(fd);
                  int m2 = fgetc(fd);

                  signed int offset = m1 | ((m2 & 0xF0) << 4);
                  unsigned int count = (m2 & 0x0F) + 3;

                  if (offset & 0x800)
                     offset |= 0xFFFFF000;

                  offset += 18;

                  while (offset < ((int)n - 0x1000))
                     offset += 0x1000;

                  if (unsigned(offset) > n)
                  {
                     //printf("repeat: error: pos %03x, count %u\n", offset, count);
                     n = length;
                     break;
                  }

                  //printf("repeat: offset %03x, count %u\n", offset, count);

                  while (count-- && n < length)
                     buffer[n++] = buffer[offset++];
               }
               if (n >= length)
                  break;
            }
         }

         unsigned char* bufptr = buffer;

         // decode conv. header

         unsigned int unknown1 = *(unsigned int*)bufptr;
         bufptr += 4;
         unsigned int codesize = *(unsigned int*)bufptr;
         bufptr += 4;

         unsigned short unknown2 = *(unsigned short*)bufptr;
         bufptr += 2;
         unsigned short strblock = *(unsigned short*)bufptr;
         bufptr += 2;
         unsigned short initstack = *(unsigned short*)bufptr;
         bufptr += 2;

         // output conversation info
         fprintf(dest, "; conversation #%u: offset=0x%04x, unk1=0x%04x, codesize=0x%04x, length=0x%04x\n",
            i, offsets[i], unknown1, codesize, length);

         fprintf(dest, "; unk2=%04x, conv=%04x, init_stack=%04x\n",
            unknown2, strblock, initstack);

         unsigned short globcount = *(unsigned short*)bufptr;
         bufptr += 2;

         for (int a = 0; a < globcount; a++)
         {
            unsigned short namelen = *(unsigned short*)bufptr;
            bufptr += 2;

            std::string funcname;
            funcname.append((const char*)bufptr, namelen);

            bufptr += namelen;

            unsigned short func_id = *(unsigned short*)bufptr;
            bufptr += 2;
            unsigned short unk1 = *(unsigned short*)bufptr;
            bufptr += 2;
            unsigned short import_type = *(unsigned short*)bufptr;
            bufptr += 2;
            unsigned short ret_type = *(unsigned short*)bufptr;
            bufptr += 2;

            if (import_type == 0x010F)
            {
               fprintf(dest, "; var mem=%04x (ret_type=%04x, unk1=%04x) name=\"%s\" \n",
                  func_id, ret_type, unk1, funcname.c_str());
            }
            else
               if (import_type == 0x0111)
               {
                  fprintf(dest, "; func id=%04x ret_type=%-6s (unk1=%04x) name=\"%s\"\n",
                     func_id,
                     ret_type == 0 ? "void" : ret_type == 0x0129 ? "int" :
                     ret_type == 0x012b ? "string" : "unknown",
                     unk1, funcname.c_str());
               }
               else
                  fprintf(dest, "; unknown import type %04x for name=\"%s\"\n",
                     import_type, funcname.c_str());
         }

         // get pointer to codes
         unsigned short* allcodes = (unsigned short*)bufptr;

         // decode them
         decode_instructions(allcodes, dest, codesize);

         delete buffer;
      }
//#else
      else {

         unsigned int unknown1;
         fread(&unknown1, 1, 4, fd);

         unsigned int codesize;
         fread(&codesize, 1, 4, fd);

         // output conversation info
         fprintf(dest, "; conversation #%u: offset=0x%04x, unk1=0x%04x, codesize=0x%04x\n",
            i, offsets[i], unknown1, codesize);

         unsigned short unknown2[3];
         fread(unknown2, 2, 3, fd);

         fprintf(dest, "; unk2=%04x, conv=%04x, init_stack=%04x\n",
            unknown2[0], unknown2[1], unknown2[2]);

         fprintf(dest, "; string block to use: %04x\n", i + 0x0e00);

         dump_import_funcs(fd, dest);

         // read in codes
         unsigned short* allcodes = new unsigned short[codesize + 1];
         fread(allcodes, 2, codesize, fd);

         // decode them
         decode_instructions(allcodes, dest, codesize);

         delete[] allcodes;

         //#endif
      }
      fprintf(dest,"; -----------------------------------------\n\n");

      convcount++;
   }

   fprintf(dest,"; dumped %u conversations\n\n",convcount);

   fclose(dest);

   delete[] offsets;

   fclose(fd);

   printf("CNV.ARK decoded to UWXtract\\CNV_ARK.txt\n");

	return 0;
}
