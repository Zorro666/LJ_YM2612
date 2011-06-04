	CPU	68000

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

	MACRO GUI_UPDATE PAD_VALUE
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

	MACRO LJ_TEST_PART_0 I, R, D, J
		DB 0x00, R, D
	ENDM

	MACRO LJ_TEST_PART_1 I, R, D, J
		DB 0x01, R, D
	ENDM

	MACRO LJ_TEST_OUTPUT I, R, D, J
		DB 0x10, R, D
	ENDM

	MACRO LJ_TEST_FINISH I, R, D, J
		DB 0xFF, R, D
	ENDM

; RAM Memory variables
lastPad	=	0xFF0802

; Test Programs

sampleDocProgram:
	ALIGN 2
noteProgram;
	ALIGN 2
	INCLUDE "../sampleDoc.prog"

noteDTProgram;
	ALIGN 2
	INCLUDE "../note.prog"

noteSLProgram:
	ALIGN 2
	INCLUDE "../noteDT.prog"

algoProgram:
	ALIGN 2
	INCLUDE "../algo.prog"

dacProgram:
	ALIGN 2
	INCLUDE "../dac.prog"

fbProgram:
	ALIGN 2
	INCLUDE "../fb.prog"

ch2Program:
	ALIGN 2
	INCLUDE "../ch2.prog"

badRegProgram:
	ALIGN 2
	INCLUDE "../badReg.prog"

timerProgram:
	ALIGN 2
	INCLUDE "../timer.prog"

ssgOnProgram:
	ALIGN 2
	INCLUDE "../ssgOn.prog"

ssgHoldProgram:
	ALIGN 2
	INCLUDE "../ssgHold.prog"

ssgInvertProgram:
	ALIGN 2
	INCLUDE "../ssgInvert.prog"

ssgAttackProgram:
	ALIGN 2
	INCLUDE "../ssgAttack.prog"

ssgInvertHoldProgram:
	ALIGN 2
	INCLUDE "../ssgInvertHold.prog"

ssgAttackHoldProgram:
	ALIGN 2
	INCLUDE "../ssgAttackHold.prog"

ssgAttackInvertProgram:
	ALIGN 2
	INCLUDE "../ssgAttackInvert.prog"

ssgAttackInvertHoldProgram:
	ALIGN 2
	INCLUDE "../ssgAttackInvertHold.prog"

