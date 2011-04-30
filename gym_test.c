#include "lj_gym.h"
#include "lj_ym2612.h"

#include <malloc.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	int result = LJ_GYM_OK;

	LJ_GYM_FILE* gymFile = NULL;
	LJ_GYM_INSTRUCTION gymInstruction;

	LJ_YM2612* ym2612 = NULL;

	LJ_YM_INT16* outputs[2];
	outputs[0] = malloc(1024 * sizeof(LJ_YM_INT16));
	outputs[1] = malloc(1024 * sizeof(LJ_YM_INT16));
	int numCycles = 128;


	gymFile = LJ_GYM_create( "test.gym" );
	ym2612 = LJ_YM2612_create();

	while (result == LJ_GYM_OK)
	{
		result = LJ_GYM_read(gymFile, &gymInstruction);
		if (result == LJ_GYM_OK)
		{
			LJ_GYM_debugPrint( &gymInstruction);
			if (gymInstruction.cmd == LJ_GYM_WRITE_PORT_0)
			{
				result = LJ_YM2612_setRegister(ym2612, 0, gymInstruction.R, gymInstruction.D);
			}
			if (gymInstruction.cmd == LJ_GYM_WRITE_PORT_1)
			{
				result = LJ_YM2612_setRegister(ym2612, 1, gymInstruction.R, gymInstruction.D);
			}
		}
		if (result == LJ_GYM_OK)
		{
			result = LJ_YM2612_generateOutput(ym2612, numCycles, outputs);
		}
	};

	LJ_GYM_destroy(gymFile);
	LJ_YM2612_destroy(ym2612);
	return -1;
}
