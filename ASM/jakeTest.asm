CPU=68000

	dc.l $00000000,	EntryPoint,	ErrorTrap,	ErrorTrap	;00
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;10
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;20
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;30
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;40
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;50
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;60
	dc.l HInt,	ErrorTrap,	VInt,		ErrorTrap	;70
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;80
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;90
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;A0
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;B0
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;C0
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;D0
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;E0
	dc.l ErrorTrap,	ErrorTrap,	ErrorTrap,	ErrorTrap	;F0

; D0 = spare temporary
; D1 = spare temporary
; D2 = spare temporary
; D3 = used as loop counter inside output samples
; D4 = used inside UpdateGUI
; D5 = used inside UpdateGUI for joy pad reading
; D6 = used inside UpdateGUI for joy pad reading
; D7 = return value from UpdateGUI

check_sum equ	$0000
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
	dc.l	$00000000,$0007ffff,$00ff0000,$00ffffff		;1A0
	dc.b	'                '	;1B0
	dc.b	'                '	;1C0
	dc.b	'                '	;1D0
	dc.b	'                '	;1E0
	dc.b	'JUE             '	;1F0


ErrorTrap:
	jmp	ErrorTrap

HInt:
VInt:
	rte

EntryPoint:
	move.w #$FFFF,D0
Loop1:
	dbf D0,Loop1

Loop2:
	move.b #1,$A11100	;BUSREQ
	move.b #1,$A11200	;RESET
	btst.b #0,$A11100
	bne	Loop2

	move.b #$40,$A10009 ; init joypad 1 by writing 0x40 to CTRL port for joypad 1

	lea ssgAttackProgram, A1 ; start of the test program to run
	lea sampleDocProgram, A1 ; start of the test program to run
StartProgram:
	move A1, A0

RunProgram:
	move.b (A0)+, D0	; D0 is the operation to perform

	cmpi.b #0x00, D0
	beq WritePort0

	cmpi.b #0x01, D0
	beq WritePort1

	cmpi.b #0x10, D0
	beq OutputSamples

	cmpi.b #0xFF, D0
	beq EndProgram
	
	jmp	ErrorTrap			; unknown operation to perform

WritePort0:
	move.b (A0)+, D0	; register
	move.b (A0)+, D1	; data
	bsr WriteYm2612
	jmp RunProgram
	
WritePort1:				; NOT IMPLEMENTED PROPERLY
	move.b (A0)+, D0	; register
	move.b (A0)+, D1	; data
	bsr WriteYm2612
	jmp RunProgram

OutputSamples:			; loop a lot
	move.b (A0)+, D0	; bottom 8-bits
	move.b (A0)+, D1	; top 8-bits
OutputOneSample:
	move.w #0x4FFF, D3 ; this should be the number of cycles to output a single 44KHz sample
OutputSample:
	bsr UpdateGUI
	cmpi.b #0x01, D7
	beq StartProgram

	subi.w #0x01, D3
	bne OutputSample
	subi.w #0x01, D0
	bne OutputOneSample
	jmp RunProgram

EndProgram:
	bsr UpdateGUI
	cmpi.b #0x01, D7
	beq StartProgram

	jmp	EndProgram

WriteYM2612:
	btst #7,$A04000
	bne WriteYM2612
	move.b D0,$A04000
WriteYM2612Data:
	btst #7,$A04000
	bne WriteYM2612Data
	move.b D1,$A04001
	rts

UpdateGUI:				; test for joypad input & draw screen text
	move.b #$00, D7	; D7 is result of what to do from this function

	move.b #$40, $A10003 ; ask for high-state on joypad 1 by writing 0x40 to DATA port for joypad 1
	nop							; wait a couple of cycles
	nop							; wait a couple of cycles
	move.b $A10003, D5	; read joypad 1 : C-button, B-button + Right/Left/Down/Up

	move.b #$00, $A10003 ; ask for low-state on joypad 1 by writing 0x00 to DATA port for joypad 1
	nop							; wait a couple of cycles
	nop							; wait a couple of cycles
	move.b $A10003, D6	; read joypad 1 : Start-button + A-Button + Down/Up

	; TODO-merge D6 & D5 into a single joypad 1 8-bit value: (D6 & 0x30 << 2) | D5

	; debounce : (lastState ^ newState ) & newState -> true when button has been released
	lea lastPadHigh, A2
	move.b (A2), D4		; D4 is lastState, D6 is newState (1 = not pressed, 0 = pressed)
	eor.b D6, D4		; D4 = lastState ^ newState
	and.b D6, D4		; D4 = (lastState ^ newState) & newState

	move.b D4, D2		; test for Start which is bit-5 -> 0x20
	andi.b #$20, D2
	beq NotStart		; test for start button not-pressed (0) -> pressed (1)

	move.b #$01, D7	; start was pressed

NotStart:

SavePadState:
	lea lastPadLow, A2
	move.b D5, (A2)
	lea lastPadHigh, A2
	move.b D6, (A2)
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

; Test Programs

ssgAttackProgram:
ALIGN=2

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

; RAM Memory variables
lastPadLow=0xFF0800
lastPadHigh=0xFF0802
