	CPU	68000
	LIST MACRO

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
	dc.b	':Suite of Tests '	;130
	dc.b	'                '	;140
	dc.b	'JAKE YM2612 TEST'	;150 US title
	dc.b	':Suite of Tests '	;160
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

T_RESET	= 0x01
T_PLAY	= 0x02
T_NEXT	= 0x03
T_PREV	= 0x04

PROGS_START	= 0x1FFF
PROGS_END 	= 0x2FFF

	MACRO GUI_UPDATE RETURN_VALUE
		bsr 		UpdateGUI
		cmpi.b	#T_RESET,		RETURN_VALUE	
		beq 		ResetPrograms
		cmpi.b	#T_PLAY,		RETURN_VALUE	
		beq 		RestartProgram
		cmpi.b	#T_NEXT,		RETURN_VALUE	
		beq 		NextProgram
		cmpi.b	#T_PREV,		RETURN_VALUE	
		beq 		PrevProgram
	ENDM

ErrorTrap:
	jmp			ErrorTrap

HInt:
VInt:
	rte

EntryPoint:

	jsr	initGFX

	jsr	copyTileData

	jsr	setPalette
	
	jsr	ClearDisplay

	move.w	#0xFFFF,D0
Loop1:
	dbf 		D0,				Loop1

Loop2:
	move.b	#0x01,		0xA11100	;BUSREQ
	move.b	#0x01,		0xA11200	;RESET
	btst.b	#0x00,		0xA11100
	bne			Loop2

	move.b	#0x40, 		0xA10009	; init joypad 1 by writing 0x40 to CTRL port for joypad 1 
	
ResetPrograms:
	lea			progListStart,	A0	; A0 = memory location of the start of the prog list
	addi.w	#0x02,		A0				; +2 to get past the sentinel value

	move.w	A0,				curProg		; *curProg = progList, set curProg to the start of progList

StartProgram:
	move.w	curProg,	A1				; A1 = *curProg, curProg contains address of entry in proglist of program to play
	move.w	(A1),			A0				; A0 = *A1 = **curProg, A0 is address of start of current program

RunProgram:
	move.b	(A0)+,		D0				; D0 is the operation to perform

	cmpi.b	#0x00,		D0
	beq 		WritePart0

	cmpi.b	#0x01,		D0
	beq 		WritePart1

	cmpi.b	#0x10,		D0
	beq 		OutputSamples

	cmpi.b	#0xFF,		D0
	beq 		EndProgram
	
	jmp			ErrorTrap						; unknown operation to perform

WritePart0:
	move.b	(A0)+,		D0				; register
	move.b	(A0)+,		D1				; data
	bsr 		WriteYm2612Part0
	jmp 		RunProgram
	
WritePart1:										; NEVER TESTED
	move.b	(A0)+,		D0				; register
	move.b	(A0)+,		D1				; data
	bsr 		WriteYm2612Part1
	jmp			RunProgram

OutputSamples:								; loop a lot
	move.b	(A0)+,		D0				; bottom 8-bits
	move.b	(A0)+,		D1				; top 8-bits
	andi.w	#0xFF,		D0				; clear out upper 8-bits
	lsl.w		#0x8,			D1				; D1 = D1 << 8
	or.w		D1,				D0				; D0 = (D0 & 0xFF) | (D1 << 8)
OutputOneSample:
	move.w	#0x0001,	D3				; this should be the number of cycles to output a single 44KHz sample
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

WriteYM2612Part0:
	btst		#0x07,		0xA04000
	bne 		WriteYM2612Part0
	move.b	D0,				0xA04000
WriteYM2612DataPart0:
	btst		#0x07,		0xA04000
	bne			WriteYM2612DataPart0
	move.b	D1,0xA04001
	rts

WriteYM2612Part1:
	btst		#0x07,		0xA04000
	bne 		WriteYM2612Part1
	move.b	D0,				0xA04002
WriteYM2612DataPart1:
	btst		#0x07,		0xA04000
	bne			WriteYM2612DataPart1
	move.b	D1,0xA04003
	rts

UpdateGUI:										; test for joypad input & draw screen text

;;;;;;;;;;;;;;;;;;;;;;;;;;

	movem.l	d0-d2/A6,	-(A7)

; Clear screen - not work needs to be on VBLANK
;	jsr	ClearDisplay

; Get the test program display string
	move.w	curProg,	D2				; D2 = *curProg
	lea			progListStart, A3		; A3 = start of prog list
	sub.w		A3,				D2				; D2 = *curProg - start of prog list

	lea			progListNames,	A3	; A3 = start of string array
	add.w		D2,				A3				; A3 += offset of the curProg number in bytes
	move.w	(A3),			A6				; A6 = *A3 = the address of the current program display string
	move.b	#0x0A,		D0				; x co-ord
	move.b	#0x05,		D1				; y co-ord
	jsr			printAt	
	
	lea			progListDescriptions,	A3	; A3 = start of string array
	add.w		D2,				A3				; A3 += offset of the curProg number in bytes
	move.w	(A3),			A6				; A6 = *A3 = the address of the current program display string
	move.b	#0x00,		D0				; x co-ord
	move.b	#0x07,		D1				; y co-ord
	jsr			printAt	
	
	lea			testProgramIndexString,	A6		; A6 = ptr to the string
	move.b	#0x01,		D0				; x co-ord
	move.b	#0x0A,		D1				; y co-ord
	jsr			printAt	

;	Work out the test program index
	move.w	curProg,	D2				; D2 = *curProg
	lea			progListStart, A3		; A3 = start of prog list
	sub.w		A3,				D2				; D2 = *curProg - start of prog list
	subi.w	#0x02,		D2				; D2 -= 2
	lsr.w		#0x01,		D2				; D2 /= 2

	move.b	#0x16,		D0				; x co-ord
	move.b	#0x0A,		D1				; y co-ord
	jsr			printHex						; hexadecimal number in D2 (always assumes long) always prints as 00000000

	lea			helpString1,	A6			; A6 = ptr to the string
	move.b	#0x01,		D0				; x co-ord
	move.b	#0x0F,		D1				; y co-ord
	jsr			printAt	

	lea			helpString2,	A6			; A6 = ptr to the string
	move.b	#0x01,		D0				; x co-ord
	move.b	#0x10,		D1				; y co-ord
	jsr			printAt	

	lea			helpString3,	A6			; A6 = ptr to the string
	move.b	#0x01,		D0				; x co-ord
	move.b	#0x11,		D1				; y co-ord
	jsr			printAt	

	lea			helpString4,	A6			; A6 = ptr to the string
	move.b	#0x01,		D0				; x co-ord
	move.b	#0x12,		D1				; y co-ord
	jsr			printAt	

	movem.l	(A7)+,		d0-d2/A6

;;;;;;;;;;;;;;;;;;;;;;;;;;

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

	move.b 	lastPad,	D4				; D4 is lastState, D6 is newState (1 = not pressed, 0 = pressed)
	eor.b 	D6, 			D4				; D4 = lastState ^ newState
	and.b 	D6, 			D4				; D4 = (lastState ^ newState) & newState


; Helpful macro for testing button results and setting the result parameter
	MACRO TEST_BUTTON BUTTON, RESULT
	move.b 	D4, 			D2				; test for BUTTON button
	andi.b 	#BUTTON,	D2
	beq 		Not ## BUTTON ## \?	; test for BUTTON button not-pressed (0) -> pressed (1)

	move.b 	#RESULT,	D7				; BUTTON button was pressed
	jmp 		SavePadState

Not ## BUTTON ## \?:
	ENDM

	TEST_BUTTON P_START, T_RESET
	TEST_BUTTON P_A, T_PLAY
	TEST_BUTTON P_LEFT, T_PREV
	TEST_BUTTON P_RIGHT, T_NEXT

SavePadState:
	move.b 	D6, 			lastPad
	rts
	
RestartProgram:
	jmp			StartProgram

NextProgram:
; TODO - test for end of programs
	move.w	curProg,	A2			; A2 = *curProg
	addi.w	#$02,			A2			; go to the next program
	move.w	(A2),			D0			; test the program
	cmpi.w	#PROGS_END, D0		; have we reached the end
	bne			NEXT_SAVE
	lea			progListStart,	A2	; reset to the start of the list
	addi.w	#0x02,		A2			; +2 to get past the sentinel value
NEXT_SAVE:
	move.w	A2,				curProg	; *curProg+=2
	jmp			StartProgram

PrevProgram:
; TODO - test for start of programs
	move.w	curProg,	A2			; A2 = *curProg
	subi.w	#$02,			A2			; go to the next program
	move.w	(A2),			D0			; test the program
	cmpi.w	#PROGS_START, D0	; have we reached the start
	bne			PREV_SAVE
	lea			progListEnd,	A2	; reset to the end of the list
	subi.w	#0x02,		A2			; -2 to get past the sentinel value
PREV_SAVE:
	move.w	A2,				curProg	; *curProg-=2
	jmp			StartProgram

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

GFXDATA = 0xC00000			; VDP Data Port
GFXCNTL	= 0xC00004			; VDP Control Port

	MACRO	VDP_SET_REGISTER	reg,data,dest			; reg is vdp register, data is byte to set, dest is 68000 reg pointing at GFXCNTL
		move.w #0x8000 + ((reg&0x1F)<<8) + (data&0xFF),(dest)
	ENDM

	MACRO	VDP_SET_VRAM_DEST	address,dest			; address is address in vram to set vram pointer, dest is 68000 reg pointing at GFXCNTL
		move.l #0x40000000 + ((address&0x3FFF)<<16) + (address>>14),(dest)
	ENDM

	MACRO	VDP_SET_CRAM_DEST	address,dest			; address is address in vram to set vram pointer, dest is 68000 reg pointing at GFXCNTL
		move.l #0xC0000000 + ((address&0x3FFF)<<16) + (address>>14),(dest)
	ENDM

	ALIGN 2
initGFX:
	movem.l D0-D7/A0-A6,-(A7)

	lea	GFXCNTL,A0

	VDP_SET_REGISTER	0,0x04,A0			; Set mode register 1  - full palette - everything else disabled
	VDP_SET_REGISTER	1,0x74,A0			; Set mode register 2 - Display Enable + Vertical interrupt + dma enable + genesis vdp mode
	VDP_SET_REGISTER	2,0x38,A0			; Plane A screen at E000
	VDP_SET_REGISTER	3,0x38,A0			; Pattern Table at E000
	VDP_SET_REGISTER	4,0x07,A0			; Plane B screen at E000
	VDP_SET_REGISTER	5,0x7E,A0			; Sprite location at E000
	VDP_SET_REGISTER	7,0x00,A0			; Use palette entry 0 for backdrop colour
	VDP_SET_REGISTER	10,0xFF,A0			; Line intterupt counter - not line interrupts are not enabled, so this won't matter
	VDP_SET_REGISTER	11,0x00,A0			; Set mode register 3 - normal scrolling
	VDP_SET_REGISTER	12,0x00,A0			; Set mode register 4 - no interlace, shadow highlight etc
	VDP_SET_REGISTER	13,0x00,A0			; H scroll table address
	VDP_SET_REGISTER	15,0x02,A0			; Auto increment by 2 on write to vdp memory
	VDP_SET_REGISTER	16,0x00,A0			; 32x32 for both plane A and B
	VDP_SET_REGISTER	17,0x00,A0			; Window H
	VDP_SET_REGISTER	18,0xFF,A0			; Window V

	movem.l	(A7)+,D0-D7/A0-A6
	rts

copyTileData:
	
	movem.l	D0-D7/A0-A6,-(A7)

	lea	GFXCNTL,A0
	lea	GFXDATA,A1

	VDP_SET_VRAM_DEST	0,A0				; sets the vram pointer (used by GFXDATA port) to VRAM address 0

	lea	tilesStart,A2

	move.w	#tilesEnd - tilesStart - 1,D0
	lsr.w	#2,D0						; moving long words at a time
.loop
	move.l	(A2)+,(A1)
	dbf	D0,.loop

	movem.l (A7)+,D0-D7/A0-A6
	rts

setPalette:
	
	movem.l	D0-D7/A0-A6,-(A7)

	lea	GFXCNTL,A0
	lea	GFXDATA,A1

	VDP_SET_CRAM_DEST	0,A0				; sets the vram pointer (used by GFXDATA port) to CRAM address 0

	move.w	#15,D0						; 64 entries in palette table	- 4 * 16
	move.w	#0x0000,D1
	move.w	#0x0000,D2
.loop
	move.w	D1,(A1)
	add.w	#0x0111,D2
	move.w	D2,D1
	and.w	#0x0EEE,D1					; ---- bbb- ggg- rrr-		for now just grey scale
	dbf	D0,.loop

	movem.l (A7)+,D0-D7/A0-A6
	rts

ClearDisplay:							;Writes 0 into 32x32 tilemap
	
	movem.l	D0-D7/A0-A6,-(A7)

	lea	GFXCNTL,A0
	lea	GFXDATA,A1

	VDP_SET_VRAM_DEST	0xE000,A0			; sets the vram pointer (used by GFXDATA port) to CRAM address 0xE000

	move.w	#32*32-1,D0					; 32x32 word entries in screen map
	move.l	#0x80008000,D1					; pPPv httt tttt tttt	(p)riority 
								;			(P)alette block num
								;			(v) flip
								;			(h) flip
								;			(t)ile number
	
	lsr.w	#1,D0						; write 2 entries at a time
.loop
	move.l	D1,(A1)
	dbf	D0,.loop

	movem.l (A7)+,D0-D7/A0-A6
	rts

PrintCharAt:
	movem.l	D0-D7/A0-A6,-(A7)

	lea	asciiOffsetTable,A2

	lsl.l	#1,D2
	add.l	D2,A2
	move.w	(A2),D2						; tile number now in D2
	add.w	#0x8000,D2					; priority bit set - prob doesn't matter

	jsr	PrintTileAt

	movem.l (A7)+,D0-D7/A0-A6
	rts

PrintTileAt:				; tile in D2
	
	movem.l	D0-D7/A0-A6,-(A7)

	lea	GFXCNTL,A0
	lea	GFXDATA,A1

	and.l	#0x0000001F,D0					; 32 x 32 range
	and.l	#0x0000001F,D1
	

	move.l	#0x0000E000,D3					; screen map address

	lsl.l	#1,D0
	lsl.l	#6,D1
	add.l	D0,D3
	add.l	D1,D3

	move.l	#0x40000000,D4					; write to vram code
	
	move.l	D3,D5
	and.w	#0x3FFF,D5
	swap	D5
	lsr.l	#8,D3
	lsr.l	#6,D3
	add.l	D3,D4
	add.l	D5,D4

	move.l	D4,(A0)						; sets the vram pointer (used by GFXDATA port) to CRAM address 0xE000 + X,Y position

	move.w	D2,(A1)

	movem.l (A7)+,D0-D7/A0-A6
	rts

PrintAt:
	
	movem.l	D0-D7/A0-A6,-(A7)

.loop	move.b	(A6)+,D2
	and.b	#0xFF,D2
	beq	.exit					; check for 0 terminator

	jsr	PrintCharAt
	add.b	#1,D0
	and.b	#$1F,D0
	bne	.loop					; check for x co-ord off edge

	add.b	#1,D1

	bra	.loop

.exit
	movem.l (A7)+,D0-D7/A0-A6
	rts


PrintHex:
	
	movem.l	D0-D7/A0-A6,-(A7)

	move.l	D2,D3
	move.b	#8,D7
.loop
	dbf	D7,.rest
.exit
	movem.l (A7)+,D0-D7/A0-A6
	rts
.rest
	rol.l	#4,D3
	move.b	D3,D2
	and.w	#0x000F,D2
	add.w	#1,D2

	jsr	PrintTileAt

	add.b	#1,D0
	and.b	#0x1F,D0
	bne	.loop

	add.b	#1,D1
	bra	.loop


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

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

; Test Programs

	MACRO TEST_PROGRAM test, description
test ## ProgramString: 
		DC.B 'test'
		DS 15, ' '								; add lots of spaces to clear the display and remove any previous string display
		DC.B 0
test ## DescriptionString: 
		DC.B description
		DS 20, ' '								; add lots of spaces to clear the display and remove any previous string display
		DC.B 0
		ALIGN 2
test ## Program:
		INCLUDE "../ ## test ## .prog"
	ENDM

	TEST_PROGRAM sampleDoc, 					'sample from the Genesis manual'
	TEST_PROGRAM note,								'most basic pure tone'
	TEST_PROGRAM noteDT,							'pure tone with detune'
	TEST_PROGRAM noteSL,							'pure tone with sustain level'
	TEST_PROGRAM algo,								'algorithm test'
	TEST_PROGRAM dac,									'dac output'
	TEST_PROGRAM fb,									'feedback'
	TEST_PROGRAM ch2,									'channel 2 special mode'
	TEST_PROGRAM badReg,							'set lots of invalid registers'
	TEST_PROGRAM timer,								'timer A and B'
	TEST_PROGRAM ssgOn,								'SSG enabled'
	TEST_PROGRAM ssgHold,							'SSG hold bit'
	TEST_PROGRAM ssgInvert,						'SSG invert bit'
	TEST_PROGRAM ssgAttack,						'SSG attack bit'
	TEST_PROGRAM ssgInvertHold,				'SSG invert and hold bits'
	TEST_PROGRAM ssgAttackHold,				'SSG attack and hold bits'
	TEST_PROGRAM ssgAttackInvert,			'SSG attack and invert bits'
	TEST_PROGRAM ssgAttackInvertHold,	'SSG attack invert and hold bits'
	TEST_PROGRAM lfoAMS,							'LFO AMS test 9.63Hz 5.9dB'
	TEST_PROGRAM lfoPMS,							'LFO PMS test 9.63Hz'
	TEST_PROGRAM part1,								'part1 channel 3'
	TEST_PROGRAM dacch5,							'part1 dac enable stops channel 5'

	ALIGN 2
tilesStart:
	INCLUDE "font.asm"
tilesEnd:

asciiOffsetTable:		; 128 entries - should be plenty I've only filled 0-9,A-Z  and a-z with A-Z
	DC.W	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000		;   0-15
	DC.W	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000		;  16-31
	DC.W	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000		;  32-47
	DC.W	0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x000A,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000		;  48-63
	DC.W	0x0000,0x000B,0x000C,0x000D,0x000E,0x000F,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019		;  64-79
	DC.W	0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,0x0020,0x0021,0x0022,0x0023,0x0024,0x0000,0x0000,0x0000,0x0000,0x0000		;  80-95
	DC.W	0x0000,0x000B,0x000C,0x000D,0x000E,0x000F,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019		;  96-111
	DC.W	0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,0x0020,0x0021,0x0022,0x0023,0x0024,0x0000,0x0000,0x0000,0x0000,0x0000		; 111-128

helpString1	DC.B	'A              replay test', 0
helpString2	DC.B	'START          reset tests', 0
helpString3	DC.B	'LEFT         previous test',0
helpString4	DC.B	'RIGHT            next test',0

testProgramIndexString	DC.B	'Test Program Index',0

	ALIGN 2
progListStart:
	DC.w			PROGS_START
	DC.w			dacch5Program
	DC.w			sampleDocProgram
	DC.w			noteProgram
	DC.w			noteDTProgram
	DC.w			noteSLProgram
	DC.w			algoProgram
	DC.w			dacProgram
	DC.w			fbProgram
	DC.w			ch2Program
	DC.w			badRegProgram
	DC.w			timerProgram
	DC.w			ssgOnProgram
	DC.w			ssgHoldProgram
	DC.w			ssgInvertProgram
	DC.w			ssgAttackProgram
	DC.w			ssgInvertHoldProgram
	DC.w			ssgAttackHoldProgram
	DC.w			ssgAttackInvertProgram
	DC.w			ssgAttackInvertHoldProgram
	DC.w			lfoAMSProgram
	DC.w			lfoPMSProgram
	DC.w			part1Program
	DC.w			dacch5Program
progListEnd:
	DC.w			PROGS_END

	ALIGN 2
progListNames:
	DC.w			PROGS_START
	DC.w			dacch5ProgramString
	DC.w			sampleDocProgramString
	DC.w			noteProgramString
	DC.w			noteDTProgramString
	DC.w			noteSLProgramString
	DC.w			algoProgramString
	DC.w			dacProgramString
	DC.w			fbProgramString
	DC.w			ch2ProgramString
	DC.w			badRegProgramString
	DC.w			timerProgramString
	DC.w			ssgOnProgramString
	DC.w			ssgHoldProgramString
	DC.w			ssgInvertProgramString
	DC.w			ssgAttackProgramString
	DC.w			ssgInvertHoldProgramString
	DC.w			ssgAttackHoldProgramString
	DC.w			ssgAttackInvertProgramString
	DC.w			ssgAttackInvertHoldProgramString
	DC.w			lfoAMSProgramString
	DC.w			lfoPMSProgramString
	DC.w			part1ProgramString
	DC.w			dacch5ProgramString
	DC.w			PROGS_END

	ALIGN 2
progListDescriptions:
	DC.w			PROGS_START
	DC.w			dacch5DescriptionString
	DC.w			sampleDocDescriptionString
	DC.w			noteDescriptionString
	DC.w			noteDTDescriptionString
	DC.w			noteSLDescriptionString
	DC.w			algoDescriptionString
	DC.w			dacDescriptionString
	DC.w			fbDescriptionString
	DC.w			ch2DescriptionString
	DC.w			badRegDescriptionString
	DC.w			timerDescriptionString
	DC.w			ssgOnDescriptionString
	DC.w			ssgHoldDescriptionString
	DC.w			ssgInvertDescriptionString
	DC.w			ssgAttackDescriptionString
	DC.w			ssgInvertHoldDescriptionString
	DC.w			ssgAttackHoldDescriptionString
	DC.w			ssgAttackInvertDescriptionString
	DC.w			ssgAttackInvertHoldDescriptionString
	DC.w			lfoAMSDescriptionString
	DC.w			lfoPMSDescriptionString
	DC.w			part1DescriptionString
	DC.w			dacch5DescriptionString
	DC.w			PROGS_END

	MACRO GLOBAL_VARIABLE name, numBytes
	ALIGN numBytes
name:	DS numBytes
	ENDM

; RAM Memory variables
	ORG 0xFF0000
	GLOBAL_VARIABLE lastPad, 2
	GLOBAL_VARIABLE curProg, 2


