
;
;
; knocked up in 30 minutes or so, its a bit crap but suffices
;
;  Hex number printer relies on order of these tiles, normal text printer uses ASCII 


	DC.b	0x00,0x00,0x00,0x00			; Blank tile
	DC.b	0x00,0x00,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x0F,0xF0
	DC.b	0x0F,0x00,0xF0,0xF0
	DC.b	0x0F,0x0F,0x00,0xF0
	DC.b	0x0F,0xF0,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 1
	DC.b	0x00,0x0F,0xFF,0x00
	DC.b	0x00,0xF0,0x0F,0x00
	DC.b	0x00,0x00,0x0F,0x00
	DC.b	0x00,0x00,0x0F,0x00
	DC.b	0x00,0x00,0x0F,0x00
	DC.b	0x00,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 2
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0xFF,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0xF0,0x00,0x00
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 3
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x0F,0xFF,0x00
	DC.b	0x00,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 4
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x0F,0x00
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0x0F,0x00
	DC.b	0x00,0x00,0x0F,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 5
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 6
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 7
	DC.b	0x00,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x0F,0x00
	DC.b	0x00,0x00,0xF0,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0xF0,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 8
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; 9
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; A
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; B
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; C
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; D
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; E
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0xFF,0xF0,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; F
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0xFF,0xF0,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; G
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0xFF,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; H
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; I
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; J
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0xF0,0x00
	DC.b	0x00,0x00,0xF0,0x00
	DC.b	0x00,0x00,0xF0,0x00
	DC.b	0x00,0x00,0xF0,0x00
	DC.b	0x0F,0xFF,0xF0,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; K
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0xFF,0x00
	DC.b	0x0F,0xFF,0x00,0x00
	DC.b	0x0F,0x00,0xF0,0x00
	DC.b	0x0F,0x00,0x0F,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; L
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; M
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xF0,0x0F,0xF0
	DC.b	0x0F,0x0F,0xF0,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; N
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xF0,0x00,0xF0
	DC.b	0x0F,0x0F,0x00,0xF0
	DC.b	0x0F,0x00,0xF0,0xF0
	DC.b	0x0F,0x00,0x0F,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; O
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; P
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x0F,0x00,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; Q
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x0F,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; R
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x0F,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; S
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0x00,0x00
	DC.b	0x00,0x00,0xFF,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; T
	DC.b	0x0F,0xFF,0xFF,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; U
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xFF,0xFF,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; V
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xF0,0x0F,0x00
	DC.b	0x00,0xF0,0x0F,0x00
	DC.b	0x00,0xF0,0x0F,0x00
	DC.b	0x00,0x0F,0xF0,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; W
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x0F,0x0F,0xF0,0xF0
	DC.b	0x0F,0xF0,0x0F,0xF0
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; X
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0xF0,0x0F,0x00
	DC.b	0x00,0x0F,0xF0,0x00
	DC.b	0x00,0xF0,0x0F,0x00
	DC.b	0x00,0xF0,0x0F,0x00
	DC.b	0x0F,0x00,0x00,0xF0
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; Y
	DC.b	0x0F,0x00,0x0F,0x00
	DC.b	0x00,0xF0,0xF0,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0x00,0x00,0x00

	DC.b	0x00,0x00,0x00,0x00			; Z
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0x0F,0x00
	DC.b	0x00,0x00,0xF0,0x00
	DC.b	0x00,0x0F,0x00,0x00
	DC.b	0x00,0xF0,0x00,0x00
	DC.b	0x0F,0xFF,0xFF,0xF0
	DC.b	0x00,0x00,0x00,0x00

