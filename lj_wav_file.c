#include "lj_wav_file.h"

#include <string.h>
#include <malloc.h>
#include <stdio.h>

/*
RIFF_HEADER: little-endian
{
riffID		Bytes 4	: Chunk ID: "RIFF"
chunkSize	Bytes 4	: Chunk size: 4+n
wavID		Bytes 4	: WAVE ID: "WAVE"
}
chunkData	Bytes n : chunk data e.g. WAV chunk
*/

/*
WAV_HEADER: little-endian
{
chunkID					Bytes 4		: Chunk ID: "fmt "
chunkSize				Bytes 4		: Chunk size: 16 or 18 or 40
wFormatTag				Bytes 2		: Format code
nChannels				Bytes 2		: Number of interleaved channels
nSamplesPerSec			Bytes 4		: Sampling rate (blocks per second)
nAvgBytesPerSec			Bytes 4		: Data rate
nBlockAlign				Bytes 2		: Data block size (bytes)
wBitsPerSample			Bytes 2		: Bits per sample
}
// chunkSize = 18
{
cbSize					Bytes 2		: Size of the extension (0 or 22)
}
// chunkSize = 40
{
wValidBitsPerSample		Bytes 2		: Number of valid bits
dwChannelMask			Bytes 4		: Speaker position mask
SubFormat				Bytes 16	: GUID, including the data format code
}

wFormatTag: Format Codes:
0x0001	WAVE_FORMAT_PCM			PCM
0x0003	WAVE_FORMAT_IEEE_FLOAT	IEEE float
0x0006	WAVE_FORMAT_ALAW		8-bit	ITU-T G.711 A-law
0x0007	WAVE_FORMAT_MULAW		8-bit	ITU-T G.711 ?-law
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

#define LJ_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((unsigned int)((unsigned char)(ch0) << 0) | ((unsigned int)(unsigned char)(ch1) << 8) |   \
                ((unsigned int )((unsigned char)(ch2)) << 16) | ((unsigned int)(unsigned char)(ch3) << 24 ))

struct LJ_WAV_RIFF_HEADER
{
	LJ_WAV_UINT32 chunkID;			/* "RIFF" */
	LJ_WAV_UINT32 chunkSize;		/* 4+chunkDataSize */
	LJ_WAV_UINT32 wavID;				/* "WAVE" */
};

struct LJ_WAV_FORMAT_HEADER
{
	LJ_WAV_UINT32 chunkID;					/* "fmt " */
	LJ_WAV_UINT32 chunkSize;				/* 16  */
	LJ_WAV_UINT16 wFormatTag;			 	/* PCM=0x1, IEEE_FLOAT=0x3, ALAW=0x6, MULAW=0x7, EXTENSIBLE = 0xFFFE */
	LJ_WAV_UINT16 nChannels;				/* number of interleaved channels */
	LJ_WAV_UINT32 nSamplesPerSec;		/* sampling rate (blocks per second) */
	LJ_WAV_UINT32 nAvgBytesPerSec;	/* data rate (bytes per second) */
	LJ_WAV_UINT16 nBlockAlign;			/* data block size in bytes */
	LJ_WAV_UINT16 wBitsPerSample;		/* bits per sample */
};

struct LJ_WAV_DATA_HEADER
{
	LJ_WAV_UINT32 chunkID;			/* "data" */
	LJ_WAV_UINT32 chunkSize;		/* wBitsPerSamples * 8 * nChannels * numSamples */
};

struct LJ_WAV_FILE
{
	FILE* fileH;
	LJ_WAV_FORMAT format;
	int numChannels;
	int sampleRate;
	int numBytesPerChannel;
	int numBytesWritten;
};

/*
* //////////////////////////////////////////////////////////////////////////////
* // 
* // External Data and functions
* // 
* //////////////////////////////////////////////////////////////////////////////
*/

LJ_WAV_FILE* LJ_WAV_create( const char* const filename, const LJ_WAV_FORMAT format, 
							const int numChannels, const int sampleRate, const int numBytesPerChannel )
{
	LJ_WAV_RIFF_HEADER riffHeader;
	LJ_WAV_FORMAT_HEADER wavHeader;
	LJ_WAV_DATA_HEADER dataHeader;
	int result;
	LJ_WAV_FILE* wavFile = NULL;
	FILE* fileH = NULL;

	wavFile = malloc(sizeof(LJ_WAV_FILE));
	if (wavFile == NULL)
	{
		fprintf(stderr,"LJ_WAV_create: malloc LJ_WAV_FILE failed '%s'\n", filename);
		return NULL;
	}
	fileH = fopen(filename, "wb");
	if (fileH == NULL)
	{
		fprintf(stderr,"LJ_WAV_create: failed to open output file '%s'\n", filename);
		return NULL;
	}

	riffHeader.chunkID = LJ_MAKEFOURCC('R','I','F','F');
	riffHeader.chunkSize = 0; /* 4 + (8 + 16) + (8 + (wBitsPerSamples * 8 * nChannels * numSamples)) */
	riffHeader.wavID = LJ_MAKEFOURCC('W','A','V','E');

	result = (int)fwrite(&riffHeader, sizeof(LJ_WAV_RIFF_HEADER), 1, fileH);
	if (result != 1)
	{
		fprintf(stderr,"LJ_WAV_create: failed to write WAV_RIFF_HEADER file '%s'\n", filename);
		return NULL;
	}

	wavHeader.chunkID = LJ_MAKEFOURCC('f','m','t',' ');
	wavHeader.chunkSize = 16;
	wavHeader.wFormatTag = format;
	wavHeader.nChannels = (LJ_WAV_UINT16)numChannels;
	wavHeader.nSamplesPerSec = (LJ_WAV_UINT32)sampleRate;
	wavHeader.nAvgBytesPerSec = (LJ_WAV_UINT32)sampleRate * (LJ_WAV_UINT32)numBytesPerChannel * (LJ_WAV_UINT32)numChannels;
	wavHeader.nBlockAlign = (LJ_WAV_UINT16)(numBytesPerChannel * numChannels);
	wavHeader.wBitsPerSample = (LJ_WAV_UINT16)(8 * numBytesPerChannel);

	result = (int)fwrite(&wavHeader, sizeof(LJ_WAV_FORMAT_HEADER), 1, fileH);
	if (result != 1)
	{
		fprintf(stderr,"LJ_WAV_create: failed to write WAV_FORMAT_HEADER file '%s'\n", filename);
		return NULL;
	}

	dataHeader.chunkID = LJ_MAKEFOURCC('d','a','t','a');
	dataHeader.chunkSize = 0; /* wBitsPerSamples * 8 * nChannels * numSamples */

	result = (int)fwrite(&dataHeader, sizeof(LJ_WAV_DATA_HEADER), 1, fileH);
	if (result != 1)
	{
		fprintf(stderr,"LJ_WAV_create: failed to write WAV_DATA_HEADER file '%s'\n", filename);
		return NULL;
	}

	wavFile->format = format;
	wavFile->numChannels = numChannels;
	wavFile->sampleRate = sampleRate;
	wavFile->numBytesPerChannel = numBytesPerChannel;
	wavFile->numBytesWritten = 0;
	wavFile->fileH = fileH;

	return wavFile;
}

LJ_WAV_RESULT LJ_WAV_FILE_writeChannel( LJ_WAV_FILE* const wavFile, void* sampleData)
{
	int result;
	if (wavFile == NULL)
	{
		fprintf(stderr,"LJ_WAV_writeChannel: wavFile is NULL\n");
		return LJ_WAV_ERROR;
	}

	result = (int)fwrite(sampleData, (size_t)(wavFile->numBytesPerChannel), 1, wavFile->fileH);
	if (result != 1)
	{
		fprintf(stderr,"LJ_WAV_writeChannel: error writing sample\n");
		fclose(wavFile->fileH);
		return LJ_WAV_ERROR;
	}
	wavFile->numBytesWritten += wavFile->numBytesPerChannel;

	return LJ_WAV_OK;
}

LJ_WAV_RESULT LJ_WAV_close(LJ_WAV_FILE* const wavFile)
{
	const int riffChunkSizeOffset = 4;
	const int dataChunkSizeOffset = sizeof(LJ_WAV_RIFF_HEADER)+ sizeof(LJ_WAV_FORMAT_HEADER) + 4;
	int result;
	int riffChunkSize;
	int dataChunkSize;

	if (wavFile == NULL)
	{
		fprintf(stderr,"LJ_WAV_close: wavFile is NULL\n");
		return LJ_WAV_ERROR;
	}

	dataChunkSize = wavFile->numBytesWritten;
	riffChunkSize = 4 + (8 + 16) + (8 + dataChunkSize);

	/*Seek back and update riff chunkSize and data chunkSize */
	result = fseek(wavFile->fileH, riffChunkSizeOffset, SEEK_SET);
	if (result != 0)
	{
		fprintf(stderr,"LJ_WAV_close: seek riffChunkSize:%d failed\n", riffChunkSizeOffset);
		fclose(wavFile->fileH);
		return LJ_WAV_ERROR;
	}
	result = (int)fwrite(&riffChunkSize, sizeof(riffChunkSize), 1, wavFile->fileH);
	if (result != 1)
	{
		fprintf(stderr,"LJ_WAV_close: error writing riffChunkSize:%d at offset:%d\n", riffChunkSize, riffChunkSizeOffset);
		fclose(wavFile->fileH);
		return LJ_WAV_ERROR;
	}

	result = fseek(wavFile->fileH, dataChunkSizeOffset, SEEK_SET);
	if (result != 0)
	{
		fprintf(stderr,"LJ_WAV_close: seek dataChunkSize:%d failed\n", dataChunkSizeOffset);
		fclose(wavFile->fileH);
		return LJ_WAV_ERROR;
	}
	result = (int)fwrite(&dataChunkSize, sizeof(dataChunkSize), 1, wavFile->fileH);
	if (result != 1)
	{
		fprintf(stderr,"LJ_WAV_close: error writing dataChunkSize:%d at offset:%d\n", dataChunkSize, dataChunkSizeOffset);
		fclose(wavFile->fileH);
		return LJ_WAV_ERROR;
	}

	fclose(wavFile->fileH);
	return LJ_WAV_OK;
}
