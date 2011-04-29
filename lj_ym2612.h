#ifndef LJ_YM2612_HH
#define LJ_YM2612_HH

typedef enum LJ_YM2612_RESULT LJ_YM2612_RESULT;

enum LJ_YM2612_RESULT { 
	LJ_YM2612_OK = 0,
	LJ_YM2612_ERROR = -1
};

typedef struct LJ_YM2612 LJ_YM2612;

LJ_YM2612* LJ_YM2612_create(void);
LJ_YM2612_RESULT LJ_YM2612_destroy(LJ_YM2612* const ym2612);

LJ_YM2612_RESULT LJ_YM2612_setRegister(LJ_YM2612* const ym2612, char port, char reg, char data);

#endif //#ifndef LJ_YM2612_HH
