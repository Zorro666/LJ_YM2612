#include "lj_vgm.h"
#include "emu.h"
#include "fm.h"

#include <malloc.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	int result = LJ_VGM_OK;

	LJ_VGM_FILE* gymFile = NULL;
	LJ_VGM_INSTRUCTION gymInstruction;

	void* ym2612 = NULL;

	FMSAMPLE* outputs[2];
	outputs[0] = malloc(1024 * sizeof(FMSAMPLE));
	outputs[1] = malloc(1024 * sizeof(FMSAMPLE));

	FILE* outFileH = fopen("gym_test2.pcm","wb");

	int numCycles = 1;
	int cmdCount;

	gymFile = LJ_VGM_create( "test.gym" );
	ym2612 = ym2612_init(NULL, NULL, 0, 0, NULL, NULL);
	ym2612_reset_chip(ym2612);

	cmdCount = 0;
	while (result == LJ_VGM_OK)
	{
		result = LJ_VGM_read(gymFile, &gymInstruction);
		if (result == LJ_VGM_OK)
		{
			LJ_VGM_debugPrint( &gymInstruction);
			if (gymInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_0)
			{
				ym2612_write(ym2612, 0x4000, gymInstruction.R);
				ym2612_write(ym2612, 0x4001, gymInstruction.D);
			}
			else if (gymInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_1)
			{
				ym2612_write(ym2612, 0x4002, gymInstruction.R);
				ym2612_write(ym2612, 0x4003, gymInstruction.D);
			}
			if (result == LJ_VGM_OK)
			{
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
			cmdCount++;
		}
	}

	LJ_VGM_destroy(gymFile);
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

