#ifndef LJ_GYM_HH
#define LJ_GYM_HH

/*
The GYM file format contains only four different instructions, each represented by one byte with 0 to 2 bytes of arguments:
0x00      Do nothing for 1/60th of a second (NOP)
0x01 R D  write data D on YM port 0, register R
0x02 R D  write data D on YM port 1, register R
0x03 D    write on PSG port the data D
*/

#include <stdio.h>

// Forward declarations
typedef struct LJ_GYM_FILE LJ_GYM_FILE;
typedef struct LJ_GYM_INSTRUCTION LJ_GYM_INSTRUCTION;
typedef enum LJ_GYM_RESULT LJ_GYM_RESULT;
typedef enum LJ_GYM_COMMAND LJ_GYM_COMMAND;

enum LJ_GYM_RESULT { 
	LJ_GYM_OK = 0,
	LJ_GYM_ERROR = -1
};

enum LJ_GYM_COMMAND { 
	LJ_GYM_NOP = 0, 
	LJ_GYM_WRITE_PORT_0 = 1, 
	LJ_GYM_WRITE_PORT_1 = 2, 
	LJ_GYM_WRITE_PORT_PSG = 3
};

struct LJ_GYM_INSTRUCTION
{
	int pos;
	LJ_GYM_COMMAND cmd;
	unsigned char R;
	unsigned char D;
};

LJ_GYM_FILE* LJ_GYM_create(const char* const fname);
LJ_GYM_RESULT LJ_GYM_destroy(LJ_GYM_FILE* const gymFile);

LJ_GYM_RESULT LJ_GYM_read(LJ_GYM_FILE* const gymFile, LJ_GYM_INSTRUCTION* const gymInstr);
LJ_GYM_RESULT LJ_GYM_reset(LJ_GYM_FILE* const gymFile);
		
void LJ_GYM_debugPrint( const LJ_GYM_INSTRUCTION* const gymInstr);

#endif // #ifndef LJ_GYM_HH
