all: jakeTest.bin

jakeTest.bin: jakeTest.asm font.asm $(wildcard ../*.prog)

%.bin:%.asm
	asm2bin $*
