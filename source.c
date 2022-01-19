/*----------------------------------------------------------------------------*/
/*--  blz.c - Bottom LZ coding for Nintendo GBA/DS                          --*/
/*--  Copyright (C) 2011 CUE                                                --*/
/*--                                                                        --*/
/*--  This program is free software: you can redistribute it and/or modify  --*/
/*--  it under the terms of the GNU General Public License as published by  --*/
/*--  the Free Software Foundation, either version 3 of the License, or     --*/
/*--  (at your option) any later version.                                   --*/
/*--                                                                        --*/
/*--  This program is distributed in the hope that it will be useful,       --*/
/*--  but WITHOUT ANY WARRANTY; without even the implied warranty of        --*/
/*--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          --*/
/*--  GNU General Public License for more details.                          --*/
/*--                                                                        --*/
/*--  You should have received a copy of the GNU General Public License     --*/
/*--  along with this program. If not, see <http://www.gnu.org/licenses/>.  --*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/*----------------------------------------------------------------------------*/
#define CMD_DECODE    0x00       // decode
#define CMD_ENCODE    0x01       // encode

#define BLZ_NORMAL    0          // normal mode
#define BLZ_BEST      1          // best mode

#define BLZ_SHIFT     1          // bits to shift
#define BLZ_MASK      0x80       // bits to check:
                                 // ((((1 << BLZ_SHIFT) - 1) << (8 - BLZ_SHIFT)

#define BLZ_THRESHOLD 2          // max number of bytes to not encode
#define BLZ_N         0x1002     // max offset ((1 << 12) + 2)
#define BLZ_F         0x12       // max coded ((1 << 4) + BLZ_THRESHOLD)

#define RAW_MINIM     0x00000000 // empty file, 0 bytes
#define RAW_MAXIM     0x00FFFFFF // 3-bytes length, 16MB - 1

#define BLZ_MINIM     0x00000004 // header only (empty RAW file)
#define BLZ_MAXIM     0x01400000 // 0x0120000A, padded to 20MB:
                                 // * length, RAW_MAXIM
                                 // * flags, (RAW_MAXIM + 7) / 8
                                 // * header, 11
                                 // 0x00FFFFFF + 0x00200000 + 12 + padding

/*----------------------------------------------------------------------------*/
bool moduleParamsFound = false;
int sdkVer[2];
char titleID[4] = {0};
unsigned int a7mbk6 = 0;
unsigned int deviceListAddr = 0;

/*----------------------------------------------------------------------------*/
// GBA Slot init (SDK 5)
static const uint32_t gbaSlotInitSignature[3]         = {0xE92D4038, 0xE59F0094, 0xE5901008};
static const uint32_t gbaSlotInitSignatureAlt[3]      = {0xE92D4038, 0xE59F4090, 0xE5940008};
static const uint16_t gbaSlotInitSignatureThumb[3]    = {0xB538, 0x4818, 0x6881};
static const uint16_t gbaSlotInitSignatureThumbAlt[3] = {0xB538, 0x4C18, 0x68A0};

/*----------------------------------------------------------------------------*/
#define BREAK(text)   { printf(text); return; }
#define EXIT(text)    { printf(text); exit(-1); }

/*----------------------------------------------------------------------------*/
void  Title(void);
void  Usage(void);

char *Load(char *filename, unsigned int source, int srcLength);
void  Save(char *filename, char *buffer, int length);
char *Memory(int length, int size);

void  BLZ_Decode(char *filename);
void  BLZ_Encode(char *filename, char *outfilename, int mode);
char *BLZ_Code(unsigned char *raw_buffer, int raw_len, int *new_len, int best);
void  BLZ_Invert(char *buffer, int length);
short BLZ_CRC16(unsigned char *buffer, unsigned int length);

int filelength(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    int fsize = 0;
    if (fp) {
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);			// Get source file's size
		fseek(fp, 0, SEEK_SET);
	}
	fclose(fp);

	return fsize;
}

/*----------------------------------------------------------------------------*/
void arm7extract(char *filename, char *outfilename, char *outfilenamei) {
  unsigned char *raw_buffer;
  unsigned int   arm7src, arm7len, arm7isrc, arm7ilen;

  printf("- loading header of '%s'\n", filename);
	FILE* ndsFile = fopen(filename, "rb");
	fseek(ndsFile, 0x30, SEEK_SET);
	fread(&arm7src, sizeof(unsigned int), 1, ndsFile);
	fseek(ndsFile, 0x3C, SEEK_SET);
	fread(&arm7len, sizeof(unsigned int), 1, ndsFile);
	fseek(ndsFile, 0x1A0, SEEK_SET);
	fread(&a7mbk6, sizeof(unsigned int), 1, ndsFile);
	fseek(ndsFile, 0x1D0, SEEK_SET);
	fread(&arm7isrc, sizeof(unsigned int), 1, ndsFile);
	fseek(ndsFile, 0x1D4, SEEK_SET);
	fread(&deviceListAddr, sizeof(unsigned int), 1, ndsFile);
	fseek(ndsFile, 0x1DC, SEEK_SET);
	fread(&arm7ilen, sizeof(unsigned int), 1, ndsFile);
	fclose(ndsFile);

  printf("- loading and dumping ARM7 binary\n");
  raw_buffer = Load(filename, arm7src, arm7len);
  Save(outfilename, raw_buffer, arm7len);
  free(raw_buffer);

  printf("- loading and dumping ARM7i binary\n");
  raw_buffer = Load(filename, arm7isrc, arm7ilen);
  // TODO: Decrypt modcrypt area
  Save(outfilenamei, raw_buffer, arm7ilen);
  free(raw_buffer);

  printf("\n");
}

/*----------------------------------------------------------------------------*/
int main(int argc, char **argv) {
  int cmd, mode;
  int arg;

  Title();

  if (argc < 2) Usage();
  /*if      (!strcmp(argv[1], "-d"))   { cmd = CMD_DECODE; }
  else if (!strcmp(argv[1], "-en"))  { cmd = CMD_ENCODE; mode = BLZ_NORMAL; }
  else if (!strcmp(argv[1], "-eo"))  { cmd = CMD_ENCODE; mode = BLZ_BEST; }
  else if (!strcmp(argv[1], "-en9")) { cmd = CMD_ENCODE; mode = BLZ_NORMAL; }
  else if (!strcmp(argv[1], "-eo9")) { cmd = CMD_ENCODE; mode = BLZ_BEST; }
  else                                 EXIT("Command not supported\n");
  if (argc < 3) EXIT("Filename not specified\n");

  switch (cmd) {
    case CMD_DECODE:
      for (arg = 2; arg < argc; arg++) BLZ_Decode(argv[arg]);
      break;
    case CMD_ENCODE:
      arm9 = argv[1][3] == '9' ? 1 : 0;
      for (arg = 2; arg < argc; arg++) BLZ_Encode(argv[arg], mode);
      break;
    default:
      break;
  }*/

	cmd = CMD_ENCODE; mode = BLZ_NORMAL;

	mkdir("out", 0777);

	char filenamenoext[128];
	char folderName[256];
	char outName9[256];
	char outName7[256];
	char outName7i[256];
	char outNameBase[256];

	char donorNdsName[256];

	for (arg = 1; arg < argc; arg++) {
		sprintf(filenamenoext, argv[arg]);
		for (int i = strlen(filenamenoext); i > 0; i--) {
			if (filenamenoext[i] == '.') {
				filenamenoext[i] = 0;
				break;
			}
		}
		sprintf(folderName, "out/%s", filenamenoext);
		mkdir(folderName, 0777);
		sprintf(outName9, "%s/arm9.bin", folderName);
		sprintf(outName7, "%s/arm7.bin", folderName);
		sprintf(outName7i, "%s/arm7i.bin", folderName);
		sprintf(outNameBase, "%s/base.nds", folderName);

		BLZ_Encode(argv[arg], outName9, mode);
		if (!moduleParamsFound) {
			continue;
		}

		sprintf(donorNdsName, "a7donors/dsiware/sdk%i%i.nds", sdkVer[0], sdkVer[1]);
		FILE* donorNdsFile = fopen(donorNdsName, "rb");
		if (!donorNdsFile && sdkVer[1] > 0) {
			for (int i = sdkVer[1]; i >= 0; i--) {
				sprintf(donorNdsName, "a7donors/dsiware/sdk%i%i.nds", sdkVer[0], i);
				donorNdsFile = fopen(donorNdsName, "rb");
				if (donorNdsFile) {
					break;
				}
			}
		}

		if (donorNdsFile) {
			fclose(donorNdsFile);
			arm7extract(donorNdsName, outName7, outName7i);
		}

		unsigned char* copyBuf = Memory(0x100000, 1);

		int romSize = filelength(argv[arg]);
		FILE* sourceFile = fopen(argv[arg], "rb");
		FILE* outputFile = fopen(outNameBase, "wb");
		if (outputFile) {
			printf("- Copying to new base.nds file\n");
			int offset = 0;
			int numr;
			bool modified = false;
			while (1) {
				// Copy file to destination path
				numr = fread(copyBuf, 1, 0x100000, sourceFile);
				if (!modified) {
					*(unsigned int*)(copyBuf + 0x1A0) = a7mbk6;
					*(unsigned int*)(copyBuf + 0x1D4) = deviceListAddr;
					modified = true;
				}
				fwrite(copyBuf, 1, numr, outputFile);
				offset += 0x100000;

				if (offset > romSize) {
					fclose(sourceFile);
					fclose(outputFile);
					break;
				}
			}
		}

		free(copyBuf);
	}

  printf("\nDone\n");

  return(0);
}

/*----------------------------------------------------------------------------*/
void Title(void) {
  printf(
    "\n"
    "TWL-ROM-Optimize - Rocket Robz 2022\n"
    "BLZ compression code from BLZ - (c) CUE 2011\n"
    "\n"
  ); 
}

/*----------------------------------------------------------------------------*/
void Usage(void) {
  EXIT(
    "Usage: TWL-ROM-Optimize romfilename [romfilename [...]]\n"
    "\n"
    "When running, new small ARM binaries will be in \"out/romfolder/\".\n"
    "Use TinkeDSi to replace the existing files in the ftc folder in the base.nds file.\n"
  );
}

/*----------------------------------------------------------------------------*/
char *Load(char *filename, unsigned int source, int srcLength) {
  FILE *fp;
  char *fb;

  if ((fp = fopen(filename, "rb")) == NULL) EXIT("\nFile open error\n");
  fseek(fp, source, SEEK_SET);
  fb = Memory(srcLength, sizeof(char));
  if (fread(fb, 1, srcLength, fp) != srcLength) EXIT("\nFile read error\n");
  if (fclose(fp) == EOF) EXIT("\nFile close error\n");

  return(fb);
}

/*----------------------------------------------------------------------------*/
void Save(char *filename, char *buffer, int length) {
  FILE *fp;

  if ((fp = fopen(filename, "wb")) == NULL) EXIT("\nFile create error\n");
  if (fwrite(buffer, 1, length, fp) != length) EXIT("\nFile write error\n");
  if (fclose(fp) == EOF) EXIT("\nFile close error\n");
}

/*----------------------------------------------------------------------------*/
char *Memory(int length, int size) {
  char *fb;

  fb = (char *) calloc(length, size);
  if (fb == NULL) EXIT("\nMemory error\n");

  return(fb);
}

/*----------------------------------------------------------------------------*/
/*void BLZ_Decode(char *filename) {
  unsigned char *pak_buffer, *raw_buffer, *pak, *raw, *pak_end, *raw_end;
  unsigned int   pak_len, raw_len, len, pos, inc_len, hdr_len, enc_len, dec_len;
  unsigned char  flags, mask;

  printf("- decoding '%s'", filename);

  pak_buffer = Load(filename, &pak_len, BLZ_MINIM, BLZ_MAXIM);

  inc_len = *(unsigned int *)(pak_buffer + pak_len - 4);
  if (!inc_len) {
    printf(", WARNING: not coded file!");
    enc_len = 0;
    dec_len = pak_len;
    pak_len = 0;
    raw_len = dec_len;
  } else {
    if (pak_len < 8) EXIT("\nFile has a bad header\n");
    hdr_len = pak_buffer[pak_len - 5];
    if ((hdr_len < 0x08) || (hdr_len > 0x0B)) EXIT("\nBad header length\n");
    if (pak_len <= hdr_len) EXIT("\nBad length\n");
    enc_len = *(unsigned int *)(pak_buffer + pak_len - 8) & 0x00FFFFFF;
    dec_len = pak_len - enc_len;
    pak_len = enc_len - hdr_len;
    raw_len = dec_len + enc_len + inc_len;
    if (raw_len > RAW_MAXIM) EXIT("\nBad decoded length\n");
  }

  raw_buffer = (unsigned char *) Memory(raw_len, sizeof(char));

  pak = pak_buffer;
  raw = raw_buffer;
  pak_end = pak_buffer + dec_len + pak_len;
  raw_end = raw_buffer + raw_len;

  for (len = 0; len < dec_len; len++) *raw++ = *pak++;

  BLZ_Invert(pak_buffer + dec_len, pak_len);

  mask = 0;

  while (raw < raw_end) {
    if (!(mask >>= BLZ_SHIFT)) {
      if (pak == pak_end) break;
      flags = *pak++;
      mask = BLZ_MASK;
    }

    if (!(flags & mask)) {
      if (pak == pak_end) break;
      *raw++ = *pak++;
    } else {
      if (pak + 1 >= pak_end) break;
      pos = *pak++ << 8;
      pos |= *pak++;
      len = (pos >> 12) + BLZ_THRESHOLD + 1;
      if (raw + len > raw_end) {
        printf(", WARNING: wrong decoded length!");
        len = raw_end - raw;
      }
      pos = (pos & 0xFFF) + 3;
      while (len--) *raw++ = *(raw - pos);
    }
  }

  BLZ_Invert(raw_buffer + dec_len, raw_len - dec_len);

  raw_len = raw - raw_buffer;

  if (raw != raw_end) printf(", WARNING: unexpected end of encoded file!");

  Save(filename, raw_buffer, raw_len);

  free(raw_buffer);
  free(pak_buffer);

  printf("\n");
}*/

/*----------------------------------------------------------------------------*/
void BLZ_Encode(char *filename, char *outfilename, int mode) {
  unsigned char *raw_buffer, *pak_buffer, *new_buffer;
  unsigned int   raw_len, pak_len, new_len;

  printf("- loading header of '%s'\n", filename);
	unsigned int arm9src = 0;
	unsigned int arm9dst = 0;
	FILE* ndsFile = fopen(filename, "rb");
	fseek(ndsFile, 0xC, SEEK_SET);
	fread(titleID, 1, 3, ndsFile);
	fseek(ndsFile, 0x20, SEEK_SET);
	fread(&arm9src, sizeof(unsigned int), 1, ndsFile);
	fseek(ndsFile, 0x28, SEEK_SET);
	fread(&arm9dst, sizeof(unsigned int), 1, ndsFile);
	fseek(ndsFile, 0x2C, SEEK_SET);
	fread(&raw_len, sizeof(unsigned int), 1, ndsFile);
	fseek(ndsFile, 0x1A0, SEEK_SET);
	fread(&a7mbk6, sizeof(unsigned int), 1, ndsFile);
	fclose(ndsFile);

  printf("- loading ARM9 binary\n");
  raw_buffer = Load(filename, arm9src, raw_len);

  printf("- searching module params...");
	moduleParamsFound = false;
	unsigned int moduleParamsOffset = 0;
	for (moduleParamsOffset = 0; moduleParamsOffset < raw_len; moduleParamsOffset += 4) {
		if (*(unsigned int*)(raw_buffer + moduleParamsOffset) == 0xDEC00621 && *(unsigned int*)(raw_buffer + moduleParamsOffset + 4) == 0x2106C0DE) {
			printf(" found\n");
			moduleParamsFound = true;
			sdkVer[0] = *(char*)(raw_buffer + moduleParamsOffset - 1); // SDK version
			sdkVer[1] = *(char*)(raw_buffer + moduleParamsOffset - 2); // SDK sub-version
			if (*(unsigned int*)(raw_buffer + moduleParamsOffset - 8) != 0) {
				printf("- ARM9 binary already compressed\n");
				free(raw_buffer);
				return;
			}
			break;
		}
	}

	if (!moduleParamsFound) {
		printf(" not found\n");
		free(raw_buffer);
		return;
	} else if (moduleParamsOffset >= 0x3000) {
		printf("- module params offset is invalid\n");
		free(raw_buffer);
		return;
	}

	if (a7mbk6 == 0x00403000) {
		bool found = false;
		for (int i = 0; i < raw_len; i += 4) {
			if ((*(uint32_t*)(raw_buffer + i)     == gbaSlotInitSignature[0] || *(uint32_t*)(raw_buffer + i)     == gbaSlotInitSignatureAlt[0])
			 && (*(uint32_t*)(raw_buffer + i + 4) == gbaSlotInitSignature[1] || *(uint32_t*)(raw_buffer + i + 4) == gbaSlotInitSignatureAlt[1])
			 && (*(uint32_t*)(raw_buffer + i + 8) == gbaSlotInitSignature[2] || *(uint32_t*)(raw_buffer + i + 8) == gbaSlotInitSignatureAlt[2]))
			{
				*(uint32_t*)(raw_buffer + i) = 0xE12FFF1E;
				found = true;
				break;
			}
		}
		if (!found) {
			for (int i = 0; i < raw_len; i += 2) {
				if ((*(uint16_t*)(raw_buffer + i)     == gbaSlotInitSignatureThumb[0] || *(uint16_t*)(raw_buffer + i)     == gbaSlotInitSignatureThumbAlt[0])
				 && (*(uint16_t*)(raw_buffer + i + 2) == gbaSlotInitSignatureThumb[1] || *(uint16_t*)(raw_buffer + i + 2) == gbaSlotInitSignatureThumbAlt[1])
				 && (*(uint16_t*)(raw_buffer + i + 4) == gbaSlotInitSignatureThumb[2] || *(uint16_t*)(raw_buffer + i + 4) == gbaSlotInitSignatureThumbAlt[2]))
				{
					*(uint16_t*)(raw_buffer + i) = 0x4770;
					found = true;
					break;
				}
			}
		}
	}

  printf("- compressing ARM9 binary", filename);

  pak_buffer = NULL;
  pak_len = BLZ_MAXIM + 1;

  new_buffer = BLZ_Code(raw_buffer, raw_len, &new_len, mode);
  if (new_len < pak_len) {
    if (pak_buffer != NULL) free(pak_buffer);
    pak_buffer = new_buffer;
    pak_len = new_len;
  }

	*(unsigned int*)(pak_buffer + moduleParamsOffset - 8) = arm9dst + pak_len;

  Save(outfilename, pak_buffer, pak_len);

  free(pak_buffer);
  free(raw_buffer);

  printf("\n");
}

/*----------------------------------------------------------------------------*/
char *BLZ_Code(unsigned char *raw_buffer, int raw_len, int *new_len, int best) {
  unsigned char *pak_buffer, *pak, *raw, *raw_end, *flg, *tmp;
  unsigned int   pak_len, inc_len, hdr_len, enc_len, len, pos, max;
  unsigned int   len_best, pos_best, len_next, pos_next, len_post, pos_post;
  unsigned int   pak_tmp, raw_tmp, raw_new;
  unsigned short crc;
  unsigned char  mask;

#define SEARCH(l,p) { \
  l = BLZ_THRESHOLD;                                          \
                                                              \
  max = raw - raw_buffer >= BLZ_N ? BLZ_N : raw - raw_buffer; \
  for (pos = 3; pos <= max; pos++) {                          \
    for (len = 0; len < BLZ_F; len++) {                       \
      if (raw + len == raw_end) break;                        \
      if (len >= pos) break;                                  \
      if (*(raw + len) != *(raw + len - pos)) break;          \
    }                                                         \
                                                              \
    if (len > l) {                                            \
      p = pos;                                                \
      if ((l = len) == BLZ_F) break;                          \
    }                                                         \
  }                                                           \
}

  pak_tmp = 0;
  raw_tmp = raw_len;

  pak_len = raw_len + ((raw_len + 7) / 8) + 11;
  pak_buffer = (unsigned char *) Memory(pak_len, sizeof(char));

  raw_new = raw_len - 0x4000;
  /* if (arm9) {
    if (raw_len < 0x4000) {
      printf(", WARNING: ARM9 must be greater as 16KB, switch [9] disabled");
    } else if (
      (*(unsigned int   *)(raw_buffer + 0x0) != 0xE7FFDEFF) ||
      (*(unsigned int   *)(raw_buffer + 0x4) != 0xE7FFDEFF) ||
      (*(unsigned int   *)(raw_buffer + 0x8) != 0xE7FFDEFF) ||
      (*(unsigned short *)(raw_buffer + 0xC) != 0xDEFF)
    ) {
      printf(", WARNING: invalid Secure Area ID, switch [9] disabled");
    } else if (*(short *)(raw_buffer + 0x7FE)) {
      printf(", WARNING: invalid Secure Area 2KB end, switch [9] disabled");
    } else {
      crc = (unsigned short)BLZ_CRC16(raw_buffer + 0x10, 0x07F0);
      if (*(unsigned short *)(raw_buffer + 0x0E) != crc) {
        printf(", WARNING: CRC16 Secure Area 2KB do not match");
        *(unsigned short *)(raw_buffer + 0x0E) = crc;
      }
      raw_new -= 0x4000;
    }
  } */

  BLZ_Invert(raw_buffer, raw_len);

  pak = pak_buffer;
  raw = raw_buffer;
  raw_end = raw_buffer + raw_new;

  mask = 0;

  while (raw < raw_end) {
    if (!(mask >>= BLZ_SHIFT)) {
      *(flg = pak++) = 0;
      mask = BLZ_MASK;
    }

    SEARCH(len_best, pos_best);

    // LZ-CUE optimization start
    if (best) {
      if (len_best > BLZ_THRESHOLD) {
        if (raw + len_best < raw_end) {
          raw += len_best;
          SEARCH(len_next, pos_next);
          raw -= len_best - 1;
          SEARCH(len_post, pos_post);
          raw--;

          if (len_next <= BLZ_THRESHOLD) len_next = 1;
          if (len_post <= BLZ_THRESHOLD) len_post = 1;

          if (len_best + len_next <= 1 + len_post) len_best = 1;
        }
      }
    }
    // LZ-CUE optimization end

    *flg <<= 1;
    if (len_best > BLZ_THRESHOLD) {
      raw += len_best;
      *flg |= 1;
      *pak++ = ((len_best - (BLZ_THRESHOLD+1)) << 4) | ((pos_best - 3) >> 8);
      *pak++ = (pos_best - 3) & 0xFF;
    } else {
      *pak++ = *raw++;
    }

#if 1
    if (pak - pak_buffer + raw_len - (raw - raw_buffer) < pak_tmp + raw_tmp) {
#else
    if (
      (((pak - pak_buffer + raw_len - (raw - raw_buffer)) + 3) & -4)
      <
      pak_tmp + raw_tmp
    ) {
#endif
      pak_tmp = pak - pak_buffer;
      raw_tmp = raw_len - (raw - raw_buffer);
    }
  }

  while (mask && (mask != 1)) {
    mask >>= BLZ_SHIFT;
    *flg <<= 1;
  }

  pak_len = pak - pak_buffer;

  BLZ_Invert(raw_buffer, raw_len);
  BLZ_Invert(pak_buffer, pak_len);

  if (!pak_tmp || (raw_len + 4 < ((pak_tmp + raw_tmp + 3) & -4) + 8)) {
    pak = pak_buffer;
    raw = raw_buffer;
    raw_end = raw_buffer + raw_len;

    while (raw < raw_end) *pak++ = *raw++;

    while ((pak - pak_buffer) & 3) *pak++ = 0;

    *(unsigned int *)pak = 0; pak += 4;
  } else {
    tmp = (unsigned char *) Memory(raw_tmp + pak_tmp + 11, sizeof(char));

    for (len = 0; len < raw_tmp; len++)
      tmp[len] = raw_buffer[len];

    for (len = 0; len < pak_tmp; len++)
      tmp[raw_tmp + len] = pak_buffer[len + pak_len - pak_tmp];

    pak = pak_buffer;
    pak_buffer = tmp;

    free(pak);

    pak = pak_buffer + raw_tmp + pak_tmp;

    enc_len = pak_tmp;
    hdr_len = 8;
    inc_len = raw_len - pak_tmp - raw_tmp;

    while ((pak - pak_buffer) & 3) {
      *pak++ = 0xFF;
      hdr_len++;
    }

    *(unsigned int *)pak = enc_len + hdr_len; pak += 3;
    *pak++ = hdr_len;
    *(unsigned int *)pak = inc_len - hdr_len; pak += 4;
  }

  *new_len = pak - pak_buffer;

  return(pak_buffer);
}

/*----------------------------------------------------------------------------*/
void BLZ_Invert(char *buffer, int length) {
  char *bottom, ch;

  bottom = buffer + length - 1;

  while (buffer < bottom) {
    ch = *buffer;
    *buffer++ = *bottom;
    *bottom-- = ch;
  }
}

/*----------------------------------------------------------------------------*/
short BLZ_CRC16(unsigned char *buffer, unsigned int length) {
  unsigned short crc;
  unsigned int   nbits;

  crc = 0xFFFF;
  while (length--) {
    crc ^= *buffer++;
    nbits = 8;
    while (nbits--) {
      if (crc & 1) { crc = (crc >> 1) ^ 0xA001; }
      else           crc =  crc >> 1;
    }
  }

  return(crc);
}

/*----------------------------------------------------------------------------*/
/*--  EOF                                           Copyright (C) 2011 CUE  --*/
/*----------------------------------------------------------------------------*/
