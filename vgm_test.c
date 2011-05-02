#include "lj_vgm.h"
#include "lj_ym2612.h"

#include <malloc.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	int result = LJ_VGM_OK;

	LJ_VGM_FILE* vgmFile = NULL;
	LJ_VGM_INSTRUCTION vgmInstruction;

	int numCycles = 1;
	int sampleCount;
	int waitCount;

	LJ_YM2612* ym2612 = NULL;

	LJ_YM_INT16* outputs[2];
	outputs[0] = malloc(1024 * sizeof(LJ_YM_INT16));
	outputs[1] = malloc(1024 * sizeof(LJ_YM_INT16));

	FILE* outFileH = fopen("vgm_test.pcm","wb");

	vgmFile = LJ_VGM_create( "test.vgm" );
	ym2612 = LJ_YM2612_create();

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
					result = LJ_YM2612_setRegister(ym2612, 0, vgmInstruction.R, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_1)
				{
					result = LJ_YM2612_setRegister(ym2612, 1, vgmInstruction.R, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_DATA)
				{
					result = LJ_YM2612_setRegister(ym2612, 0, vgmInstruction.R, vgmInstruction.D);
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
				if (result == LJ_YM2612_ERROR)
				{
					fprintf(stderr,"VGM:%d ERROR processing command\n",sampleCount);
					result = LJ_YM2612_OK;
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
				result = LJ_YM2612_generateOutput(ym2612, numCycles, outputs);
				if (result == LJ_VGM_OK)
				{
					int sample;
					for (sample = 0; sample < numCycles; sample++)
					{
						LJ_YM_INT16* outputLeft = outputs[0];
						LJ_YM_INT16* outputRight = outputs[1];

						// LR LR LR LR LR LR
						fwrite(outputLeft+sample, sizeof(LJ_YM_INT16), 1, outFileH);
						fwrite(outputRight+sample, sizeof(LJ_YM_INT16), 1, outFileH);
					}
				}
			}
		}
		vgmInstruction.waitSamples--;
		if (sampleCount == -1000)
		{
			result = LJ_YM2612_ERROR;
		}
	}

	printf("Sample Count = %d\n", sampleCount);
	printf("Wait Count = %d\n", waitCount);

	LJ_VGM_destroy(vgmFile);
	LJ_YM2612_destroy(ym2612);

	return -1;
}
