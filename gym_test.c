#include "gym.h"

int main(int argc, char* argv[])
{
	GYM_FILE* gymFile = NULL;
	GYM_INSTRUCTION gymInstruction;
	int result = GYM_OK;
	
	gymFile = GYM_create( "test.gym" );

	while (result == GYM_OK)
	{
		result = GYM_read(gymFile, &gymInstruction);
		if (result == GYM_OK)
		{
			GYM_debugPrint( &gymInstruction);
		}
	};

	GYM_destroy(gymFile);
	return -1;
}
