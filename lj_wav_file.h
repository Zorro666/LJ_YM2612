#ifndef LJ_WAV_FILE_HH
#define LJ_WAV_FILE_HH

typedef struct LJ_WAV_FILE LJ_WAV_FILE;

typedef enum LJ_WAV_FORMAT LJ_WAV_FORMAT;
typedef enum LJ_WAV_RESULT LJ_WAV_RESULT;

enum LJ_WAV_FORMAT {
	LJ_WAV_FORMAT_PCM = 0x1,
	LJ_WAV_FORMAT_IEEE_FLOAT = 0x3,
	LJ_WAV_FORMAT_ALAW = 0x6,
	LJ_WAV_FORMAT_MULAW = 0x6,
	LJ_WAV_FORMAT_EXTENSIBLE = 0xFFFE,
};

enum LJ_WAV_RESULT {
	LJ_WAV_OK = 0,
	LJ_WAV_ERROR = -1,
};

LJ_WAV_FILE* LJ_WAV_create( const char* const filename, const LJ_WAV_FORMAT format, 
							const int numChannels, const int sampleRate, const int numBytesPerChannel );

LJ_WAV_RESULT LJ_WAV_FILE_writeChannel( LJ_WAV_FILE* const wavFile, void* sampleData);

LJ_WAV_RESULT LJ_WAV_close(LJ_WAV_FILE* const wavFile);

#endif // #ifndef LJ_WAV_FILE_HH

