#include "lj_gym.h"
#include "lj_ym2612.h"

#include <malloc.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	int result = LJ_GYM_OK;

	LJ_GYM_FILE* gymFile = NULL;
	LJ_GYM_INSTRUCTION gymInstruction;

	LJ_YM2612* ym2612 = NULL;

	LJ_YM_INT16* outputs[2];

	FILE* outFileH = fopen("gym_test.pcm","wb");

	int numCycles = 1;
	int cmdCount;
	int outputSampleRate = 44100;

	outputs[0] = malloc(1024 * sizeof(LJ_YM_INT16));
	outputs[1] = malloc(1024 * sizeof(LJ_YM_INT16));

	gymFile = LJ_GYM_create( "test.gym" );
	ym2612 = LJ_YM2612_create(LJ_YM2612_DEFAULT_CLOCK_RATE, outputSampleRate);

	cmdCount = 0;
	while (result == LJ_GYM_OK)
	{
		result = LJ_GYM_read(gymFile, &gymInstruction);
		if (result == LJ_GYM_OK)
		{
			const LJ_YM_UINT8 notCS = 0;
			const LJ_YM_UINT8 notRD = 1;
			const LJ_YM_UINT8 notWR = 0;
			LJ_GYM_debugPrint( &gymInstruction);
			if (gymInstruction.cmd == LJ_GYM_WRITE_PORT_0)
			{
				const LJ_YM_UINT8 A1 = 0; /* part 0 */
				result = LJ_YM2612_setDataPinsD07(ym2612, gymInstruction.R);
				result = LJ_YM2612_setAddressPinsCSRDWRA1A0(ym2612, notCS, notRD, notWR, A1, 0);
				result = LJ_YM2612_setDataPinsD07(ym2612, gymInstruction.D);
				result = LJ_YM2612_setAddressPinsCSRDWRA1A0(ym2612, notCS, notRD, notWR, A1, 1);
			}
			else if (gymInstruction.cmd == LJ_GYM_WRITE_PORT_1)
			{
				const LJ_YM_UINT8 A1 = 1; /* part 1 */
				result = LJ_YM2612_setDataPinsD07(ym2612, gymInstruction.R);
				result = LJ_YM2612_setAddressPinsCSRDWRA1A0(ym2612, notCS, notRD, notWR, A1, 0);
				result = LJ_YM2612_setDataPinsD07(ym2612, gymInstruction.D);
				result = LJ_YM2612_setAddressPinsCSRDWRA1A0(ym2612, notCS, notRD, notWR, A1, 1);
			}
			if (result == LJ_YM2612_ERROR)
			{
				fprintf(stderr,"GYM:%d ERROR processing command\n",cmdCount);
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

						/* LR LR LR LR LR LR */
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

	LJ_GYM_destroy(gymFile);
	LJ_YM2612_destroy(ym2612);

	return -1;
}
