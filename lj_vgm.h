#ifndef LJ_VGM_HH
#define LJ_VGM_HH

#include <stdio.h>

// Forward declarations
typedef struct LJ_VGM_FILE LJ_VGM_FILE;
typedef struct LJ_VGM_HEADER LJ_VGM_HEADER;
typedef struct LJ_VGM_INSTRUCTION LJ_VGM_INSTRUCTION;
typedef enum LJ_VGM_RESULT LJ_VGM_RESULT;
typedef enum LJ_VGM_COMMAND LJ_VGM_COMMAND;

typedef unsigned char LJ_VGM_UINT8;

enum LJ_VGM_RESULT { 
	LJ_VGM_OK = 0,
	LJ_VGM_ERROR = -1
};

enum LJ_VGM_COMMAND { 
	LJ_VGM_GameGear_PSG = 0x4F, 
	LJ_VGM_SN764xx_PSG = 0x50, 
	LJ_VGM_YM2413_WRITE =0x51, 
	LJ_VGM_YM2612_WRITE_PORT_0 = 0x52,
	LJ_VGM_YM2612_WRITE_PORT_1 = 0x53,
	LJ_VGM_YM2151_WRITE = 0x54,
	LJ_VGM_WAIT_SAMPLES = 0x61,
	LJ_VGM_WAIT_60th = 0x62,
	LJ_VGM_WAIT_50th = 0x63,
	LJ_VGM_END_SOUND_DATA = 0x66,
	LJ_VGM_DATA_BLOCK_START = 0x67,
	LJ_VGM_WAIT_N_SAMPLES = 0x70,
	LJ_VGM_YM2612_WRITE_DATA = 0x80,
	LJ_VGM_SEEK_OFFSET = 0xe0,
};

struct LJ_VGM_INSTRUCTION
{
	int pos;
	int cmdCount;
	LJ_VGM_COMMAND cmd;
	LJ_VGM_UINT8 R;
	LJ_VGM_UINT8 D;
};

LJ_VGM_FILE* LJ_VGM_create(const char* const fname);
LJ_VGM_RESULT LJ_VGM_destroy(LJ_VGM_FILE* const vgmFile);

LJ_VGM_RESULT LJ_VGM_read(LJ_VGM_FILE* const vgmFile, LJ_VGM_INSTRUCTION* const vgmInstr);
//TODO: test reset
LJ_VGM_RESULT LJ_VGM_reset(LJ_VGM_FILE* const vgmFile);
		
void LJ_VGM_debugPrint( const LJ_VGM_INSTRUCTION* const vgmInstr);

#endif // #ifndef LJ_VGM_HH
