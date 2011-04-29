#include "lj_gym.h"

int main(int argc, char* argv[])
{
	LJ_GYM_FILE* gymFile = NULL;
	LJ_GYM_INSTRUCTION gymInstruction;
	int result = LJ_GYM_OK;
	
	gymFile = LJ_GYM_create( "test.gym" );

	while (result == LJ_GYM_OK)
	{
		result = LJ_GYM_read(gymFile, &gymInstruction);
		if (result == LJ_GYM_OK)
		{
			LJ_GYM_debugPrint( &gymInstruction);
		}
	};

	LJ_GYM_destroy(gymFile);
	return -1;
}
