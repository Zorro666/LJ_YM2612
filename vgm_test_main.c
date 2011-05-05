#include "lj_vgm.h"
#include "lj_wav_file.h"
#include "vgm_test_main.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[])
{
	int result = LJ_VGM_OK;

	int numCycles = 1;
	int sampleCount;
	int waitCount;
	int i;

	LJ_VGM_FILE* vgmFile = NULL;
	LJ_VGM_INSTRUCTION vgmInstruction;

	void* ym2612;
	unsigned int flags;

	LJ_WAV_FILE* wavFile;
	short* outputs[2];

	const char* inputFileName = "test.vgm";

	int outputNumChannels = 2;
	const int OUTPUT_NUM_BYTES_PER_CHANNEL = 2;
	const int OUTPUT_SAMPLE_RATE = 44100;
	const char* wavOutputName = NULL;

	int debug = 0;
	int nodac = 0;
	int nofm = 0;
	int mono = 1;
	int stereo = 0;
	int channel = 0;
	int noerror = 0;

	for (i=1; i<argc; i++)
	{
		const char* option = argv[i];
		if (option[0] == '-')
		{
			if (strcmp(option+1, "debug") == 0)
			{
				debug = 1;
			}
			else if (strcmp(option+1, "mono") == 0)
			{
				mono = 1;
				stereo = 0;
			}
			else if (strcmp(option+1, "stereo") == 0)
			{
				mono = 0;
				stereo = 1;
			}
			else if (strcmp(option+1, "nodac") == 0)
			{
				nodac = 1;
			}
			else if (strcmp(option+1, "nofm") == 0)
			{
				nofm = 1;
			}
			else if (strcmp(option+1, "noerror") == 0)
			{
				noerror = 1;
			}
		}
		else
		{
			inputFileName = argv[i];
		}
	}
	printf("inputFileName:'%s'\n", inputFileName);
	printf("debug:%d\n", debug);
	printf("mono:%d\n", mono);
	printf("stereo:%d\n", stereo);
	printf("nodac:%d\n", nodac);
	printf("nofm:%d\n", nofm);
	printf("noerror:%d\n", nofm);
	printf("channel:%d\n", nofm);
	printf("\n");

	flags = 0x0;
	if (debug == 1)
	{
		flags |= DEVICE_YM2612_DEBUG;
	}
	if (nodac == 1)
	{
		flags |= DEVICE_YM2612_NODAC;
	}
	if (nofm == 1)
	{
		flags |= DEVICE_YM2612_NOFM;
	}
	if (channel > 0)
	{
		flags |= DEVICE_YM2612_ONECHANNEL;
		flags |= (channel << DEVICE_YM2612_ONECHANNEL_SHIFT);
	}

	ym2612 = device_ym2612_create(flags);
	if (ym2612 == NULL)
	{
		fprintf(stderr,"device_ym2612_create() failed\n");
		return -1;
	}

	outputs[0] = malloc(1024 * sizeof(short));
	if (outputs[0] == NULL)
	{
		fprintf(stderr,"outputs[0] malloc %d failed\n", 1024*sizeof(short));
		return -1;
	}
	outputs[1] = malloc(1024 * sizeof(short));
	if (outputs[1] == NULL)
	{
		fprintf(stderr,"outputs[1] malloc %d failed\n", 1024*sizeof(short));
		return -1;
	}

	vgmFile = LJ_VGM_create( inputFileName );
	if (vgmFile == NULL)
	{
		fprintf(stderr,"LJ_VGM_create() failed '%s'\n", inputFileName);
		return -1;
	}

	wavOutputName = getWavOutputName(inputFileName);
	printf("wavOutputName:'%s'\n", wavOutputName);
	if (mono == 1) 
	{
		outputNumChannels = 1;
	}
	else if (stereo == 1) 
	{
		outputNumChannels = 2;
	}

	wavFile = LJ_WAV_create( wavOutputName, LJ_WAV_FORMAT_PCM, outputNumChannels, OUTPUT_SAMPLE_RATE, OUTPUT_NUM_BYTES_PER_CHANNEL );
	if (wavFile == NULL)
	{
		fprintf(stderr,"LJ_WAV_create() failed '%s'\n", wavOutputName);
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
				else if (vgmInstruction.cmd == LJ_VGM_WAIT_60th)
				{
					waitCount += vgmInstruction.waitSamples;
				}
				else if (vgmInstruction.cmd == LJ_VGM_WAIT_50th)
				{
					waitCount += vgmInstruction.waitSamples;
				}
				if (result == LJ_VGM_TEST_ERROR)
				{
					fprintf(stderr,"VGM:%d ERROR processing command\n",sampleCount);
					result = LJ_VGM_TEST_ERROR;
					if (vgmInstruction.R == 0x22)
					{
						result = LJ_VGM_TEST_OK;
					}
					if (vgmInstruction.R == 0x27)
					{
						result = LJ_VGM_TEST_OK;
					}
					if (noerror == 1)
					{
						result = LJ_VGM_TEST_OK;
					}
				}
			}
		}
		if (result == LJ_VGM_OK)
		{
			if (debug == 1)
			{
				LJ_VGM_debugPrint( &vgmInstruction);
			}
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
						if (stereo == 1)
						{
							LJ_WAV_FILE_writeChannel(wavFile, outputRight+sample);
						}
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

	printf("\n");
	printf("inputFileName:'%s'\n", inputFileName);
	printf("wavOutputName:'%s'\n", wavOutputName);
	printf("Sample Count = %d\n", sampleCount);
	printf("Wait Count = %d\n", waitCount);
	printf("\n");

	LJ_WAV_close(wavFile);
	LJ_VGM_destroy(vgmFile);
	device_ym2612_destroy(ym2612);

	return 0;
}
