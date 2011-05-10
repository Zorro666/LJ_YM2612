#include "lj_vgm.h"
#include "lj_wav_file.h"
#include "vgm_test_main.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

LJ_VGM_RESULT startTestProgram(const char* const testName);
LJ_VGM_RESULT getNextTestProgramInstruction(LJ_VGM_INSTRUCTION* const vgmInstruction);

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
	const int YM2612_DEFAULT_CLOCK_RATE = 7670453;

	LJ_WAV_FILE* wavFile;
	short* outputs[2];

	const char* inputFileName = "test.vgm";
	const char* testName = "";

	int outputNumChannels = 2;
	const int OUTPUT_NUM_BYTES_PER_CHANNEL = 2;
	const int OUTPUT_SAMPLE_RATE = 44100;
	const char* wavOutputName = NULL;

	int debug = 0;
	int nodac = 0;
	int nofm = 0;
	int mono = 1;
	int stereo = 0;
	int channel = -1;
	int noerror = 0;
	int test = 0;

	for (i = 1; i < argc; i++)
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
			else if (strncmp(option+1, "channel=", strlen("channel=")) == 0)
			{
				channel=atoi(option+1+strlen("channel="));
			}
			else if (strncmp(option+1, "test=", strlen("test=")) == 0)
			{
				test = 1;
				testName = option+1+strlen("test=");
				inputFileName = "";
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
	printf("noerror:%d\n", noerror);
	printf("channel:%d\n", channel);
	printf("test:%d\n", test);
	printf("testName:'%s'\n", testName);
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
	if (channel >= 0)
	{
		flags |= DEVICE_YM2612_ONECHANNEL;
		flags |= (channel << DEVICE_YM2612_ONECHANNEL_SHIFT);
		printf("Channel:%d flags:0x%X\n", channel, flags);
	}

	ym2612 = device_ym2612_create(YM2612_DEFAULT_CLOCK_RATE, OUTPUT_SAMPLE_RATE, flags);
	if (ym2612 == NULL)
	{
		fprintf(stderr,"ERROR: device_ym2612_create() failed\n");
		return -1;
	}

	outputs[0] = malloc(1024 * sizeof(short));
	if (outputs[0] == NULL)
	{
		fprintf(stderr,"ERROR: outputs[0] malloc %d failed\n", 1024*sizeof(short));
		return -1;
	}
	outputs[1] = malloc(1024 * sizeof(short));
	if (outputs[1] == NULL)
	{
		fprintf(stderr,"ERROR: outputs[1] malloc %d failed\n", 1024*sizeof(short));
		return -1;
	}

	if (test == 0)
	{
		char vgmFileName[1024];
		strcpy(vgmFileName, inputFileName);
		const char* const ext = strchr(inputFileName, '.');
		if (ext == NULL)
		{
			strcat(vgmFileName, ".vgm");
		}
		vgmFile = LJ_VGM_create(vgmFileName);
	}
	else
	{
		vgmFile = LJ_VGM_create( NULL );
		if (startTestProgram(testName) == LJ_VGM_ERROR)
		{
			fprintf(stderr,"ERROR: startTestProgram '%s' program not found\n", testName);
			return -1;
		}
	}
	if (vgmFile == NULL)
	{
		fprintf(stderr,"ERROR: LJ_VGM_create() failed '%s'\n", inputFileName);
		return -1;
	}

	if (test == 0)
	{
		wavOutputName = getWavOutputName(inputFileName);
	}
	else
	{
		wavOutputName = getWavOutputName(testName);
	}
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
		fprintf(stderr,"ERROR: LJ_WAV_create() failed '%s'\n", wavOutputName);
		return -1;
	}

	sampleCount = 0;
	waitCount = 0;
	memset(&vgmInstruction,0x0, sizeof(vgmInstruction));
	vgmInstruction.waitSamples = 0;
	int sampleMin = (1<<30);
	int sampleMax = -(1<<30);
	while (result == LJ_VGM_OK)
	{
		if (vgmInstruction.waitSamples < 0)
		{
			vgmInstruction.waitSamples = 0;
		}
		if (vgmInstruction.waitSamples == 0)
		{
			if (test == 0)
			{
				result = LJ_VGM_read(vgmFile, &vgmInstruction);
			}
			else
			{
				result = getNextTestProgramInstruction(&vgmInstruction);
			}
			if (result == LJ_VGM_OK)
			{
				if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PART_0)
				{
					result = device_ym2612_write(ym2612, 0x4000, vgmInstruction.R);
					result = device_ym2612_write(ym2612, 0x4001, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PART_1)
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
					fprintf(stderr,"ERROR: VGM:%d ERROR processing command\n",sampleCount);
					result = LJ_VGM_TEST_ERROR;
					if ((vgmInstruction.R & 0xF0) == 0x40)
					{
						result = LJ_VGM_TEST_OK;
					}
					if ((vgmInstruction.R & 0xF0) == 0x50)
					{
						result = LJ_VGM_TEST_OK;
					}
					if ((vgmInstruction.R & 0xF0) == 0x60)
					{
						result = LJ_VGM_TEST_OK;
					}
					if ((vgmInstruction.R & 0xF0) == 0x70)
					{
						result = LJ_VGM_TEST_OK;
					}
					if ((vgmInstruction.R & 0xF0) == 0x80)
					{
						result = LJ_VGM_TEST_OK;
					}
					if ((vgmInstruction.R & 0xF0) == 0x90)
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
						if (outputLeft[sample] > sampleMax)
						{
							sampleMax = outputLeft[sample];
						}
						if (outputLeft[sample] < sampleMin)
						{
							sampleMin = outputLeft[sample];
						}
						if (outputRight[sample] > sampleMax)
						{
							sampleMax = outputRight[sample];
						}
						if (outputRight[sample] < sampleMin)
						{
							sampleMin = outputRight[sample];
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
	printf("sampleMin:%d\n", sampleMin);
	printf("sampleMax:%d\n", sampleMax);
	printf("\n");

	LJ_WAV_close(wavFile);
	LJ_VGM_destroy(vgmFile);
	device_ym2612_destroy(ym2612);

	return 0;
}

static LJ_VGM_UINT8 sampleDocProgram[];
static LJ_VGM_UINT8 noteProgram[];
static LJ_VGM_UINT8 noteDTProgram[];
static LJ_VGM_UINT8 algoProgram[];
static LJ_VGM_UINT8 dacTestProgram[];
static LJ_VGM_UINT8 feedbackProgram[];

static LJ_VGM_UINT8* currentTestInstruction = NULL;
LJ_VGM_RESULT startTestProgram(const char* const testName)
{
	if (strcmp(testName,"note") == 0)
	{
		currentTestInstruction = noteProgram;
		return LJ_VGM_OK;
	}
	else if (strcmp(testName,"noteDT") == 0)
	{
		currentTestInstruction = noteDTProgram;
		return LJ_VGM_OK;
	}
	else if (strcmp(testName,"algo") == 0)
	{
		currentTestInstruction = algoProgram;
		return LJ_VGM_OK;
	}
	else if (strcmp(testName,"dacTest") == 0)
	{
		currentTestInstruction = dacTestProgram;
		return LJ_VGM_OK;
	}
	else if (strcmp(testName,"sample") == 0)
	{
		currentTestInstruction = sampleDocProgram;
		return LJ_VGM_OK;
	}
	else if (strcmp(testName,"feedback") == 0)
	{
		currentTestInstruction = feedbackProgram;
		return LJ_VGM_OK;
	}

	return LJ_VGM_ERROR;
}

#define LJ_TEST_PART_0	(0x00)
#define LJ_TEST_PART_1	(0x01)
#define LJ_TEST_OUTPUT	(0x10)
#define LJ_TEST_FINISH	(0xFF)

LJ_VGM_RESULT getNextTestProgramInstruction(LJ_VGM_INSTRUCTION* const vgmInstruction)
{
	const LJ_VGM_UINT8 cmd = currentTestInstruction[0];
	const LJ_VGM_UINT8 reg = currentTestInstruction[1];
	const LJ_VGM_UINT8 data = currentTestInstruction[2];

	vgmInstruction->dataType = 0;
	vgmInstruction->dataNum = 0;
	vgmInstruction->dataSeekOffset = 0;

	if (cmd == LJ_TEST_FINISH)
	{
		vgmInstruction->cmd = LJ_VGM_END_SOUND_DATA;
		vgmInstruction->cmdCount++;
	}
	else if (cmd == LJ_TEST_OUTPUT)
	{
		vgmInstruction->cmd = LJ_VGM_WAIT_N_SAMPLES;
		vgmInstruction->waitSamples = (reg << 8) + data;
		vgmInstruction->waitSamplesData = vgmInstruction->waitSamples;
		vgmInstruction->cmdCount++;
		currentTestInstruction += 3;
	}
	else if (cmd == LJ_TEST_PART_0)
	{
		vgmInstruction->cmd = LJ_VGM_YM2612_WRITE_PART_0;
		vgmInstruction->R = reg;
		vgmInstruction->D = data;
		vgmInstruction->cmdCount++;
		currentTestInstruction += 3;
	}
	else if (cmd == LJ_TEST_PART_1)
	{
		vgmInstruction->cmd = LJ_VGM_YM2612_WRITE_PART_1;
		vgmInstruction->R = reg;
		vgmInstruction->D = data;
		vgmInstruction->cmdCount++;
		currentTestInstruction += 3;
	}
	return LJ_VGM_OK;
}

static LJ_VGM_UINT8 sampleDocProgram[] = {
		LJ_TEST_PART_0, 0x22, 0x00,	// LFO off
		LJ_TEST_PART_0, 0x27, 0x00,	// Channel 3 mode normal
		LJ_TEST_PART_0, 0x28, 0x00,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x01,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x02,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x04,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x05,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x06,	// All channels off
		LJ_TEST_PART_0, 0x2B, 0x00,	// DAC off
		LJ_TEST_PART_0, 0x30, 0x71,	// DT1/MUL - channel 0 slot 0
		LJ_TEST_PART_0, 0x34, 0x0D,	// DT1/MUL - channel 0 slot 2
		LJ_TEST_PART_0, 0x38, 0x33,	// DT1/MUL - channel 0 slot 1
		LJ_TEST_PART_0, 0x3C, 0x01,	// DT1/MUL - channel 0 slot 3
		LJ_TEST_PART_0, 0x40, 0x23,	// Total Level - channel 0 slot 0
		LJ_TEST_PART_0, 0x44, 0x2D,	// Total Level - channel 0 slot 2
		LJ_TEST_PART_0, 0x48, 0x26,	// Total Level - channel 0 slot 1
		LJ_TEST_PART_0, 0x4C, 0x00,	// Total Level - channel 0 slot 3
		LJ_TEST_PART_0, 0x50, 0x5F,	// RS/AR - channel 0 slot 0
		LJ_TEST_PART_0, 0x54, 0x99,	// RS/AR - channel 0 slot 2
		LJ_TEST_PART_0, 0x58, 0x5F,	// RS/AR - channel 0 slot 1
		LJ_TEST_PART_0, 0x5C, 0x94,	// RS/AR - channel 0 slot 3
		LJ_TEST_PART_0, 0x60, 0x05,	// AM/D1R - channel 0 slot 0
		LJ_TEST_PART_0, 0x64, 0x05,	// AM/D1R - channel 0 slot 2
		LJ_TEST_PART_0, 0x68, 0x05,	// AM/D1R - channel 0 slot 1
		LJ_TEST_PART_0, 0x6C, 0x07,	// AM/D1R - channel 0 slot 3
		LJ_TEST_PART_0, 0x70, 0x02,	// D2R - channel 0 slot 0
		LJ_TEST_PART_0, 0x74, 0x02,	// D2R - channel 0 slot 2
		LJ_TEST_PART_0, 0x78, 0x02,	// D2R - channel 0 slot 1
		LJ_TEST_PART_0, 0x7C, 0x02,	// D2R - channel 0 slot 3
		LJ_TEST_PART_0, 0x80, 0x11,	// D1L/RR - channel 0 slot 0
		LJ_TEST_PART_0, 0x84, 0x11,	// D1L/RR - channel 0 slot 2
		LJ_TEST_PART_0, 0x88, 0x11,	// D1L/RR - channel 0 slot 1
		LJ_TEST_PART_0, 0x8C, 0xA6,	// D1L/RR - channel 0 slot 3
		LJ_TEST_PART_0, 0x90, 0x00,	// SSG - channel 0 slot 0
		LJ_TEST_PART_0, 0x94, 0x00,	// SSG - channel 0 slot 2
		LJ_TEST_PART_0, 0x98, 0x00,	// SSG - channel 0 slot 1
		LJ_TEST_PART_0, 0x9C, 0x00,	// SSG - channel 0 slot 3
		LJ_TEST_PART_0, 0xB0, 0x32,	// Feedback/algorithm
		LJ_TEST_PART_0, 0xB4, 0xC0,	// Both speakers on
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_PART_0, 0xA4, 0x22,	// Set frequency
		LJ_TEST_PART_0, 0xA0, 0x69,	// Set frequency
		LJ_TEST_PART_0, 0x28, 0xF0,	// Key on
		LJ_TEST_OUTPUT, 0xB0, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_OUTPUT, 0x30, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_FINISH, 0xFF, 0xFF,	// END PROGRAM
};

static LJ_VGM_UINT8 noteProgram[] = {
		LJ_TEST_PART_0, 0x22, 0x00,	// LFO off
		LJ_TEST_PART_0, 0x27, 0x00,	// Channel 3 mode normal
		LJ_TEST_PART_0, 0x28, 0x00,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x01,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x02,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x04,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x05,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x06,	// All channels off
		LJ_TEST_PART_0, 0x2B, 0x00,	// DAC off
		LJ_TEST_PART_0, 0x30, 0x01,	// DT1/MUL - channel 0 slot 0 : DT=0 MUL=1
		LJ_TEST_PART_0, 0x34, 0x01,	// DT1/MUL - channel 0 slot 2 : DT=0 MUL=1
		LJ_TEST_PART_0, 0x38, 0x01,	// DT1/MUL - channel 0 slot 1 : DT=0 MUL=1
		LJ_TEST_PART_0, 0x3C, 0x01,	// DT1/MUL - channel 0 slot 3 : DT=0 MUL=1
		LJ_TEST_PART_0, 0x40, 0x00,	// Total Level - channel 0 slot 0 (*1)
		LJ_TEST_PART_0, 0x44, 0x7F,	// Total Level - channel 0 slot 2 (*0.000001f)
		LJ_TEST_PART_0, 0x48, 0x7F,	// Total Level - channel 0 slot 1 (*0.000001f)
		LJ_TEST_PART_0, 0x4C, 0x7F,	// Total Level - channel 0 slot 3 (*0.000001f)
		LJ_TEST_PART_0, 0x50, 0x1F,	// RS/AR - channel 0 slot 0
		LJ_TEST_PART_0, 0x54, 0x1F,	// RS/AR - channel 0 slot 2
		LJ_TEST_PART_0, 0x58, 0x1F,	// RS/AR - channel 0 slot 1
		LJ_TEST_PART_0, 0x5C, 0x1F,	// RS/AR - channel 0 slot 3
		LJ_TEST_PART_0, 0x60, 0x00,	// AM/D1R - channel 0 slot 0
		LJ_TEST_PART_0, 0x64, 0x00,	// AM/D1R - channel 0 slot 2
		LJ_TEST_PART_0, 0x68, 0x00,	// AM/D1R - channel 0 slot 1
		LJ_TEST_PART_0, 0x6C, 0x00,	// AM/D1R - channel 0 slot 3
		LJ_TEST_PART_0, 0x70, 0x00,	// D2R - channel 0 slot 0
		LJ_TEST_PART_0, 0x74, 0x00,	// D2R - channel 0 slot 2
		LJ_TEST_PART_0, 0x78, 0x00,	// D2R - channel 0 slot 1
		LJ_TEST_PART_0, 0x7C, 0x00,	// D2R - channel 0 slot 3
		LJ_TEST_PART_0, 0x80, 0x0F,	// D1L/RR - channel 0 slot 0
		LJ_TEST_PART_0, 0x84, 0x0F,	// D1L/RR - channel 0 slot 2
		LJ_TEST_PART_0, 0x88, 0x0F,	// D1L/RR - channel 0 slot 1
		LJ_TEST_PART_0, 0x8C, 0x0F,	// D1L/RR - channel 0 slot 3
		LJ_TEST_PART_0, 0x90, 0x00,	// SSG - channel 0 slot 0
		LJ_TEST_PART_0, 0x94, 0x00,	// SSG - channel 0 slot 2
		LJ_TEST_PART_0, 0x98, 0x00,	// SSG - channel 0 slot 1
		LJ_TEST_PART_0, 0x9C, 0x00,	// SSG - channel 0 slot 3
		LJ_TEST_PART_0, 0xB0, 0x07,	// Feedback/algorithm (FB=0, ALG=7)
		LJ_TEST_PART_0, 0xB4, 0xC0,	// Both speakers on
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_PART_0, 0xA4, 0x6A,	// Set frequency (BLOCK=7)
		LJ_TEST_PART_0, 0xA0, 0x69,	// Set frequency FREQ=???)
		LJ_TEST_PART_0, 0x28, 0x10,	// Key on (slot 0, channel 0)
		LJ_TEST_OUTPUT, 0xB0, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_OUTPUT, 0x30, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_FINISH, 0xFF, 0xFF,	// END PROGRAM
};

static LJ_VGM_UINT8 noteDTProgram[] = {
		LJ_TEST_PART_0, 0x22, 0x00,	// LFO off
		LJ_TEST_PART_0, 0x27, 0x00,	// Channel 3 mode normal
		LJ_TEST_PART_0, 0x28, 0x00,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x01,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x02,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x04,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x05,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x06,	// All channels off
		LJ_TEST_PART_0, 0x2B, 0x00,	// DAC off
		LJ_TEST_PART_0, 0x30, 0x3F,	// DT1/MUL - channel 0 slot 0 : DT=-3 MUL=2 -> *2
		LJ_TEST_PART_0, 0x34, 0x21,	// DT1/MUL - channel 0 slot 2 : DT=-1 MUL=1 -> *1
		LJ_TEST_PART_0, 0x38, 0x33,	// DT1/MUL - channel 0 slot 1 : DT=-2 MUL=3 -> *3
		LJ_TEST_PART_0, 0x3C, 0x03,	// DT1/MUL - channel 0 slot 3 : DT=+0 MUL=3 -> *3
		LJ_TEST_PART_0, 0x40, 0x00,	// Total Level - channel 0 slot 0
		LJ_TEST_PART_0, 0x44, 0x70,	// Total Level - channel 0 slot 2
		LJ_TEST_PART_0, 0x48, 0x70,	// Total Level - channel 0 slot 1
		LJ_TEST_PART_0, 0x4C, 0x7F,	// Total Level - channel 0 slot 3 (*0.0)
		LJ_TEST_PART_0, 0x50, 0x1F,	// RS/AR - channel 0 slot 0
		LJ_TEST_PART_0, 0x54, 0x1F,	// RS/AR - channel 0 slot 2
		LJ_TEST_PART_0, 0x58, 0x1F,	// RS/AR - channel 0 slot 1
		LJ_TEST_PART_0, 0x5C, 0x1F,	// RS/AR - channel 0 slot 3
		LJ_TEST_PART_0, 0x60, 0x00,	// AM/D1R - channel 0 slot 0
		LJ_TEST_PART_0, 0x64, 0x00,	// AM/D1R - channel 0 slot 2
		LJ_TEST_PART_0, 0x68, 0x00,	// AM/D1R - channel 0 slot 1
		LJ_TEST_PART_0, 0x6C, 0x00,	// AM/D1R - channel 0 slot 3
		LJ_TEST_PART_0, 0x70, 0x00,	// D2R - channel 0 slot 0
		LJ_TEST_PART_0, 0x74, 0x00,	// D2R - channel 0 slot 2
		LJ_TEST_PART_0, 0x78, 0x00,	// D2R - channel 0 slot 1
		LJ_TEST_PART_0, 0x7C, 0x00,	// D2R - channel 0 slot 3
		LJ_TEST_PART_0, 0x80, 0x0F,	// D1L/RR - channel 0 slot 0
		LJ_TEST_PART_0, 0x84, 0x0F,	// D1L/RR - channel 0 slot 2
		LJ_TEST_PART_0, 0x88, 0x0F,	// D1L/RR - channel 0 slot 1
		LJ_TEST_PART_0, 0x8C, 0x0F,	// D1L/RR - channel 0 slot 3
		LJ_TEST_PART_0, 0x90, 0x00,	// SSG - channel 0 slot 0
		LJ_TEST_PART_0, 0x94, 0x00,	// SSG - channel 0 slot 2
		LJ_TEST_PART_0, 0x98, 0x00,	// SSG - channel 0 slot 1
		LJ_TEST_PART_0, 0x9C, 0x00,	// SSG - channel 0 slot 3
		LJ_TEST_PART_0, 0xB0, 0x07,	// Feedback/algorithm (FB=0, ALG=7)
		LJ_TEST_PART_0, 0xB4, 0xC0,	// Both speakers on
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_PART_0, 0xA4, 0x30,	// Set frequency (BLOCK=7)
		LJ_TEST_PART_0, 0xA0, 0x69,	// Set frequency FREQ=???)
		LJ_TEST_PART_0, 0x28, 0x70,	// Key on (slot 0+1+2, channel 0)
		LJ_TEST_OUTPUT, 0xB0, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_OUTPUT, 0x30, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_FINISH, 0xFF, 0xFF,	// END PROGRAM
};

static LJ_VGM_UINT8 algoProgram[] = {
		LJ_TEST_PART_0, 0x22, 0x00,	// LFO off
		LJ_TEST_PART_0, 0x27, 0x00,	// Channel 3 mode normal
		LJ_TEST_PART_0, 0x28, 0x00,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x01,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x02,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x04,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x05,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x06,	// All channels off
		LJ_TEST_PART_0, 0x2B, 0x00,	// DAC off
		LJ_TEST_PART_0, 0x30, 0x09,	// DT1/MUL - channel 0 slot 0 : DT=0 MUL=0
		LJ_TEST_PART_0, 0x34, 0x0A,	// DT1/MUL - channel 0 slot 2 : DT=0 MUL=0
		LJ_TEST_PART_0, 0x38, 0x0B,	// DT1/MUL - channel 0 slot 1 : DT=0 MUL=0
		LJ_TEST_PART_0, 0x3C, 0x08,	// DT1/MUL - channel 0 slot 3 : DT=0 MUL=0
		LJ_TEST_PART_0, 0x40, 0x10,	// Total Level - channel 0 slot 0 (*0)
		LJ_TEST_PART_0, 0x44, 0x10,	// Total Level - channel 0 slot 2 (*0)
		LJ_TEST_PART_0, 0x48, 0x10,	// Total Level - channel 0 slot 1 (*1)
		LJ_TEST_PART_0, 0x4C, 0x10,	// Total Level - channel 0 slot 3 (*1)
		LJ_TEST_PART_0, 0x50, 0x1F,	// RS/AR - channel 0 slot 0
		LJ_TEST_PART_0, 0x54, 0x1F,	// RS/AR - channel 0 slot 2
		LJ_TEST_PART_0, 0x58, 0x1F,	// RS/AR - channel 0 slot 1
		LJ_TEST_PART_0, 0x5C, 0x1F,	// RS/AR - channel 0 slot 3
		LJ_TEST_PART_0, 0x60, 0x00,	// AM/D1R - channel 0 slot 0
		LJ_TEST_PART_0, 0x64, 0x00,	// AM/D1R - channel 0 slot 2
		LJ_TEST_PART_0, 0x68, 0x00,	// AM/D1R - channel 0 slot 1
		LJ_TEST_PART_0, 0x6C, 0x00,	// AM/D1R - channel 0 slot 3
		LJ_TEST_PART_0, 0x70, 0x00,	// D2R - channel 0 slot 0
		LJ_TEST_PART_0, 0x74, 0x00,	// D2R - channel 0 slot 2
		LJ_TEST_PART_0, 0x78, 0x00,	// D2R - channel 0 slot 1
		LJ_TEST_PART_0, 0x7C, 0x00,	// D2R - channel 0 slot 3
		LJ_TEST_PART_0, 0x80, 0x0F,	// D1L/RR - channel 0 slot 0
		LJ_TEST_PART_0, 0x84, 0x0F,	// D1L/RR - channel 0 slot 2
		LJ_TEST_PART_0, 0x88, 0x0F,	// D1L/RR - channel 0 slot 1
		LJ_TEST_PART_0, 0x8C, 0x0F,	// D1L/RR - channel 0 slot 3
		LJ_TEST_PART_0, 0x90, 0x00,	// SSG - channel 0 slot 0
		LJ_TEST_PART_0, 0x94, 0x00,	// SSG - channel 0 slot 2
		LJ_TEST_PART_0, 0x98, 0x00,	// SSG - channel 0 slot 1
		LJ_TEST_PART_0, 0x9C, 0x00,	// SSG - channel 0 slot 3
		LJ_TEST_PART_0, 0xB0, 0x07,	// Feedback/algorithm (FB=0, ALG=7)
		LJ_TEST_PART_0, 0xB4, 0xC0,	// Both speakers on
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_PART_0, 0xA4, 0x5A,	// Set frequency (BLOCK=7)
		LJ_TEST_PART_0, 0xA0, 0x69,	// Set frequency FREQ=???)
		LJ_TEST_PART_0, 0x28, 0xF0,	// Key on (slot 0 & 1 & 2 & 3, channel 0)
		LJ_TEST_OUTPUT, 0xB0, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_OUTPUT, 0x30, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_FINISH, 0xFF, 0xFF,	// END PROGRAM
};

static LJ_VGM_UINT8 dacTestProgram[] = {
		LJ_TEST_PART_0, 0x22, 0x00,	// LFO off
		LJ_TEST_PART_0, 0x27, 0x00,	// Channel 3 mode normal
		LJ_TEST_PART_0, 0x28, 0x00,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x01,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x02,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x04,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x05,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x06,	// All channels off
		LJ_TEST_PART_0, 0x2B, 0x80,	// DAC on
		LJ_TEST_PART_1, 0x32, 0x33,	// DT1/MUL - channel 5 slot 0 : DT=-3 MUL=2 -> *2
		LJ_TEST_PART_1, 0x36, 0x21,	// DT1/MUL - channel 5 slot 2 : DT=-1 MUL=1 -> *1
		LJ_TEST_PART_1, 0x3A, 0x33,	// DT1/MUL - channel 5 slot 1 : DT=-2 MUL=3 -> *3
		LJ_TEST_PART_1, 0x3E, 0x03,	// DT1/MUL - channel 5 slot 3 : DT=+0 MUL=3 -> *3
		LJ_TEST_PART_1, 0x42, 0x00,	// Total Level - channel 5 slot 0
		LJ_TEST_PART_1, 0x46, 0x70,	// Total Level - channel 5 slot 2
		LJ_TEST_PART_1, 0x4A, 0x70,	// Total Level - channel 5 slot 1
		LJ_TEST_PART_1, 0x4E, 0x7F,	// Total Level - channel 5 slot 3 (*0.0)
		LJ_TEST_PART_1, 0x52, 0x1F,	// RS/AR - channel 5 slot 0
		LJ_TEST_PART_1, 0x56, 0x1F,	// RS/AR - channel 5 slot 2
		LJ_TEST_PART_1, 0x5A, 0x1F,	// RS/AR - channel 5 slot 1
		LJ_TEST_PART_1, 0x5E, 0x1F,	// RS/AR - channel 5 slot 3
		LJ_TEST_PART_1, 0x62, 0x00,	// AM/D1R - channel 5 slot 0
		LJ_TEST_PART_1, 0x66, 0x00,	// AM/D1R - channel 5 slot 2
		LJ_TEST_PART_1, 0x6A, 0x00,	// AM/D1R - channel 5 slot 1
		LJ_TEST_PART_1, 0x6E, 0x00,	// AM/D1R - channel 5 slot 3
		LJ_TEST_PART_1, 0x72, 0x00,	// D2R - channel 5 slot 0
		LJ_TEST_PART_1, 0x76, 0x00,	// D2R - channel 5 slot 2
		LJ_TEST_PART_1, 0x7A, 0x00,	// D2R - channel 5 slot 1
		LJ_TEST_PART_1, 0x7E, 0x00,	// D2R - channel 5 slot 3
		LJ_TEST_PART_1, 0x82, 0x0F,	// D1L/RR - channel 5 slot 0
		LJ_TEST_PART_1, 0x86, 0x0F,	// D1L/RR - channel 5 slot 2
		LJ_TEST_PART_1, 0x8A, 0x0F,	// D1L/RR - channel 5 slot 1
		LJ_TEST_PART_1, 0x8E, 0x0F,	// D1L/RR - channel 5 slot 3
		LJ_TEST_PART_1, 0x92, 0x00,	// SSG - channel 5 slot 0
		LJ_TEST_PART_1, 0x96, 0x00,	// SSG - channel 5 slot 2
		LJ_TEST_PART_1, 0x9A, 0x00,	// SSG - channel 5 slot 1
		LJ_TEST_PART_1, 0x9E, 0x00,	// SSG - channel 5 slot 3
		LJ_TEST_PART_1, 0xB2, 0x07,	// Feedback/algorithm (FB=0, ALG=7) - channel 5
		LJ_TEST_PART_1, 0xB6, 0xC0,	// Both speakers on - channel 5
		LJ_TEST_PART_0, 0x28, 0x06,	// Key off - channel 5
		LJ_TEST_PART_1, 0xA6, 0x30,	// Set frequency (BLOCK=7) - channel 5
		LJ_TEST_PART_1, 0xA2, 0x69,	// Set frequency FREQ=???) - channel 5
		LJ_TEST_PART_0, 0x28, 0x76,	// Key on (slot 0+1+2, channel 5)
		LJ_TEST_PART_0, 0x2A, 0x70,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x60,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x50,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x40,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x30,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x20,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x10,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x00,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x10,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x20,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x30,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x40,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x50,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x60,	// DAC data
		LJ_TEST_OUTPUT, 0x00, 0x02,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x2A, 0x70,	// DAC data
		LJ_TEST_OUTPUT, 0xB0, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x28, 0x06,	// Key off - channel 5
		LJ_TEST_OUTPUT, 0x30, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_FINISH, 0xFF, 0xFF,	// END PROGRAM
};

static LJ_VGM_UINT8 feedbackProgram[] = {
		LJ_TEST_PART_0, 0x22, 0x00,	// LFO off
		LJ_TEST_PART_0, 0x27, 0x00,	// Channel 3 mode normal
		LJ_TEST_PART_0, 0x28, 0x00,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x01,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x02,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x04,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x05,	// All channels off
		LJ_TEST_PART_0, 0x28, 0x06,	// All channels off
		LJ_TEST_PART_0, 0x2B, 0x00,	// DAC off
		LJ_TEST_PART_0, 0x30, 0x01,	// DT1/MUL - channel 0 slot 0 : DT=0 MUL=1
		LJ_TEST_PART_0, 0x34, 0x01,	// DT1/MUL - channel 0 slot 2 : DT=0 MUL=1
		LJ_TEST_PART_0, 0x38, 0x01,	// DT1/MUL - channel 0 slot 1 : DT=0 MUL=1
		LJ_TEST_PART_0, 0x3C, 0x01,	// DT1/MUL - channel 0 slot 3 : DT=0 MUL=1
		LJ_TEST_PART_0, 0x40, 0x08,	// Total Level - channel 0 slot 0 (*1)
		LJ_TEST_PART_0, 0x44, 0x7F,	// Total Level - channel 0 slot 2 (*0.000001f)
		LJ_TEST_PART_0, 0x48, 0x7F,	// Total Level - channel 0 slot 1 (*0.000001f)
		LJ_TEST_PART_0, 0x4C, 0x7F,	// Total Level - channel 0 slot 3 (*0.000001f)
		LJ_TEST_PART_0, 0x50, 0x1F,	// RS/AR - channel 0 slot 0
		LJ_TEST_PART_0, 0x54, 0x1F,	// RS/AR - channel 0 slot 2
		LJ_TEST_PART_0, 0x58, 0x1F,	// RS/AR - channel 0 slot 1
		LJ_TEST_PART_0, 0x5C, 0x1F,	// RS/AR - channel 0 slot 3
		LJ_TEST_PART_0, 0x60, 0x00,	// AM/D1R - channel 0 slot 0
		LJ_TEST_PART_0, 0x64, 0x00,	// AM/D1R - channel 0 slot 2
		LJ_TEST_PART_0, 0x68, 0x00,	// AM/D1R - channel 0 slot 1
		LJ_TEST_PART_0, 0x6C, 0x00,	// AM/D1R - channel 0 slot 3
		LJ_TEST_PART_0, 0x70, 0x00,	// D2R - channel 0 slot 0
		LJ_TEST_PART_0, 0x74, 0x00,	// D2R - channel 0 slot 2
		LJ_TEST_PART_0, 0x78, 0x00,	// D2R - channel 0 slot 1
		LJ_TEST_PART_0, 0x7C, 0x00,	// D2R - channel 0 slot 3
		LJ_TEST_PART_0, 0x80, 0x0F,	// D1L/RR - channel 0 slot 0
		LJ_TEST_PART_0, 0x84, 0x0F,	// D1L/RR - channel 0 slot 2
		LJ_TEST_PART_0, 0x88, 0x0F,	// D1L/RR - channel 0 slot 1
		LJ_TEST_PART_0, 0x8C, 0x0F,	// D1L/RR - channel 0 slot 3
		LJ_TEST_PART_0, 0x90, 0x00,	// SSG - channel 0 slot 0
		LJ_TEST_PART_0, 0x94, 0x00,	// SSG - channel 0 slot 2
		LJ_TEST_PART_0, 0x98, 0x00,	// SSG - channel 0 slot 1
		LJ_TEST_PART_0, 0x9C, 0x00,	// SSG - channel 0 slot 3
		LJ_TEST_PART_0, 0xB0, 0x67,	// Feedback/algorithm (FB=6, ALG=7)
		LJ_TEST_PART_0, 0xB4, 0xC0,	// Both speakers on
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_PART_0, 0xA4, 0x6A,	// Set frequency (BLOCK=7)
		LJ_TEST_PART_0, 0xA0, 0x69,	// Set frequency FREQ=???)
		LJ_TEST_PART_0, 0x28, 0x10,	// Key on (slot 0, channel 0)
		LJ_TEST_OUTPUT, 0xB0, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_PART_0, 0x28, 0x00,	// Key off
		LJ_TEST_OUTPUT, 0x30, 0x00,	// OUTPUT SAMPLES
		LJ_TEST_FINISH, 0xFF, 0xFF,	// END PROGRAM
};


