#include "lj_vgm.h"
#include "lj_ym2612.h"

#include <malloc.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	int result = LJ_VGM_OK;

	LJ_VGM_FILE* vgmFile = NULL;
	LJ_VGM_INSTRUCTION vgmInstruction;

	LJ_YM2612* ym2612 = NULL;

	LJ_YM_INT16* outputs[2];
	outputs[0] = malloc(1024 * sizeof(LJ_YM_INT16));
	outputs[1] = malloc(1024 * sizeof(LJ_YM_INT16));

	FILE* outFileH = fopen("vgm_test.pcm","wb");

	int numCycles = 1;
	int cmdCount;

	vgmFile = LJ_VGM_create( "test.vgm" );
	ym2612 = LJ_YM2612_create();

	cmdCount = 0;
	while (result == LJ_VGM_OK)
	{
		result = LJ_VGM_read(vgmFile, &vgmInstruction);
		if (result == LJ_VGM_OK)
		{
			LJ_VGM_debugPrint( &vgmInstruction);
			if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_0)
			{
				result = LJ_YM2612_setRegister(ym2612, 0, vgmInstruction.R, vgmInstruction.D);
			}
			else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_1)
			{
				result = LJ_YM2612_setRegister(ym2612, 1, vgmInstruction.R, vgmInstruction.D);
			}
			if (result == LJ_YM2612_ERROR)
			{
				fprintf(stderr,"VGM:%d ERROR processing command\n",cmdCount);
				result = LJ_YM2612_OK;
			}
			if (result == LJ_YM2612_OK)
			{
				result = LJ_YM2612_generateOutput(ym2612, numCycles, outputs);
				if (result == LJ_YM2612_OK)
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
			cmdCount++;
			if (cmdCount == -1000)
			{
				result = LJ_YM2612_ERROR;
			}
		}
	}

	LJ_VGM_destroy(vgmFile);
	LJ_YM2612_destroy(ym2612);

	return -1;
}
