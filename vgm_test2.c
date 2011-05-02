#include "lj_vgm.h"
#include "emu.h"
#include "fm.h"

#include <malloc.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	int result = LJ_VGM_OK;

	LJ_VGM_FILE* vgmFile = NULL;
	LJ_VGM_INSTRUCTION vgmInstruction;

	void* ym2612 = NULL;

	FMSAMPLE* outputs[2];

	FILE* outFileH = fopen("vgm_test2.pcm","wb");

	int numCycles = 1;
	int sampleCount;
	int waitCount;

	int clock = 7670453;
	int rate = 44100;

	outputs[0] = malloc(1024 * sizeof(FMSAMPLE));
	outputs[1] = malloc(1024 * sizeof(FMSAMPLE));

	vgmFile = LJ_VGM_create( "test.vgm" );
	//ym2612_init(void *param, device_t *device, int clock, int rate, FM_TIMERHANDLER timer_handler,FM_IRQHANDLER IRQHandler)
	ym2612 = ym2612_init(NULL, NULL, clock, rate, NULL, NULL);
	ym2612_reset_chip(ym2612);

	sampleCount = 0;
	waitCount = 0;
	vgmInstruction.waitSamples = 0;
	while (result == LJ_VGM_OK)
	{
		if (vgmInstruction.waitSamples < 0)
		{
			vgmInstruction.waitSamples = 0;
		}
		if (vgmInstruction.waitSamples == 0)
		{
			result = LJ_VGM_read(vgmFile, &vgmInstruction);
			if (result == LJ_VGM_OK)
			{
				if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_0)
				{
					ym2612_write(ym2612, 0x4000, vgmInstruction.R);
					ym2612_write(ym2612, 0x4001, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_1)
				{
					ym2612_write(ym2612, 0x4002, vgmInstruction.R);
					ym2612_write(ym2612, 0x4003, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_DATA)
				{
					ym2612_write(ym2612, 0x4000, vgmInstruction.R);
					ym2612_write(ym2612, 0x4001, vgmInstruction.D);
					waitCount += vgmInstruction.waitSamples;
				}
				else if (vgmInstruction.cmd == LJ_VGM_WAIT_N_SAMPLES)
				{
					waitCount += vgmInstruction.waitSamples;
				}
				else if (vgmInstruction.cmd == LJ_VGM_WAIT_SAMPLES)
				{
					waitCount += vgmInstruction.waitSamples;
				}
			}
		}
		if (result == LJ_VGM_OK)
		{
			LJ_VGM_debugPrint( &vgmInstruction);
		}
		if (vgmInstruction.cmd == LJ_VGM_END_SOUND_DATA)
		{
			break;
		}
		if (vgmInstruction.waitSamples > 0)
		{
			if (result == LJ_VGM_OK)
			{
				sampleCount++;
				ym2612_update_one(ym2612, outputs, numCycles);
				if (result == LJ_VGM_OK)
				{
					int sample;
					for (sample = 0; sample < numCycles; sample++)
					{
						FMSAMPLE* outputLeft = outputs[0];
						FMSAMPLE* outputRight = outputs[1];

						// LR LR LR LR LR LR
						fwrite(outputLeft+sample, sizeof(FMSAMPLE), 1, outFileH);
						fwrite(outputRight+sample, sizeof(FMSAMPLE), 1, outFileH);
					}
				}
			}
		}
		vgmInstruction.waitSamples--;
	}

	printf("Sample Count = %d\n", sampleCount);
	printf("Wait Count = %d\n", waitCount);

	LJ_VGM_destroy(vgmFile);
	ym2612_shutdown(ym2612);

	return -1;
}

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

