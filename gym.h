#ifndef GYM_HH
#define GYM_HH

/*
The GYM file format contains only four different instructions, each represented by one byte with 0 to 2 bytes of arguments:
0x00      Do nothing for 1/60th of a second (NOP)
0x01 R D  write data D on YM port 0, register R
0x02 R D  write data D on YM port 1, register R
0x03 D    write on PSG port the data D
*/

#include <stdio.h>

typedef struct GYM_FILE
{
	FILE* fh;
	int pos;
} GYM_FILE;

typedef enum GYM_RESULT { 
	GYM_OK = 0,
	GYM_ERROR = -1
} GYM_RESULT;

typedef enum GYM_COMMAND { 
	GYM_NOP = 0, 
	GYM_WRITE_PORT_0 = 1, 
	GYM_WRITE_PORT_1 = 2, 
	GYM_WRITE_PORT_PSG = 3
} GYM_COMMAND;

typedef struct GYM_INSTRUCTION
{
	int pos;
	GYM_COMMAND cmd;
	unsigned char R;
	unsigned char D;
} GYM_INSTRUCTION;

GYM_FILE* GYM_create(const char* const fname);
GYM_RESULT GYM_destroy(GYM_FILE* const gymFile);

GYM_RESULT GYM_read(GYM_FILE* const gymFile, GYM_INSTRUCTION* const gymInstr);
GYM_RESULT GYM_reset(GYM_FILE* const gymFile);
		
void GYM_debugPrint( const GYM_INSTRUCTION* const gymInstr);

#endif // #ifndef GYM_HH
