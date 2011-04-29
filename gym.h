#ifndef GYM_HH
#define GYM_HH

#include <stdio.h>

typedef struct gym_file
{
	FILE* fh;
	int pos;
} gym_file;

gym_file* GYM_create(char* fname);
		
#endif // #ifndef GYM_HH
