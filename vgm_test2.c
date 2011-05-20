#include "emu.h"
#include "fm.h"

#include "vgm_test_main.h"

#include <malloc.h>
#include <string.h>

/* Globals for vgm_test_main.c */
void* device_ym2612_create(const int clockRate, const int outputSampleRate, const unsigned int flags)
{
	UINT32 ymFlags = 0;

	/*ym2612_init(void *param, device_t *device, int clock, int rate, FM_TIMERHANDLER timer_handler,FM_IRQHANDLER IRQHandler)*/
	void* ym2612 = ym2612_init( NULL, NULL, clockRate, outputSampleRate, NULL, NULL);
	ym2612_reset_chip(ym2612);

	if (flags & DEVICE_YM2612_DEBUG)
	{
		ymFlags |= YM2612_DEBUG;
	}
	if (flags & DEVICE_YM2612_NODAC)
	{
		ymFlags |= YM2612_NODAC;
	}
	if (flags & DEVICE_YM2612_NOFM)
	{
		ymFlags |= YM2612_NOFM;
	}
	ym2612_set_flags(ym2612, ymFlags);

	return ym2612;
}

int device_ym2612_write(void* const ym2612, const int address, const int data)
{
	return ym2612_write(ym2612, address, (UINT8)data);
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

char* getWavOutputName(const char* const inputName)
{
	static char wavOutputName[256];
	char* ext;
	strncpy(wavOutputName, inputName, 256); 
	ext = strchr(wavOutputName, '.');
	if (ext != NULL)
	{
		ext[0] = '\0';
	}
	strcat(wavOutputName, "_2");
	strcat(wavOutputName, ".wav");
	return wavOutputName;
}

/* Globals for fm2612.c */
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
	if (param)
	{
	}
}

