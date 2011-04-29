/*
The GYM file format contains only four different instructions, each represented by one byte with 0 to 2 bytes of arguments:
0x00      Do nothing for 1/60th of a second (NOP)
0x01 R D  write data D on YM port 0, register R
0x02 R D  write data D on YM port 1, register R
0x03 D    write on PSG port the data D
*/

#include <malloc.h>

#include "gym.h"

////////////////////////////////////////////////////////////////////
// 
// Internal data & functions
// 
////////////////////////////////////////////////////////////////////

static void gym_clear(gym_file* gym)
{
	gym->fh = 0;
	gym->pos = 0;
}

static int gym_open(gym_file* gym, char* fname)
{
	if ( gym == NULL )
	{
		fprintf( stderr, "gym_open:gym is NUM trying to open file '%s'\n", fname);
		return -1;
	}

	gym_clear( gym );

	FILE* fh = fopen(fname, "rb");
	if (fh == NULL)
	{
		fprintf( stderr, "gym_open:Failed to open file '%s'\n", fname);
		return -1;
	}
	gym->fh = fh;
	gym->pos = 0;

	return 1;
}

////////////////////////////////////////////////////////////////////
// 
// Exposed external data & functions
// 
////////////////////////////////////////////////////////////////////

gym_file* GYM_create(char* fname)
{
	gym_file* gym = malloc(sizeof(gym_file));
	if (gym == NULL)
	{
		fprintf( stderr, "GYM_create:malloc failed for file '%s'\n", fname);
		return NULL;
	}

	int result = gym_open(gym,fname);
	if (result == -1)
	{
		fprintf( stderr, "GYM_create:gym_open failed for file '%s'\n", fname);
		free(gym);
		return NULL;
	}

	return gym;
}

