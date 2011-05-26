#include "lj_ym2612.h"
#include "vgm_test_main.h"

#include <string.h>
#include <stdio.h>

/* Globals for vgm_test_main.c */
void* device_ym2612_create(const int clockRate, const int outputSampleRate, const unsigned int flags)
{
	LJ_YM2612* ym2612 = LJ_YM2612_create(clockRate, outputSampleRate);
	unsigned int ymFlags = 0x0;

	if (flags & DEVICE_YM2612_DEBUG)
	{
		ymFlags |= LJ_YM2612_DEBUG;
	}
	if (flags & DEVICE_YM2612_NODAC)
	{
		ymFlags |= LJ_YM2612_NODAC;
	}
	if (flags & DEVICE_YM2612_NOFM)
	{
		ymFlags |= LJ_YM2612_NOFM;
	}
	if (flags & DEVICE_YM2612_ONECHANNEL)
	{
		const int channel = ((flags >> DEVICE_YM2612_ONECHANNEL_SHIFT) & DEVICE_YM2612_ONECHANNEL_MASK);
		ymFlags |= LJ_YM2612_ONECHANNEL;
		ymFlags |= ((unsigned int)channel << LJ_YM2612_ONECHANNEL_SHIFT);
		printf("flags:0x%X\n", ymFlags);
	}
	LJ_YM2612_setFlags(ym2612, ymFlags);
	return ym2612;
}

int device_ym2612_write(void* const ym2612, const int address, const int data)
{
	int result = LJ_VGM_TEST_ERROR;
	const LJ_YM_UINT8 notCS = 0;
	const LJ_YM_UINT8 notRD = 1;
	const LJ_YM_UINT8 notWR = 0;
	const LJ_YM_UINT8 A0 = (address >> 0) & 0x1;
	const LJ_YM_UINT8 A1 = (address >> 1) & 0x1;
	result = LJ_YM2612_setDataPinsD07(ym2612, (LJ_YM_UINT8)data);
	result = LJ_YM2612_setAddressPinsCSRDWRA1A0(ym2612, notCS, notRD, notWR, A1, A0);
	
	return result;
}

int device_ym2612_getStatus(void* const ym2612, const int address, unsigned char* status)
{
	int result = LJ_VGM_TEST_ERROR;
	const LJ_YM_UINT8 notCS = 0;
	const LJ_YM_UINT8 notRD = 0;
	const LJ_YM_UINT8 notWR = 1;
	const LJ_YM_UINT8 A0 = (address >> 0) & 0x1;
	const LJ_YM_UINT8 A1 = (address >> 1) & 0x1;
	result = LJ_YM2612_setAddressPinsCSRDWRA1A0(ym2612, notCS, notRD, notWR, A1, A0);
	result = LJ_YM2612_getDataPinsD07(ym2612, status);
	
	return result;
}

int device_ym2612_generateOutput(void* const ym2612, const int numCycles, short* output[2])
{
	return LJ_YM2612_generateOutput(ym2612, numCycles, output);
}

int device_ym2612_destroy(void* ym2612)
{
	return LJ_YM2612_destroy(ym2612);
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
	strcat(wavOutputName, ".wav");
	return wavOutputName;
}

