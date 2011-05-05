#ifndef VGM_TEST_MAIN_HH
#define VGM_TEST_MAIN_HH

enum DVICE_YM2612_FLAGS { 
	DEVICE_YM2612_DEBUG = 				(1<<0),
	DEVICE_YM2612_NODAC = 				(1<<1),
	DEVICE_YM2612_NOFM = 				(1<<2),
	DEVICE_YM2612_ONECHANNEL =			(1<<3),
	DEVICE_YM2612_ONECHANNEL_SHIFT =	(4),
	DEVICE_YM2612_ONECHANNEL_MASK =		(0x7),
	DEVICE_YM2612_NEXT_DEBUG_THINGY =	(1<<7),
};

void* device_ym2612_create(const unsigned int flags);
int device_ym2612_write(void* const ym2612, const int address, const int data);
int device_ym2612_generateOutput(void* const ym2612, const int numCycles, short* output[2]);
int device_ym2612_destroy(void* ym2612);

const char* const getWavOutputName(const char* const inputName);

#define LJ_VGM_TEST_OK (0)
#define LJ_VGM_TEST_ERROR (-1)

#endif // #ifndef VGM_TEST_MAIN_HH

