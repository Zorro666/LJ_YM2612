#ifndef LJ_YM2612_HH
#define LJ_YM2612_HH

typedef enum LJ_YM2612_RESULT LJ_YM2612_RESULT;

enum LJ_YM2612_RESULT { 
	LJ_YM2612_OK = 0,
	LJ_YM2612_ERROR = -1
};

enum LJ_YM2612_FLAGS { 
	LJ_YM2612_DEBUG = 	(1<<0),
	LJ_YM2612_NODAC = 	(1<<1),
	LJ_YM2612_NOFM = 	(1<<2),
};

typedef struct LJ_YM2612 LJ_YM2612;

typedef unsigned char LJ_YM_UINT8;
typedef unsigned short LJ_YM_UINT16;
typedef unsigned int LJ_YM_UINT32;

typedef short LJ_YM_INT16;

LJ_YM2612* LJ_YM2612_create(void);

LJ_YM2612_RESULT LJ_YM2612_setFlags(LJ_YM2612* const ym2612, const unsigned int flags);

LJ_YM2612_RESULT LJ_YM2612_destroy(LJ_YM2612* const ym2612);

LJ_YM2612_RESULT LJ_YM2612_write(LJ_YM2612* const ym2612, LJ_YM_UINT16 address, LJ_YM_UINT8 data);

LJ_YM2612_RESULT LJ_YM2612_generateOutput(LJ_YM2612* const ym2612, int numCycles, LJ_YM_INT16* output[2]);

#endif //#ifndef LJ_YM2612_HH
