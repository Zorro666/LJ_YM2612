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
	int vgmDebug = 0;
	int nodac = 0;
	int nofm = 0;
	int mono = 1;
	int stereo = 0;
	int channel = -1;
	int noerror = 0;
	int test = 0;
	int statusRead = 0;

	int sampleMin;
	int sampleMax;

	for (i = 1; i < argc; i++)
	{
		const char* option = argv[i];
		if (option[0] == '-')
		{
			/* Make option lower-case */
			const char* const optionStart = option+1;
			char* szPtr = (char*)optionStart;
			while (szPtr[0] != '\0')
			{
				char val = szPtr[0];
				char newVal = val;
				if ((val >= 'A') && (val <= 'Z'))
				{
					newVal = (char)(val + 'a' - 'A');
				}

				szPtr[0] = newVal;
				szPtr++;
			}
			if (strcmp(optionStart, "debug") == 0)
			{
				debug = 1;
			}
			else if (strcmp(optionStart, "vgmdebug") == 0)
			{
				vgmDebug = 1;
			}
			else if (strcmp(optionStart, "mono") == 0)
			{
				mono = 1;
				stereo = 0;
			}
			else if (strcmp(optionStart, "stereo") == 0)
			{
				mono = 0;
				stereo = 1;
			}
			else if (strcmp(optionStart, "nodac") == 0)
			{
				nodac = 1;
			}
			else if (strcmp(optionStart, "nofm") == 0)
			{
				nofm = 1;
			}
			else if (strcmp(optionStart, "noerror") == 0)
			{
				noerror = 1;
			}
			else if (strcmp(optionStart, "statusread") == 0)
			{
				statusRead = 1;
			}
			else if (strncmp(optionStart, "channel=", strlen("channel=")) == 0)
			{
				channel=atoi(optionStart+strlen("channel="));
			}
			else if (strncmp(optionStart, "test=", strlen("test=")) == 0)
			{
				test = 1;
				testName = optionStart+strlen("test=");
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
	printf("statusRead:%d\n", statusRead);
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
		flags |= ((unsigned int)channel << DEVICE_YM2612_ONECHANNEL_SHIFT);
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
		const char* ext;
		strcpy(vgmFileName, inputFileName);
		ext = strchr(inputFileName, '.');
		if (ext == NULL)
		{
			strcat(vgmFileName, ".vgm");
		}
		vgmFile = LJ_VGM_create(vgmFileName);
	}
	else
	{
		vgmFile = LJ_VGM_create(NULL);
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

	wavFile = LJ_WAV_create(wavOutputName, LJ_WAV_FORMAT_PCM, outputNumChannels, OUTPUT_SAMPLE_RATE, OUTPUT_NUM_BYTES_PER_CHANNEL);
	if (wavFile == NULL)
	{
		fprintf(stderr,"ERROR: LJ_WAV_create() failed '%s'\n", wavOutputName);
		return -1;
	}

	sampleCount = 0;
	waitCount = 0;
	memset(&vgmInstruction,0x0, sizeof(vgmInstruction));
	vgmInstruction.waitSamples = 0;
	sampleMin = (1<<30);
	sampleMax = -(1<<30);
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
					if (noerror == 1)
					{
						result = LJ_VGM_TEST_OK;
					}
				}
			}
		}
		if (result == LJ_VGM_OK)
		{
			if (vgmDebug == 1)
			{
				LJ_VGM_debugPrint(&vgmInstruction);
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

						/* LR LR LR LR LR LR */
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
				if (statusRead == 1)
				{
					const int address = 0x0;
					unsigned char status;
					result = device_ym2612_getStatus(ym2612, address, &status);
					if (result == LJ_VGM_OK)
					{
						if (status & 0x1)
						{
							printf("Sample[%d] Timer A overflow\n", sampleCount);
						}
						if (status & 0x2)
						{
							printf("Sample[%d] Timer B overflow\n", sampleCount);
						}
						/* Calling program needs to reset timer here to make them work again */
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

	if (test == 0)
	{
		LJ_VGM_HEADER header;
		if (LJ_VGM_getHeader(vgmFile, &header) == LJ_VGM_OK)
		{
			printf("VGM:numSamples:%d\n", header.numSamples);
			printf("VGM:loopOffset:%d\n", header.loopOffset);
			printf("VGM:numLoops:%d\n", header.numLoops);
		}
	}

	LJ_WAV_close(wavFile);
	LJ_VGM_destroy(vgmFile);
	device_ym2612_destroy(ym2612);

	return 0;
}

static LJ_VGM_UINT8 sampleDocProgram[141];
static LJ_VGM_UINT8 noteProgram[141];
static LJ_VGM_UINT8 noteDTProgram[141];
static LJ_VGM_UINT8 noteSLProgram[141];
static LJ_VGM_UINT8 algoProgram[141];
static LJ_VGM_UINT8 dacProgram[235];
static LJ_VGM_UINT8 fbProgram[147];
static LJ_VGM_UINT8 ch2Program[150];
static LJ_VGM_UINT8 badRegProgram[141];
static LJ_VGM_UINT8 timerProgram[147];
static LJ_VGM_UINT8 ssgOnProgram[141];
static LJ_VGM_UINT8 ssgHoldProgram[141];
static LJ_VGM_UINT8 ssgInvertProgram[141];
static LJ_VGM_UINT8 ssgAttackProgram[141];
static LJ_VGM_UINT8 ssgInvertHoldProgram[141];
static LJ_VGM_UINT8 ssgAttackHoldProgram[141];
static LJ_VGM_UINT8 ssgAttackInvertProgram[141];
static LJ_VGM_UINT8 ssgAttackInvertHoldProgram[141];

typedef struct LJ_VGM_TEST_PROGRAM
{
	LJ_VGM_UINT8* program;
	const char* const name;
} LJ_VGM_TEST_PROGRAM;

static LJ_VGM_TEST_PROGRAM testPrograms[64] = {
		{sampleDocProgram, "samp"},
		{noteProgram, "note"},
		{noteDTProgram, "notedt"},
		{noteSLProgram, "notesl"},
		{algoProgram, "algo"},
		{dacProgram, "dac"},
		{fbProgram, "fb"},
		{ch2Program, "ch2"},
		{badRegProgram, "badreg"},
		{timerProgram, "timer"},
		{ssgOnProgram, "ssgon"},
		{ssgHoldProgram, "ssgh"},
		{ssgInvertProgram, "ssgi"},
		{ssgAttackProgram, "ssga"},
		{ssgInvertHoldProgram, "ssgih"},
		{ssgAttackHoldProgram, "ssgah"},
		{ssgAttackInvertProgram, "ssgai"},
		{ssgAttackInvertHoldProgram, "ssgaih"},
		{NULL, "END"}
	};

static LJ_VGM_UINT8* currentTestInstruction = NULL;
LJ_VGM_RESULT startTestProgram(const char* const testName)
{
	LJ_VGM_TEST_PROGRAM* testProgram = testPrograms;
	do
	{
		if (testProgram->program == NULL)
		{
			return LJ_VGM_ERROR;
		}
		if (strcmp(testProgram->name, testName) == 0)
		{
			currentTestInstruction = testProgram->program;
			return LJ_VGM_OK;
		}
		testProgram++;
	}
	while (1);

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
#include "sampleDoc.prog"
};

static LJ_VGM_UINT8 noteProgram[] = {
#include "note.prog"
};

static LJ_VGM_UINT8 noteDTProgram[] = {
#include "noteDT.prog"
};

static LJ_VGM_UINT8 noteSLProgram[] = {
#include "noteSL.prog"
};

static LJ_VGM_UINT8 algoProgram[] = {
#include "algo.prog"
};

static LJ_VGM_UINT8 dacProgram[] = {
#include "dac.prog"
};

static LJ_VGM_UINT8 fbProgram[] = {
#include "fb.prog"
};

static LJ_VGM_UINT8 ch2Program[] = {
#include "ch2.prog"
};

static LJ_VGM_UINT8 badRegProgram[] = {
#include "badReg.prog"
};

static LJ_VGM_UINT8 timerProgram[] = {
#include "timer.prog"
};

static LJ_VGM_UINT8 ssgOnProgram[] = {
#include "ssgOn.prog"
};

static LJ_VGM_UINT8 ssgHoldProgram[] = {
#include "ssgHold.prog"
};

static LJ_VGM_UINT8 ssgInvertProgram[] = {
#include "ssgInvert.prog"
};

static LJ_VGM_UINT8 ssgAttackProgram[] = {
#include "ssgAttack.prog"
};

static LJ_VGM_UINT8 ssgInvertHoldProgram[] = {
#include "ssgInvertHold.prog"
};

static LJ_VGM_UINT8 ssgAttackHoldProgram[] = {
#include "ssgAttackHold.prog"
};

static LJ_VGM_UINT8 ssgAttackInvertProgram[] = {
#include "ssgAttackInvert.prog"
};

static LJ_VGM_UINT8 ssgAttackInvertHoldProgram[] = {
#include "ssgAttackInvertHold.prog"
};

