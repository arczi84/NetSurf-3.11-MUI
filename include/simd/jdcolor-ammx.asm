; jdcolor-ammx.asm - fast YCbCr to RGB conversion
;
; Copyright 2018 Henryk Richter <henryk.richter@gmx.net>
;
; This file should be assembled with VASM.
; VASM is available from http://sun.hasenbraten.de/vasm/
;
; This file contains the YCbCr to RGB color conversion routines
; for Apollo Core / AMMX. The applied color space is ITU-R BT.601
; as of the JPEG spec.
;
;
	machine	ac68080

;
; Options
;
BT709		EQU	0	;if(1) apply ITU-R BT.709 constants instead of BT.601 (keep 0 here!)



	xdef	_jsimd_ycc_rgb_convert_ammx
	xdef	_jsimd_ycc_rgbx_convert_ammx


;Constants for YUV to RGB conversion
        ifne    BT709
FIX_0_3359      EQU     48      ;scaled by 256
FIX_0_6985      EQU     119
FIX_1_3711      EQU     402
FIX_1_7337      EQU     475
        else
	;BT.601 constants
FIX_0_3359      EQU     88	;86
FIX_0_6985      EQU     183	;179
FIX_1_3711      EQU     359	;351
FIX_1_7337      EQU     454	;444
        endc


; in: A5/A6 = Cb/Cr pointers
;out: E16 = 0
;     E17 = B'0 B'1 B'2 B'3
;     E18 = R'0 R'1 R'2 R'3
;     E19 = G'0 G'1 G'2 G'3
;trash:
; A5++/A6++ D4,D5,E20
RGB24_CHROMABLOCK	macro
		move.l		(a6)+,d4		;x x x x V0 V1 V2 V3 .b
		 peor		E16,E16,E16		;
		 move.l		(a5)+,d5		;x x x x U0 U1 U2 U3 .b
		vperm		#$84858687,d4,E16,E18	;V0.w V1.w V2.w V3.w
		 vperm		#$84858687,d5,E16,E17	;U0.w U1.w U2.w U3.w
		psubw.w		#128,E18,E18		;DC offset
		 pmul88.w	#-FIX_0_6985,E18,E20	;G'2 =V*-119 >>8
		psubw.w		#128,E17,E17		;DC offset
		 pmul88.w	#-FIX_0_3359,E17,E19	;G'1 =U*-48  >>8
		pmul88.w	#FIX_1_3711,E18,E18	;R' = (V*402)>>8
		 pmul88.w	#FIX_1_7337,E17,E17	;B' = (U*475)>>8
		paddw		E19,E20,E19		;G' = U*-48 + V*-119
		;11 cycles for 4 pixels (at 4:4:4)
			endm

;
;destination format: RGB24
;
;void jsimd_ycc_rgb_convert_ammx( register int img_width __asm("d2"),
;                            register JSAMPIMAGE iptr    __asm("a2"),
;                            register JSAMPARRAY optr    __asm("a3"),
;                            register int img_row        __asm("d1"),
;                            register int img_nrows      __asm("d0") );
_jsimd_ycc_rgb_convert_ammx:
	movem.l	d3-d5/a3-a6,-(sp)

	;JSAMPIMAGE: 3D pointer (here), first entry is the color component, from there it's a SAMPIMAGE
	;JSAMPARRAY: pointer to pointers for rows

.row_loop:
	;get input pointers
	move.l		(a2),a4		;Y  array
	move.l		4(a2),a5	;Cb array
	move.l		8(a2),a6	;Cr array
	move.l		(a3)+,a0	;RGB output 
	move.l		(a4,d1.l*4),a4	;Y  row
	move.l		(a5,d1.l*4),a5	;Cb row
	move.l		(a6,d1.l*4),a6	;Cr row
	addq.l		#1,d1		;next row input / output

	move.l		d2,d3		;number of pixels per row
	add.l		d2,d3		;number of pixels per row * 2
	add.l		d2,d3		;number of pixels per row * 3 = total Bytes (RGB24)
.column_loop:
	;A0 = RGB24 output
	;A4/A5/A6 = YCbCr inputs

	RGB24_CHROMABLOCK	;in: A5/A6, out: E16-E19, trash: A5/A6/D4/D5/E20
	;E16 0 0 0 0 0 0 0 0
	;E18 R'0 R'1 R'2 R'3
	;E19 G'0 G'1 G'2 G'3
	;E17 B'0 B'1 B'2 B'3
	load		(a4)+,E4			; Y0 Y1 Y2 Y3 Y4 Y5 Y6 Y7 (do here to avoid bubble = 1)

	TRANSHI 	E16-E19,E0:E1			; E0: 000 B'0 R'0 G'0 E1: 000 B'1 R'1 G'1 .w
	TRANSLO 	E16-E19,E2:E3			; E2: 000 B'2 R'2 G'2 E3: 000 B'3 R'3 G'3 .w

	vperm		#$80808081,E4,E16,E22		; Y0  Y0  Y0  Y1  .w
	vperm		#$81818282,E4,E16,E23		; Y1  Y1  Y2  Y2  .w
	vperm		#$456723cd,E0,E1,E20		; R'0 G'0 B'0 R'1 .w
	vperm		#$6723cdef,E1,E2,E21		; G'1 B'1 R'2 G'2 .w
	paddw		E20,E22,E22			; R0  G0  B0  R1  .w
	paddw		E21,E23,E23			; G1  B1  R2  G2  .w
	packuswb	E22,E23,E5			; R00 G00 B00 R01 G01 B02 R02 G02 .b
	storec		E5,d3,(a0)+			; 2.6 Pixels done
	subq.l		#8,d3

	;next 2.6 pixels require a new set of chroma -> do in between regular ops
	RGB24_CHROMABLOCK	;in: A5/A6, out: E16-E19, trash: A5/A6/D4/D5/E20
	;E16 0 0 0 0 0 0 0 0
	;E18 R'4 R'5 R'6 R'7
	;E19 G'4 G'5 G'6 G'7
	;E17 B'4 B'5 B'6 B'7

	vperm		#$23cdefab,E2,E3,E20		; B'2 R'3 G'3 B'3 .w

	TRANSHI 	E16-E19,E0:E1			; E0: 000 B'4 R'4 G'4 E1: 000 B'5 R'5 G'5 .w
	TRANSLO 	E16-E19,E2:E3			; E2: 000 B'6 R'6 G'6 E3: 000 B'7 R'7 G'7 .w

	vperm		#$82838383,E4,E16,E22		; Y2  Y3  Y3  Y3  .w
	vperm		#$84848485,E4,E16,E23		; Y4  Y4  Y4  Y5  .w
	vperm		#$456723cd,E0,E1,E21		; R'4 G'4 B'4 R'5 .w

	paddw		E20,E22,E22			; R4  G4  B4  R5  .w
	paddw		E21,E23,E23			; G5  B5  R6  G6  .w
	packuswb	E22,E23,E5			; B02 R03 G03 B03 R04 G04 B04 R05 .b
	storec		E5,d3,(a0)+			; 2.6 Pixels done
	subq.l		#8,d3

	vperm		#$85858686,E4,E16,E22		; Y5  Y5  Y6  Y6  .w
	vperm		#$86878787,E4,E16,E23		; Y6  Y7  Y7  Y7  .w
	vperm		#$6723cdef,E1,E2,E20		; G'5 B'5 R'6 G'6 .w
	vperm		#$23cdefab,E2,E3,E21		; B'6 R'7 G'7 B'7 .w
	paddw		E20,E22,E22			; G5  B5  R6  G6  .w
	paddw		E21,E23,E23			; B6  R7  G7  B7  .w
	packuswb	E22,E23,E5			; R00 G00 B00 R01 G01 B02 R02 G02 .b
	;pand.w		#$ff00,e5,e5 ;DEBUG: test whether routine is called by destroying some pixels
	storec		E5,d3,(a0)+			; 2.6 Pixels done

	subq.l	#8,d3		;8 Bytes less
	bgt	.column_loop	;51 cycles for 8 pixels = 6.375 cycles / pixel

	subq.l	#1,d0
	bgt	.row_loop

	movem.l	(sp)+,d3-d5/a3-a6
	rts

;
;destination format: RGBx or RGBA 
;
;void jsimd_ycc_rgbx_convert_ammx( register int img_width __asm("d2"),
;                            register JSAMPIMAGE iptr    __asm("a2"),
;                            register JSAMPARRAY optr    __asm("a3"),
;                            register int img_row        __asm("d1"),
;                            register int img_nrows      __asm("d0") );
_jsimd_ycc_rgbx_convert_ammx:
	movem.l	d3-d5/a3-a6,-(sp)

	;JSAMPIMAGE: 3D pointer (here), first entry is the color component, from there it's a SAMPIMAGE
	;JSAMPARRAY: pointer to pointers for rows

.row_loop:
	;get input pointers
	move.l		(a2),a4		;Y  array
	move.l		4(a2),a5	;Cb array
	move.l		8(a2),a6	;Cr array
	move.l		(a3)+,a0	;RGB output 
	move.l		(a4,d1.l*4),a4	;Y  row
	move.l		(a5,d1.l*4),a5	;Cb row
	move.l		(a6,d1.l*4),a6	;Cr row
	addq.l		#1,d1		;next row input / output

	move.l		d2,d3		;number of pixels per row
	lsl.l		#2,d3		;number of pixels per row * 4 = total Bytes (RGB32)
.column_loop:
	;A0 = RGB24 output
	;A4/A5/A6 = YCbCr inputs

	RGB24_CHROMABLOCK	;in: A5/A6, out: E16-E19, trash: A5/A6/D4/D5/E20
	;E16 0 0 0 0 0 0 0 0
	;E18 R'0 R'1 R'2 R'3
	;E19 G'0 G'1 G'2 G'3
	;E17 B'0 B'1 B'2 B'3
	load		(a4)+,E4			; Y0 Y1 Y2 Y3 Y4 Y5 Y6 Y7 (do here to avoid bubble = 1)

	TRANSHI 	E16-E19,E0:E1			; E0: 000 B'0 R'0 G'0 E1: 000 B'1 R'1 G'1 .w
	TRANSLO 	E16-E19,E2:E3			; E2: 000 B'2 R'2 G'2 E3: 000 B'3 R'3 G'3 .w

	vperm		#$80808080,E4,E16,E22		; Y0 Y0 Y0 Y0 .w
	vperm		#$81818181,E4,E16,E23		; Y1 Y1 Y1 Y1 .w
	vperm		#$45672345,E0,E0,E20		; R'0 G'0 B'0 R'0 .w
	vperm		#$45672345,E1,E1,E21		; R'1 G'1 B'1 R'1 .w
	paddw		E20,E22,E22			; R0  G0  B0  R0  .w
	paddw		E21,E23,E23			; R1  G1  B1  R1  .w
	packuswb	E22,E23,E5			; R00 G00 B00 xxx R01 G01 B01 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done
	subq.l		#8,d3

	vperm		#$82828282,E4,E16,E22		; Y2 Y2 Y2 Y2 .w
	vperm		#$83838383,E4,E16,E23		; Y3 Y3 Y3 Y3 .w
	vperm		#$45672345,E2,E2,E20		; R'0 G'0 B'0 R'0 .w
	vperm		#$45672345,E3,E3,E21		; R'1 G'1 B'1 R'1 .w
	paddw		E20,E22,E22			; R2  G2  B2  R2  .w
	paddw		E21,E23,E23			; R3  G3  B3  R3  .w
	packuswb	E22,E23,E5			; R02 G02 B02 xxx R03 G03 B03 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done (total 4)
	subq.l		#8,d3

	;next 2 pixels require a new set of chroma -> do in between regular ops
	RGB24_CHROMABLOCK	;in: A5/A6, out: E16-E19, trash: A5/A6/D4/D5/E20
	;E16 0 0 0 0 0 0 0 0
	;E18 R'4 R'5 R'6 R'7
	;E19 G'4 G'5 G'6 G'7
	;E17 B'4 B'5 B'6 B'7

	TRANSHI 	E16-E19,E0:E1			; E0: 000 B'4 R'4 G'4 E1: 000 B'5 R'5 G'5 .w
	TRANSLO 	E16-E19,E2:E3			; E2: 000 B'6 R'6 G'6 E3: 000 B'7 R'7 G'7 .w

	vperm		#$84848484,E4,E16,E22		; Y4 Y4 Y4 Y4 .w
	vperm		#$85858585,E4,E16,E23		; Y5 Y5 Y5 Y5 .w
	vperm		#$45672345,E0,E0,E20		; R'4 G'4 B'4 R'4 .w
	vperm		#$45672345,E1,E1,E21		; R'5 G'5 B'5 R'5 .w
	paddw		E20,E22,E22			; R4  G4  B4  R4  .w
	paddw		E21,E23,E23			; R5  G5  B5  R5  .w
	packuswb	E22,E23,E5			; R04 G04 B04 xxx R05 G05 B05 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done (total 6)
	subq.l		#8,d3

	vperm		#$86868686,E4,E16,E22		; Y6 Y6 Y6 Y6 .w
	vperm		#$87878787,E4,E16,E23		; Y7 Y7 Y7 Y7 .w
	vperm		#$45672345,E2,E2,E20		; R'6 G'6 B'6 R'6 .w
	vperm		#$45672345,E3,E3,E21		; R'7 G'7 B'7 R'7 .w
	paddw		E20,E22,E22			; R6  G6  B6  R6  .w
	paddw		E21,E23,E23			; R7  G7  B7  R7  .w
	packuswb	E22,E23,E5			; R06 G06 B06 xxx R07 G07 B07 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done (total 8)

	subq.l	#8,d3		;8 Bytes less
	bgt	.column_loop	;51 cycles for 8 pixels = 6.375 cycles / pixel

	subq.l	#1,d0
	bgt	.row_loop

	movem.l	(sp)+,d3-d5/a3-a6
	rts


