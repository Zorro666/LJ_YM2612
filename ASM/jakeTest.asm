CPU=68000

	dc.l 0x00000000,	EntryPoint,	ErrorTrap,	ErrorTrap	;00
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;10
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;20
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;30
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;40
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;50
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;60
	dc.l HInt,				ErrorTrap,	VInt,				ErrorTrap	;70
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;80
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;90
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;A0
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;B0
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;C0
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;D0
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;E0
	dc.l ErrorTrap,		ErrorTrap,	ErrorTrap,	ErrorTrap	;F0

; D0 = spare temporary
; D1 = spare temporary
; D2 = spare temporary
; D3 = used as loop counter inside output samples
; D4 = used inside UpdateGUI
; D5 = used inside UpdateGUI for joy pad reading
; D6 = used inside UpdateGUI for joy pad reading
; D7 = return value from UpdateGUI

check_sum equ	0x0000
;------------------------------------------------------------------------
	dc.b	'SEGA GENESIS    '	;100
	dc.b	'(C)T-01 2011.06 '	;110 release year.month
	dc.b	'JAKE YM2612 TEST'	;120 Japan title
	dc.b	':Sample Doc Prog'	;130
	dc.b	'                '	;140
	dc.b	'JAKE YM2612 TEST'	;150 US title
	dc.b	':Sample Doc Prog'	;160
	dc.b	'                '	;170
	dc.b	'GM T-123456 42'  	;180 product #, version
	dc.w	check_sum         	;18E check sum
	dc.b	'J               '	;190 controller
	dc.l	0x00000000, 0x0007ffff, 0x00ff0000, 0x00ffffff		;1A0
	dc.b	'                '	;1B0
	dc.b	'                '	;1C0
	dc.b	'                '	;1D0
	dc.b	'                '	;1E0
	dc.b	'JUE             '	;1F0

P_START	=	0x80
P_A			=	0x40
P_C			=	0x20
P_B			=	0x10
P_RIGHT	=	0x08
P_LEFT	=	0x04
P_DOWN	=	0x02
P_UP		=	0x01

T_PLAY	= 0x01
T_NEXT	= 0x02
T_PREV	= 0x04

GUI_UPDATE MACRO PAD_VALUE
	bsr 		UpdateGUI
	cmpi.b	#T_PLAY,		PAD_VALUE	
	beq 		StartProgram
	ENDM


ErrorTrap:
	jmp			ErrorTrap

HInt:
VInt:
	rte

EntryPoint:
	move.w	#0xFFFF,	D0
Loop1:
	dbf 		D0,				Loop1

Loop2:
	move.b	#0x01,		0xA11100	;BUSREQ
	move.b	#0x01,		0xA11200	;RESET
	btst.b	#0x00,		0xA11100
	bne			Loop2

	move.b	#0x40, 		0x10009		; init joypad 1 by writing 0x40 to CTRL port for joypad 1 
	
	; A1 is the start of the test program to run 
	lea			sampleDocProgram,						A1 
	lea			noteProgram,								A1 
	lea			noteDTProgram,							A1 
	lea			noteSLProgram,							A1 
	lea			algoProgram,								A1 
	lea			dacProgram,									A1 
	lea			fbProgram,									A1 
	lea			ch2Program,									A1 
	lea			badRegProgram,							A1
	lea			timerProgram,								A1
	lea			ssgOnProgram,								A1
	lea			ssgHoldProgram,							A1
	lea			ssgInvertProgram,						A1
	lea			ssgAttackProgram,						A1
	lea			ssgInvertHoldProgram,				A1
	lea			ssgAttackHoldProgram,				A1
	lea			ssgAttackInvertProgram,			A1
	lea			ssgAttackInvertHoldProgram,	A1

	lea			fbProgram,									A1

StartProgram:
	move 		A1,				A0

RunProgram:
	move.b	(A0)+,		D0				; D0 is the operation to perform

	cmpi.b	#0x00,		D0
	beq 		WritePort0

	cmpi.b	#0x01,		D0
	beq 		WritePort1

	cmpi.b	#0x10,		D0
	beq 		OutputSamples

	cmpi.b	#0xFF,		D0
	beq 		EndProgram
	
	jmp			ErrorTrap						; unknown operation to perform

WritePort0:
	move.b	(A0)+,		D0				; register
	move.b	(A0)+,		D1				; data
	bsr 		WriteYm2612Port0
	jmp 		RunProgram
	
WritePort1:										; NEVER TESTED
	move.b	(A0)+,		D0				; register
	move.b	(A0)+,		D1				; data
	bsr 		WriteYm2612Port1
	jmp			RunProgram

OutputSamples:								; loop a lot
	move.b	(A0)+,		D0				; bottom 8-bits
	move.b	(A0)+,		D1				; top 8-bits
	andi.w	#0xFF,		D0				; clear out upper 8-bits
	lsl.w		#0x8,			D1				; D1 = D1 << 8
	or.w		D1,				D0				; D0 = (D0 & 0xFF) | (D1 << 8)
OutputOneSample:
	move.w	#0x0080,	D3				; this should be the number of cycles to output a single 44KHz sample
OutputSample:
	GUI_UPDATE D7
	subi.w	#0x01,		D3
	bne 		OutputSample
	subi.w	#0x01,		D0
	bne 		OutputOneSample
	jmp 		RunProgram

EndProgram:
	GUI_UPDATE D7
	jmp			EndProgram

WriteYM2612Port0:
	btst		#0x07,		0xA04000
	bne 		WriteYM2612Port0
	move.b	D0,				0xA04000
WriteYM2612DataPort0:
	btst		#0x07,		0xA04000
	bne			WriteYM2612DataPort0
	move.b	D1,0xA04001
	rts

WriteYM2612Port1:
	btst		#0x07,		0xA04000
	bne 		WriteYM2612Port1
	move.b	D0,				0xA04002
WriteYM2612DataPort1:
	btst		#0x07,		0xA04000
	bne			WriteYM2612DataPort1
	move.b	D1,0xA04003
	rts

UpdateGUI:										; test for joypad input & draw screen text
	move.b	#0x00, 		D7				; D7 is result of what to do from this function

	move.b	#0x40,		0xA10003	; ask for high-state on joypad 1 by writing 0x40 to DATA port for joypad 1
	nop													; wait a couple of cycles
	nop													; wait a couple of cycles
	move.b 	0xA10003,	D2				; read joypad 1 : C-button, B-button + Right/Left/Down/Up

	move.b 	#0x00, 		0xA10003	; ask for low-state on joypad 1 by writing 0x00 to DATA port for joypad 1
	nop													; wait a couple of cycles
	nop													; wait a couple of cycles
	move.b 	0xA10003, D6				; read joypad 1 : Start-button + A-Button + Down/Up
	andi.b 	#0x3F, 		D2				; joyPadLow = joyPadLow & 0x3F
	andi.b 	#0x30, 		D6				; joyPadHigh = joyPadHigh & 0x30
	lsl.b 	#0x02, 		D6				; joyPadHigh = joyPadHigh << 2
	or.b		D2,				D6				; D6 = (joyPadLow & 0x3F) | ((joyPadHigh & 0x30) << 2 ))

	lea 		lastPad, 	A2
	move.b 	(A2), 		D4				; D4 is lastState, D6 is newState (1 = not pressed, 0 = pressed)
	eor.b 	D6, 			D4				; D4 = lastState ^ newState
	and.b 	D6, 			D4				; D4 = (lastState ^ newState) & newState

	move.b 	D4, 			D2				; test for Start which is bit-7 -> 0x80
	andi.b 	#P_START,	D2
	beq 		NotStart						; test for start button not-pressed (0) -> pressed (1)

	move.b 	#T_PLAY,	D7				; start was pressed
	jmp SavePadState

NotStart:
	move.b 	D4, 			D2				; test for A which is bit-6 -> 0x40
	andi.b 	#P_A,			D2
	beq 		NotA								; test for A button not-pressed (0) -> pressed (1)

	move.b 	#T_PLAY,	D7				; A button was pressed
	jmp SavePadState

NotA:

SavePadState:
	lea 		lastPad, 	A2
	move.b 	D6, 			(A2)
	rts
	
; Handy macros to make test programs easy to convert from C -> ASM

LJ_TEST_PART_0 MACRO I, R, D, J
	DB 0x00, R, D
	ENDM

LJ_TEST_PART_1 MACRO I, R, D, J
	DB 0x01, R, D
	ENDM

LJ_TEST_OUTPUT MACRO I, R, D, J
	DB 0x10, R, D
	ENDM

LJ_TEST_FINISH MACRO I, R, D, J
	DB 0xFF, R, D
	ENDM

; RAM Memory variables
lastPad	=	0xFF0802

; Test Programs

sampleDocProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x71,	/* DT1/MUL - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x34, 0x0D,	/* DT1/MUL - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x38, 0x33,	/* DT1/MUL - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x40, 0x23,	/* Total Level - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x44, 0x2D,	/* Total Level - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x48, 0x26,	/* Total Level - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x4C, 0x00,	/* Total Level - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x50, 0x5F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x99,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x5F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x94,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x05,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x05,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x05,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x07,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x02,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x02,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x02,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x02,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x11,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x11,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x11,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0xA6,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x00,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x32,	/* Feedback/algorithm */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x22,	/* Set frequency */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency */
		LJ_TEST_PART_0, 0x28, 0xF0,	/* Key on */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

noteProgram;
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x00,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x1F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x1F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x1F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x00,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x00,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x00,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x00,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x00,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x0F,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x00,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

noteDTProgram;
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x3F,	/* DT1/MUL - channel 0 slot 0 : DT=-3 MUL=2 -> *2 */
		LJ_TEST_PART_0, 0x34, 0x21,	/* DT1/MUL - channel 0 slot 2 : DT=-1 MUL=1 -> *1 */
		LJ_TEST_PART_0, 0x38, 0x33,	/* DT1/MUL - channel 0 slot 1 : DT=-2 MUL=3 -> *3 */
		LJ_TEST_PART_0, 0x3C, 0x03,	/* DT1/MUL - channel 0 slot 3 : DT=+0 MUL=3 -> *3 */
		LJ_TEST_PART_0, 0x40, 0x00,	/* Total Level - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x44, 0x70,	/* Total Level - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x48, 0x70,	/* Total Level - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.0) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x1F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x1F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x1F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x00,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x00,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x00,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x00,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x00,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x0F,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x00,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x18,	/* Set frequency (BLOCK=3) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x70,	/* Key on (slot 0+1+2 channel 0) */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

noteSLProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x57,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x15,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x15,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x00,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

algoProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x09,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=0 */
		LJ_TEST_PART_0, 0x34, 0x0A,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=0 */
		LJ_TEST_PART_0, 0x38, 0x0B,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=0 */
		LJ_TEST_PART_0, 0x3C, 0x08,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=0 */
		LJ_TEST_PART_0, 0x40, 0x10,	/* Total Level - channel 0 slot 0 (*0) */
		LJ_TEST_PART_0, 0x44, 0x10,	/* Total Level - channel 0 slot 2 (*0) */
		LJ_TEST_PART_0, 0x48, 0x10,	/* Total Level - channel 0 slot 1 (*1) */
		LJ_TEST_PART_0, 0x4C, 0x10,	/* Total Level - channel 0 slot 3 (*1) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x1F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x1F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x1F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x00,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x00,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x00,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x00,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x00,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x0F,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x00,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x1A,	/* Set frequency (BLOCK=7) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0xF0,	/* Key on (slot 0 & 1 & 2 & 3 channel 0) */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

dacProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x80,	/* DAC on */
		LJ_TEST_PART_1, 0x32, 0x33,	/* DT1/MUL - channel 5 slot 0 : DT=-3 MUL=2 -> *2 */
		LJ_TEST_PART_1, 0x36, 0x21,	/* DT1/MUL - channel 5 slot 2 : DT=-1 MUL=1 -> *1 */
		LJ_TEST_PART_1, 0x3A, 0x33,	/* DT1/MUL - channel 5 slot 1 : DT=-2 MUL=3 -> *3 */
		LJ_TEST_PART_1, 0x3E, 0x03,	/* DT1/MUL - channel 5 slot 3 : DT=+0 MUL=3 -> *3 */
		LJ_TEST_PART_1, 0x42, 0x00,	/* Total Level - channel 5 slot 0 */
		LJ_TEST_PART_1, 0x46, 0x70,	/* Total Level - channel 5 slot 2 */
		LJ_TEST_PART_1, 0x4A, 0x70,	/* Total Level - channel 5 slot 1 */
		LJ_TEST_PART_1, 0x4E, 0x7F,	/* Total Level - channel 5 slot 3 (*0.0) */
		LJ_TEST_PART_1, 0x52, 0x1F,	/* RS/AR - channel 5 slot 0 */
		LJ_TEST_PART_1, 0x56, 0x1F,	/* RS/AR - channel 5 slot 2 */
		LJ_TEST_PART_1, 0x5A, 0x1F,	/* RS/AR - channel 5 slot 1 */
		LJ_TEST_PART_1, 0x5E, 0x1F,	/* RS/AR - channel 5 slot 3 */
		LJ_TEST_PART_1, 0x62, 0x00,	/* AM/D1R - channel 5 slot 0 */
		LJ_TEST_PART_1, 0x66, 0x00,	/* AM/D1R - channel 5 slot 2 */
		LJ_TEST_PART_1, 0x6A, 0x00,	/* AM/D1R - channel 5 slot 1 */
		LJ_TEST_PART_1, 0x6E, 0x00,	/* AM/D1R - channel 5 slot 3 */
		LJ_TEST_PART_1, 0x72, 0x00,	/* D2R - channel 5 slot 0 */
		LJ_TEST_PART_1, 0x76, 0x00,	/* D2R - channel 5 slot 2 */
		LJ_TEST_PART_1, 0x7A, 0x00,	/* D2R - channel 5 slot 1 */
		LJ_TEST_PART_1, 0x7E, 0x00,	/* D2R - channel 5 slot 3 */
		LJ_TEST_PART_1, 0x82, 0x0F,	/* D1L/RR - channel 5 slot 0 */
		LJ_TEST_PART_1, 0x86, 0x0F,	/* D1L/RR - channel 5 slot 2 */
		LJ_TEST_PART_1, 0x8A, 0x0F,	/* D1L/RR - channel 5 slot 1 */
		LJ_TEST_PART_1, 0x8E, 0x0F,	/* D1L/RR - channel 5 slot 3 */
		LJ_TEST_PART_1, 0x92, 0x00,	/* SSG - channel 5 slot 0 */
		LJ_TEST_PART_1, 0x96, 0x00,	/* SSG - channel 5 slot 2 */
		LJ_TEST_PART_1, 0x9A, 0x00,	/* SSG - channel 5 slot 1 */
		LJ_TEST_PART_1, 0x9E, 0x00,	/* SSG - channel 5 slot 3 */
		LJ_TEST_PART_1, 0xB2, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) - channel 5 */
		LJ_TEST_PART_1, 0xB6, 0x40,	/* Right speaker on - channel 5 */
		LJ_TEST_PART_0, 0x28, 0x06,	/* Key off - channel 5 */
		LJ_TEST_PART_1, 0xA6, 0x38,	/* Set frequency (BLOCK=7) - channel 5 */
		LJ_TEST_PART_1, 0xA2, 0x69,	/* Set frequency FREQ=???) - channel 5 */
		LJ_TEST_PART_0, 0x28, 0x76,	/* Key on (slot 0+1+2 channel 5) */
		LJ_TEST_PART_0, 0x2A, 0x70,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x60,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x50,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x40,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x30,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x20,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x10,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x00,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x10,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x20,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x30,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x40,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x50,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x60,	/* DAC data */
		LJ_TEST_OUTPUT, 0x00, 0x02,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x2A, 0x70,	/* DAC data */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x06,	/* Key off - channel 5 */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

fbProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x08,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x1F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x1F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x1F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x00,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x00,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x00,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x00,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x00,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x0F,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x00,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x67,	/* Feedback/algorithm (FB=6 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x2A,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x50, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x50, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ch2Program:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 2 mode = 0 -> normal */
		LJ_TEST_PART_0, 0x27, 0x40,	/* Channel 2 mode = 1 -> special */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x32, 0x01,	/* DT1/MUL - channel 2 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x36, 0x01,	/* DT1/MUL - channel 2 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3A, 0x01,	/* DT1/MUL - channel 2 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3E, 0x01,	/* DT1/MUL - channel 2 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x42, 0x07,	/* Total Level - channel 2 slot 0 (*1) */
		LJ_TEST_PART_0, 0x46, 0x7F,	/* Total Level - channel 2 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x4A, 0x7F,	/* Total Level - channel 2 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4E, 0x07,	/* Total Level - channel 2 slot 3 (*1) */
		LJ_TEST_PART_0, 0x52, 0x1F,	/* RS/AR - channel 2 slot 0 */
		LJ_TEST_PART_0, 0x56, 0x1F,	/* RS/AR - channel 2 slot 2 */
		LJ_TEST_PART_0, 0x5A, 0x1F,	/* RS/AR - channel 2 slot 1 */
		LJ_TEST_PART_0, 0x5E, 0x1F,	/* RS/AR - channel 2 slot 3 */
		LJ_TEST_PART_0, 0x62, 0x00,	/* AM/D1R - channel 2 slot 0 */
		LJ_TEST_PART_0, 0x66, 0x00,	/* AM/D1R - channel 2 slot 2 */
		LJ_TEST_PART_0, 0x6A, 0x00,	/* AM/D1R - channel 2 slot 1 */
		LJ_TEST_PART_0, 0x6E, 0x00,	/* AM/D1R - channel 2 slot 3 */
		LJ_TEST_PART_0, 0x72, 0x00,	/* D2R - channel 2 slot 0 */
		LJ_TEST_PART_0, 0x76, 0x00,	/* D2R - channel 2 slot 2 */
		LJ_TEST_PART_0, 0x7A, 0x00,	/* D2R - channel 2 slot 1 */
		LJ_TEST_PART_0, 0x7E, 0x00,	/* D2R - channel 2 slot 3 */
		LJ_TEST_PART_0, 0x82, 0x0F,	/* D1L/RR - channel 2 slot 0 */
		LJ_TEST_PART_0, 0x86, 0x0F,	/* D1L/RR - channel 2 slot 2 */
		LJ_TEST_PART_0, 0x8A, 0x0F,	/* D1L/RR - channel 2 slot 1 */
		LJ_TEST_PART_0, 0x8E, 0x0F,	/* D1L/RR - channel 2 slot 3 */
		LJ_TEST_PART_0, 0x92, 0x00,	/* SSG - channel 2 slot 0 */
		LJ_TEST_PART_0, 0x96, 0x00,	/* SSG - channel 2 slot 2 */
		LJ_TEST_PART_0, 0x9A, 0x00,	/* SSG - channel 2 slot 1 */
		LJ_TEST_PART_0, 0x9E, 0x00,	/* SSG - channel 2 slot 3 */
		LJ_TEST_PART_0, 0xB2, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB6, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA6, 0x34,	/* Set frequency (BLOCK=6) - slot 3 */
		LJ_TEST_PART_0, 0xA2, 0x69,	/* Set frequency FREQ=???) - slot 3 */
		LJ_TEST_PART_0, 0xAD, 0x2F,	/* Set frequency (BLOCK=5) - slot 0 */
		LJ_TEST_PART_0, 0xA9, 0x69,	/* Set frequency FREQ=???) - slot 0 */
		LJ_TEST_PART_0, 0x28, 0x92,	/* Key on (slot0+slot 3 channel 2) */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

badRegProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x20, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x23, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x29, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x2C, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x2D, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x2E, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x2F, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x33, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x37, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x3B, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x3F, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x43, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x47, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x4B, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x4F, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x53, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x57, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x5B, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x5F, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x63, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x67, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x6B, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x6F, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x73, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x77, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x7B, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x7F, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x83, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x87, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x8B, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x8F, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x93, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x97, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x9B, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0x9F, 0x01,	/* invalid reg */
		LJ_TEST_PART_0, 0xA3, 0x69,	/* invalid reg */
		LJ_TEST_PART_0, 0xA7, 0x34,	/* invalid reg */
		LJ_TEST_PART_0, 0xAB, 0x69,	/* invalid reg */
		LJ_TEST_PART_0, 0xAF, 0x2F,	/* invalid reg */
		LJ_TEST_PART_0, 0xB3, 0x69,	/* invalid reg */
		LJ_TEST_PART_0, 0xB7, 0x69,	/* invalid reg */
		LJ_TEST_PART_0, 0xB8, 0x69,	/* invalid reg */
		LJ_TEST_PART_0, 0x28, 0x92,	/* Key on (slot0+slot 3 channel 2) */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

timerProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x24, 0xFF,	/* Timer A MSB */
		LJ_TEST_PART_0, 0x25, 0x0F,	/* Timer A LSB */
		LJ_TEST_PART_0, 0x26, 0x00,	/* Timer B */
		LJ_TEST_PART_0, 0x27, 0x8F,	/* Channel 2 mode CSM : Timer A & B enable : Timer A & B load */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x32, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x36, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3A, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3E, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x42, 0x08,	/* Total Level - channel 2 slot 0 (*1) */
		LJ_TEST_PART_0, 0x46, 0x7F,	/* Total Level - channel 2 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x4A, 0x7F,	/* Total Level - channel 2 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4E, 0x7F,	/* Total Level - channel 2 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x52, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x56, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x5A, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5E, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x62, 0x0A,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x66, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x6A, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6E, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x72, 0x10,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x76, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x7A, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7E, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x82, 0x12,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x86, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x8A, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8E, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x92, 0x00,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x96, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x9A, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9E, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB2, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB6, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0xA6, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA2, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0xAD, 0x2F,	/* Set frequency (BLOCK=5) - slot 0 */
		LJ_TEST_PART_0, 0xA9, 0x69,	/* Set frequency FREQ=???) - slot 0 */
		LJ_TEST_OUTPUT, 0xB0, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ssgOnProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x18,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x18,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x08,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x0F, 0x18,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ssgHoldProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x57,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x15,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x15,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x08+0x1,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x0F, 0x18,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ssgInvertProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x18,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x18,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x08+0x2,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x0F, 0x18,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ssgAttackProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x18,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x18,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x08+0x4,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x0F, 0x18,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ssgInvertHoldProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x18,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x10,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x08+0x2+0x1,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x0F, 0x18,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ssgAttackHoldProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x18,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x18,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x08+0x4+0x1,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x0F, 0x18,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ssgAttackInvertProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x18,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x18,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x08+0x4+0x2,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x0F, 0x18,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

ssgAttackInvertHoldProgram:
ALIGN=2
		LJ_TEST_PART_0, 0x22, 0x00,	/* LFO off */
		LJ_TEST_PART_0, 0x27, 0x00,	/* Channel 3 mode normal */
		LJ_TEST_PART_0, 0x28, 0x00,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x01,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x02,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x04,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x05,	/* All channels off */
		LJ_TEST_PART_0, 0x28, 0x06,	/* All channels off */
		LJ_TEST_PART_0, 0x2B, 0x00,	/* DAC off */
		LJ_TEST_PART_0, 0x30, 0x01,	/* DT1/MUL - channel 0 slot 0 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x34, 0x01,	/* DT1/MUL - channel 0 slot 2 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x38, 0x01,	/* DT1/MUL - channel 0 slot 1 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x3C, 0x01,	/* DT1/MUL - channel 0 slot 3 : DT=0 MUL=1 */
		LJ_TEST_PART_0, 0x40, 0x02,	/* Total Level - channel 0 slot 0 (*1) */
		LJ_TEST_PART_0, 0x44, 0x7F,	/* Total Level - channel 0 slot 2 (*0.000001f) */
		LJ_TEST_PART_0, 0x48, 0x7F,	/* Total Level - channel 0 slot 1 (*0.000001f) */
		LJ_TEST_PART_0, 0x4C, 0x7F,	/* Total Level - channel 0 slot 3 (*0.000001f) */
		LJ_TEST_PART_0, 0x50, 0x1F,	/* RS/AR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x54, 0x0F,	/* RS/AR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x58, 0x0F,	/* RS/AR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x5C, 0x0F,	/* RS/AR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x60, 0x18,	/* AM/D1R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x64, 0x1F,	/* AM/D1R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x68, 0x1F,	/* AM/D1R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x6C, 0x0F,	/* AM/D1R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x70, 0x18,	/* D2R - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x74, 0x00,	/* D2R - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x78, 0x00,	/* D2R - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x7C, 0x00,	/* D2R - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x80, 0x15,	/* D1L/RR - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x84, 0x0F,	/* D1L/RR - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x88, 0x0F,	/* D1L/RR - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x8C, 0x0F,	/* D1L/RR - channel 0 slot 3 */
		LJ_TEST_PART_0, 0x90, 0x08+0x4+0x2+0x1,	/* SSG - channel 0 slot 0 */
		LJ_TEST_PART_0, 0x94, 0x00,	/* SSG - channel 0 slot 2 */
		LJ_TEST_PART_0, 0x98, 0x00,	/* SSG - channel 0 slot 1 */
		LJ_TEST_PART_0, 0x9C, 0x00,	/* SSG - channel 0 slot 3 */
		LJ_TEST_PART_0, 0xB0, 0x07,	/* Feedback/algorithm (FB=0 ALG=7) */
		LJ_TEST_PART_0, 0xB4, 0xC0,	/* Both speakers on */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_PART_0, 0xA4, 0x34,	/* Set frequency (BLOCK=6) */
		LJ_TEST_PART_0, 0xA0, 0x69,	/* Set frequency FREQ=???) */
		LJ_TEST_PART_0, 0x28, 0x10,	/* Key on (slot 0 channel 0) */
		LJ_TEST_OUTPUT, 0x0F, 0x18,	/* OUTPUT SAMPLES */
		LJ_TEST_PART_0, 0x28, 0x00,	/* Key off */
		LJ_TEST_OUTPUT, 0x30, 0x00,	/* OUTPUT SAMPLES */
		LJ_TEST_FINISH, 0xFF, 0xFF,	/* END PROGRAM */

