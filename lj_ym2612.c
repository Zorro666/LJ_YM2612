#include "lj_ym2612.h"

#include <stdio.h>

////////////////////////////////////////////////////////////////////
// 
// Internal data & functions
// 
////////////////////////////////////////////////////////////////////

typedef enum LJ_YM2612_REGISTERS LJ_YM2612_REGISTERS;

enum LJ_YM2612_REGISTERS {
		LJ_KEY_ONOFF = 0x28,
};

////////////////////////////////////////////////////////////////////
// 
// External data & functions
// 
////////////////////////////////////////////////////////////////////

LJ_YM2612* LJ_YM2612_create(void)
{
	return NULL;
}

LJ_YM2612_RESULT LJ_YM2612_destroy(LJ_YM2612* const ym2612)
{
	int result = LJ_YM2612_ERROR;

	return result;
}

LJ_YM2612_RESULT LJ_YM2612_setRegister(LJ_YM2612* const ym2612, char port, char reg, char data)
{
	fprintf(stderr, "LJ_YM2612_setRegister:unknown register:0x%X port:%d data:0x%X\n", reg, port, data);
	return LJ_YM2612_ERROR;
}
