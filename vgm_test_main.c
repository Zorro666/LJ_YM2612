#include "lj_vgm.h"
#include "lj_wav_file.h"
#include "vgm_test_main.h"

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

	void* ym2612 = NULL;

	short* outputs[2];

	LJ_WAV_FILE* wavFile = NULL;

	const int OUTPUT_NUM_CHANNELS = 2;
	const int OUTPUT_NUM_BYTES_PER_CHANNEL = 2;
	const int OUTPUT_SAMPLE_RATE = 44100;
	const char* const wavOutputName = getWavOutputName( "vgm_test" );

	wavFile = LJ_WAV_create( wavOutputName, LJ_WAV_FORMAT_PCM, OUTPUT_NUM_CHANNELS, OUTPUT_SAMPLE_RATE, OUTPUT_NUM_BYTES_PER_CHANNEL );
	if (wavFile == NULL)
	{
		return -1;
	}

	vgmFile = LJ_VGM_create( "test.vgm" );
	if (vgmFile == NULL)
	{
		return -1;
	}

	ym2612 = device_ym2612_create();
	if (ym2612 == NULL)
	{
		return -1;
	}

	outputs[0] = malloc(1024 * sizeof(short));
	if (outputs[0] == NULL)
	{
		return -1;
	}
	outputs[1] = malloc(1024 * sizeof(short));
	if (outputs[1] == NULL)
	{
		return -1;
	}

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
					result = device_ym2612_write(ym2612, 0x4000, vgmInstruction.R);
					result = device_ym2612_write(ym2612, 0x4001, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_1)
				{
					result = device_ym2612_write(ym2612, 0x4002, vgmInstruction.R);
					result = device_ym2612_write(ym2612, 0x4003, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_DATA)
				{
					result = device_ym2612_write(ym2612, 0x4000, vgmInstruction.R);
					result = device_ym2612_write(ym2612, 0x4001, vgmInstruction.D);
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
				if (result == LJ_VGM_TEST_ERROR)
				{
					fprintf(stderr,"VGM:%d ERROR processing command\n",sampleCount);
					result = LJ_VGM_TEST_OK;
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
				result = device_ym2612_generateOutput(ym2612, numCycles, outputs);
				if (result == LJ_VGM_OK)
				{
					int sample;
					for (sample = 0; sample < numCycles; sample++)
					{
						short* outputLeft = outputs[0];
						short* outputRight = outputs[1];

						// LR LR LR LR LR LR
						LJ_WAV_FILE_writeChannel(wavFile, outputLeft+sample);
						LJ_WAV_FILE_writeChannel(wavFile, outputRight+sample);
					}
				}
			}
		}
		vgmInstruction.waitSamples--;
		if (sampleCount == -1000)
		{
			result = LJ_VGM_TEST_ERROR;
		}
	}

	printf("Sample Count = %d\n", sampleCount);
	printf("Wait Count = %d\n", waitCount);

	LJ_WAV_close(wavFile);
	LJ_VGM_destroy(vgmFile);
	device_ym2612_destroy(ym2612);

	return 0;
}
