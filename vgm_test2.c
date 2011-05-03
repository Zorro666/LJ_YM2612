#include "emu.h"
#include "fm.h"

#include "vgm_test_main.h"

#include <malloc.h>
#include <string.h>

// Globals for vgm_test_main.c
void* device_ym2612_create()
{
	const int clock = 7670453;
	const int rate = 44100;

	//ym2612_init(void *param, device_t *device, int clock, int rate, FM_TIMERHANDLER timer_handler,FM_IRQHANDLER IRQHandler)
	void* ym2612 = ym2612_init(NULL, NULL, clock, rate, NULL, NULL);
	ym2612_reset_chip(ym2612);

	return ym2612;
}

int device_ym2612_write(void* const ym2612, const int address, const int data)
{
	return ym2612_write(ym2612, address, data);
}

int device_ym2612_generateOutput(void* const ym2612, const int numCycles, short* output[2])
{
	FMSAMPLE* fmOutput[2];
	fmOutput[0] = (FMSAMPLE*)output[0];
	fmOutput[1] = (FMSAMPLE*)output[1];

	ym2612_update_one(ym2612, fmOutput, numCycles);
	return LJ_VGM_TEST_OK;
}

int device_ym2612_destroy(void* ym2612)
{
	ym2612_shutdown(ym2612);
	return LJ_VGM_TEST_OK;
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
	strcat(wavOutputName, "2");
	strcat(wavOutputName, ".wav");
	return wavOutputName;
}

// Globals for fm2612.c
void* allocClear(unsigned int size)
{
	return malloc(size);
}

void allocFree(void* v)
{
	free(v);
}

void ym2612_update_request(void *param)
{
}

