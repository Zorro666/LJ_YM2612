#include "lj_vgm.h"
#include "lj_ym2612.h"

#include <malloc.h>
#include <stdio.h>

/*
RIFF_HEADER: little-endian
{
riffID		Bytes 4	: Chunk ID: "RIFF"
chunkSize	Bytes 4	: Chunk size: 4+n
wavID			Bytes 4	: WAVE ID: "WAVE"
}
chunkData	Bytes n : chunk data e.g. WAV chunk
*/

/*
WAV_HEADER: little-endian
{
chunkID							Bytes 4		: Chunk ID: "fmt "
chunkSize						Bytes	4		: Chunk size: 16 or 18 or 40
wFormatTag					Bytes 2		: Format code
nChannels						Bytes 2		: Number of interleaved channels
nSamplesPerSec			Bytes 4		: Sampling rate (blocks per second)
nAvgBytesPerSec			Bytes 4		: Data rate
nBlockAlign					Bytes 2		: Data block size (bytes)
wBitsPerSample			Bytes 2		: Bits per sample
}
// chunkSize = 18
{
cbSize							Bytes 2		: Size of the extension (0 or 22)
}
// chunkSize = 40
{
wValidBitsPerSample	Bytes 2		: Number of valid bits
dwChannelMask				Bytes 4		: Speaker position mask
SubFormat						Bytes 16	: GUID, including the data format code
}

wFormatTag: Format Codes:
0x0001	WAVE_FORMAT_PCM					PCM
0x0003	WAVE_FORMAT_IEEE_FLOAT	IEEE float
0x0006	WAVE_FORMAT_ALAW	8-bit	ITU-T G.711 A-law
0x0007	WAVE_FORMAT_MULAW	8-bit	ITU-T G.711 µ-law
0xFFFE	WAVE_FORMAT_EXTENSIBLE	Determined by SubFormat

PCM Format

The first part of the Format chunk is used to describe PCM data.

For PCM data, the Format chunk in the header declares the number of bits/sample in each sample (wBitsPerSample). The original documentation (Revision 1) specified that the number of bits per sample is to be rounded up to the next multiple of 8 bits. This rounded-up value is the container size. This information is redundant in that the container size (in bytes) for each sample can also be determined from the block size divided by the number of channels (nBlockAlign / nChannels).
This redundancy has been appropriated to define new formats. For instance, Cool Edit uses a format which declares a sample size of 24 bits together with a container size of 4 bytes (32 bits) determined from the block size and number of channels. With this combination, the data is actually stored as 32-bit IEEE floats. The normalization (full scale 223) is however different from the standard float format.
PCM data is two's-complement except for resolutions of 1-8 bits, which are represented as offset binary.

*/

typedef unsigned int LJ_WAV_UINT32;
typedef unsigned short LJ_WAV_UINT16;

typedef struct LJ_WAV_RIFF_HEADER LJ_WAV_RIFF_HEADER;
typedef struct LJ_WAV_FORMAT_HEADER LJ_WAV_FORMAT_HEADER;
typedef struct LJ_WAV_DATA_HEADER LJ_WAV_DATA_HEADER;

typedef enum LJ_WAV_FORMAT LJ_WAV_FORMAT;

#define LJ_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((unsigned int)((unsigned char)(ch0) << 0) | ((unsigned int)(unsigned char)(ch1) << 8) |   \
                ((unsigned int )((unsigned char)(ch2)) << 16) | ((unsigned int)(unsigned char)(ch3) << 24 ))

enum LJ_WAV_FORMAT {
	LJ_WAV_FORMAT_PCM = 0x1,
	LJ_WAV_FORMAT_IEEE_FLOAT = 0x3,
	LJ_WAV_FORMAT_ALAW = 0x6,
	LJ_WAV_FORMAT_MULAW = 0x6,
	LJ_WAV_FORMAT_EXTENSIBLE = 0xFFFE,
};

struct LJ_WAV_RIFF_HEADER
{
	LJ_WAV_UINT32 chunkID;		// "RIFF"
	LJ_WAV_UINT32 chunkSize;	// 4+chunkDataSize
	LJ_WAV_UINT32 wavID;			// "WAVE"
};

struct LJ_WAV_FORMAT_HEADER
{
	LJ_WAV_UINT32 chunkID;							// "fmt "
	LJ_WAV_UINT32 chunkSize;						// 16 
	LJ_WAV_UINT16 wFormatTag;						// PCM=0x1, IEEE_FLOAT=0x3, ALAW=0x6, MULAW=0x7, EXTENSIBLE = 0xFFFE
	LJ_WAV_UINT16 nChannels;						// number of interleaved channels
	LJ_WAV_UINT32 nSamplesPerSec;				// sampling rate (blocks per second)
	LJ_WAV_UINT32 nAvgBytesPerSec;			// data rate (bytes per second)
	LJ_WAV_UINT16 nBlockAlign;					// data block size in bytes
	LJ_WAV_UINT16 wBitsPerSample;				// bits per sample
};

struct LJ_WAV_DATA_HEADER
{
	LJ_WAV_UINT32 chunkID;							// "data"
	LJ_WAV_UINT32 chunkSize;						// wBitsPerSamples * 8 * nChannels * numSamples
};

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

	FILE* outFileH = NULL;
	LJ_WAV_RIFF_HEADER riffHeader;
	LJ_WAV_FORMAT_HEADER wavHeader;
	LJ_WAV_DATA_HEADER dataHeader;

	const int OUTPUT_NUM_CHANNELS = 2;
	const int OUTPUT_NUM_BYTES_PER_CHANNEL = 2;
	const int OUTPUT_SAMPLE_RATE = 44100;

	outFileH = fopen("vgm_test.wav","wb");

	riffHeader.chunkID = LJ_MAKEFOURCC('R','I','F','F');
	riffHeader.chunkSize = 0; // 4 + (8 + 16) + (8 + (wBitsPerSamples * 8 * nChannels * numSamples))
	riffHeader.wavID = LJ_MAKEFOURCC('W','A','V','E');

	fwrite(&riffHeader, sizeof(LJ_WAV_RIFF_HEADER), 1, outFileH);

	wavHeader.chunkID = LJ_MAKEFOURCC('f','m','t',' ');
	wavHeader.chunkSize = 16;
	wavHeader.wFormatTag = LJ_WAV_FORMAT_PCM;
	wavHeader.nChannels = OUTPUT_NUM_CHANNELS;
	wavHeader.nSamplesPerSec = OUTPUT_SAMPLE_RATE;
	wavHeader.nAvgBytesPerSec = OUTPUT_SAMPLE_RATE * OUTPUT_NUM_BYTES_PER_CHANNEL * OUTPUT_NUM_CHANNELS;
	wavHeader.nBlockAlign = OUTPUT_NUM_BYTES_PER_CHANNEL * OUTPUT_NUM_CHANNELS;
	wavHeader.wBitsPerSample = 8 * OUTPUT_NUM_BYTES_PER_CHANNEL;

	fwrite(&wavHeader, sizeof(LJ_WAV_FORMAT_HEADER), 1, outFileH);

	dataHeader.chunkID = LJ_MAKEFOURCC('d','a','t','a');
	dataHeader.chunkSize = 0; // wBitsPerSamples * 8 * nChannels * numSamples

	fwrite(&dataHeader, sizeof(LJ_WAV_DATA_HEADER), 1, outFileH);

	vgmFile = LJ_VGM_create( "test.vgm" );
	ym2612 = LJ_YM2612_create();

	outputs[0] = malloc(1024 * sizeof(LJ_YM_INT16));
	outputs[1] = malloc(1024 * sizeof(LJ_YM_INT16));

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
					result = LJ_YM2612_write(ym2612, 0x4000, vgmInstruction.R);
					result = LJ_YM2612_write(ym2612, 0x4001, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_PORT_1)
				{
					result = LJ_YM2612_write(ym2612, 0x4002, vgmInstruction.R);
					result = LJ_YM2612_write(ym2612, 0x4003, vgmInstruction.D);
				}
				else if (vgmInstruction.cmd == LJ_VGM_YM2612_WRITE_DATA)
				{
					result = LJ_YM2612_write(ym2612, 0x4000, vgmInstruction.R);
					result = LJ_YM2612_write(ym2612, 0x4001, vgmInstruction.D);
					waitCount += vgmInstruction.waitSamples;
				}
#if 0
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
#endif
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
