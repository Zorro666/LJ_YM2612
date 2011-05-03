#ifndef VGM_TEST_MAIN_HH
#define VGM_TEST_MAIN_HH

void* device_ym2612_create();
int device_ym2612_write(void* const ym2612, const int address, const int data);
int device_ym2612_generateOutput(void* const ym2612, const int numCycles, short* output[2]);
int device_ym2612_destroy(void* ym2612);

const char* const getWavOutputName(const char* const inputName);

#define LJ_VGM_TEST_OK (0)
#define LJ_VGM_TEST_ERROR (-1)

#endif // #ifndef VGM_TEST_MAIN_HH

