#include "lj_ym2612.h"

#include <string.h>

// Globals for vgm_test_main.c
void* device_ym2612_create()
{
	LJ_YM2612* ym2612 = LJ_YM2612_create();
	return ym2612;
}

int device_ym2612_write(void* const ym2612, const int address, const int data)
{
	return LJ_YM2612_write(ym2612, address, data);
}

int device_ym2612_generateOutput(void* const ym2612, const int numCycles, short* output[2])
{
	return LJ_YM2612_generateOutput(ym2612, numCycles, output);
}

int device_ym2612_destroy(void* ym2612)
{
	return LJ_YM2612_destroy(ym2612);
}

const char* const getWavOutputName(const char* const inputName)
{
	static char wavOutputName[256];
	strncpy(wavOutputName, inputName, 256); 
	char* ext = strchr(wavOutputName, '.');
	if (ext != NULL)
	{
		ext[0] = '\0';
	}
	strcat(wavOutputName, ".wav");
	return wavOutputName;
}


