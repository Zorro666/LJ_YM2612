#include "lj_gym.h"
#include "lj_ym2612.h"

int main(int argc, char* argv[])
{
	LJ_GYM_FILE* gymFile = NULL;
	LJ_GYM_INSTRUCTION gymInstruction;

	LJ_YM2612* ym2612 = NULL;

	int result = LJ_GYM_OK;
	
	gymFile = LJ_GYM_create( "test.gym" );

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
	};

	LJ_GYM_destroy(gymFile);
	return -1;
}
