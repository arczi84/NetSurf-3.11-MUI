; jdmerge-ammx.asm - fast YCbCr to RGB conversion for 4:2:2 and 4:2:0
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

	section	text,code

;
; Options
;
BT709		EQU	0	;if(1) apply ITU-R BT.709 constants instead of BT.601 (keep 0 here!)
NROWS_IS_1	EQU	1	;set 1, if nrows is always one

	xdef	_jsimd_h2v1_merged_upsample_ammx
	xdef	_jsimd_h2v2_merged_upsample_ammx
	xdef	_jsimd_h2v1_merged_upsample_rgb32_ammx
	xdef	_jsimd_h2v2_merged_upsample_rgb32_ammx




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
		;11 cycles for 8 pixels (at 4:2:2)
			endm


;void jsimd_h2v1_merged_upsample_ammx( register int img_width __asm("d2"),
;                            register JSAMPIMAGE iptr    __asm("a2"),
;                            register JSAMPARRAY optr    __asm("a3"),
;                            register int img_row        __asm("d1"),
;                            register int img_nrows      __asm("d0") );
_jsimd_h2v1_merged_upsample_ammx:
	ifne	NROWS_IS_1
	 movem.l	d3-d5/a4-a6,-(sp)
	else
	 movem.l	d3-d5/a3-a6,-(sp)
	endc

	;JSAMPIMAGE: 3D pointer (here), first entry is the color component, from there it's a SAMPIMAGE
	;JSAMPARRAY: pointer to pointers for rows

.row_loop:
	;get input pointers
	move.l		(a2),a4		;Y  array
	move.l		4(a2),a5	;Cb array
	move.l		8(a2),a6	;Cr array
	ifne	NROWS_IS_1
	 move.l		(a3),a0		;RGB output
	else
	 move.l		(a3)+,a0	;RGB output 
	endc
	move.l		(a4,d1.l*4),a4	;Y  row
	move.l		d2,d3		;F number of pixels per row
	add.l		d2,d3		;F number of pixels per row * 2
	move.l		(a5,d1.l*4),a5	;Cb row
	add.l		d2,d3		;number of pixels per row * 3 = total Bytes (RGB24)
	move.l		(a6,d1.l*4),a6	;Cr row
	addq.l		#1,d1		;next row input / output

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
	vperm		#$45672345,E0,E1,E20		; R'0 G'0 B'0 R'0 .w
	vperm		#$6723cdef,E0,E1,E21		; G'0 B'0 R'1 G'1 .w
	paddw		E20,E22,E22			; R0  G0  B0  R1  .w
	paddw		E21,E23,E23			; G1  B1  R2  G2  .w
	packuswb	E22,E23,E5			; R00 G00 B00 R01 G01 B02 R02 G02 .b

	vperm		#$82838383,E4,E16,E22		; Y2  Y3  Y3  Y3  .w
	 storec		E5,d3,(a0)+			; 2.6 Pixels done
	vperm		#$84848485,E4,E16,E23		; Y4  Y4  Y4  Y5  .w
	 subq.l		#8,d3
	vperm		#$23456723,E1,E2,E20		; B'1 R'1 G'1 B'1 .w
	vperm		#$45672345,E2,E3,E21		; R'2 G'2 B'2 R'2 .w
	paddw		E20,E22,E22			; R4  G4  B4  R5  .w
	paddw		E21,E23,E23			; G5  B5  R6  G6  .w
	packuswb	E22,E23,E5			; B02 R03 G03 B03 R04 G04 B04 R05 .b

	vperm		#$85858686,E4,E16,E22		; Y5  Y5  Y6  Y6  .w
	 storec		E5,d3,(a0)+			; 2.6 Pixels done
	vperm		#$86878787,E4,E16,E23		; Y6  Y7  Y7  Y7  .w
	 subq.l		#8,d3
	vperm		#$6723cdef,E2,E3,E20		; G'2 B'2 R'3 G'3 .w
	vperm		#$abcdefab,E2,E3,E21		; B'3 R'3 G'3 B'3 .w
	paddw		E20,E22,E22			; G5  B5  R6  G6  .w
	paddw		E21,E23,E23			; B6  R7  G7  B7  .w
	packuswb	E22,E23,E5			; R00 G00 B00 R01 G01 B02 R02 G02 .b
	;por.w	#$ff00,e5,e5
	storec		E5,d3,(a0)+			; 2.6 Pixels done

	subq.l	#8,d3		;8 Bytes less
	bgt	.column_loop	;38 cycles for 8 pixels = 4.75 cycles / pixel

	ifne	NROWS_IS_1
	 movem.l	(sp)+,d3-d5/a4-a6
	else
	 subq.l	#1,d0
	 bgt	.row_loop
	 movem.l	(sp)+,d3-d5/a3-a6
	endc

	rts

;void jsimd_h2v2_merged_upsample_ammx( register int img_width __asm("d2"),
;                            register JSAMPIMAGE iptr    __asm("a2"),
;                            register JSAMPARRAY optr    __asm("a3"),
;                            register int img_row        __asm("d1"),
;                            register int img_nrows      __asm("d0") );
_jsimd_h2v2_merged_upsample_ammx:
	movem.l	d3-d6/a3-a6,-(sp)

	;JSAMPIMAGE: 3D pointer (here), first entry is the color component, from there it's a SAMPIMAGE
	;JSAMPARRAY: pointer to pointers for rows

.row_loop:
	;get input pointers
	move.l		(a2),a4		;Y  array
	move.l		4(a2),a5	;Cb array
	move.l		8(a2),a6	;Cr array
	move.l		(a3)+,a0	;RGB output 1
	move.l		(a3)+,a1	;RGB output 2

	ifeq	NROWS_IS_1
	 move.l		a3,d6		;save A3 (in multi-row mode only)
	endc

	move.l		4(a4,d1.l*8),a3	;Y2 row 
	move.l		(a4,d1.l*8),a4	;Y  row
	move.l		(a5,d1.l*4),a5	;Cb row
	moveq		#3,d3
	move.l		(a6,d1.l*4),a6	;Cr row
	addq.l		#1,d1		;next row input / output

	mulu.l		d2,d3		;number of pixels per row (input) *3 (RGB24)
	; note that d2 is input width, *2 is done below by decrementing d3 half as often

;.column_loop:
	;A0 = RGB24 output
	;A4/A5/A6 = YCbCr inputs

	RGB24_CHROMABLOCK	;in: A5/A6, out: E16-E19, trash: A5/A6/D4/D5/E20
	;E16 0 0 0 0 0 0 0 0
	;E18 R'0 R'1 R'2 R'3
	;E19 G'0 G'1 G'2 G'3
	;E17 B'0 B'1 B'2 B'3
.column_loop:

	load		(a4)+,E4			; Y00 Y01 Y02 Y03 Y04 Y05 Y06 Y07 (do here to avoid bubble = 1)
	load		(a3)+,E6			; Y10 Y11 Y12 Y13 Y14 Y15 Y16 Y17 (do here to avoid bubble = 1)

	TRANSHI 	E16-E19,E0:E1			; E0: 000 B'0 R'0 G'0 E1: 000 B'1 R'1 G'1 .w
	TRANSLO 	E16-E19,E2:E3			; E2: 000 B'2 R'2 G'2 E3: 000 B'3 R'3 G'3 .w
	
	vperm		#$80808081,E6,E16,E22		; Y10 Y10 Y10 Y11 .w
	vperm		#$81818282,E6,E16,E23		; Y11 Y11 Y12 Y12 .w
	vperm		#$45672345,E0,E1,E20		; R'0 G'0 B'0 R'0 .w
	vperm		#$6723cdef,E0,E1,E21		; G'0 B'0 R'1 G'1 .w
	paddw		E20,E22,E22			; R0  G0  B0  R1  .w
	paddw		E21,E23,E23			; G1  B1  R2  G2  .w
	packuswb	E22,E23,E5			; R00 G00 B00 R01 G01 B02 R02 G02 .b

	vperm		#$80808081,E4,E16,E22		; Y00 Y00 Y00 Y01 .w
	vperm		#$81818282,E4,E16,E23		; Y01 Y01 Y02 Y02 .w
	vperm		#$45672345,E0,E1,E20		; R'0 G'0 B'0 R'0 .w
	vperm		#$6723cdef,E0,E1,E21		; G'0 B'0 R'1 G'1 .w
	 storec		E5,d3,(a1)+			; 2.6 Pixels done
	paddw		E20,E22,E22			; R0  G0  B0  R1  .w
	paddw		E21,E23,E23			; G1  B1  R2  G2  .w
	packuswb	E22,E23,E5			; R00 G00 B00 R01 G01 B02 R02 G02 .b

	vperm		#$82838383,E6,E16,E22		; Y12 Y13 Y13 Y13 .w
	vperm		#$84848485,E6,E16,E23		; Y14 Y14 Y14 Y15 .w
	vperm		#$23456723,E1,E2,E20		; B'1 R'1 G'1 B'1 .w
	vperm		#$45672345,E2,E3,E21		; R'2 G'2 B'2 R'2 .w
	 storec		E5,d3,(a0)+			; 2.6 Pixels done
	paddw		E20,E22,E22			; R4  G4  B4  R5  .w
	 subq.l		#8,d3
	paddw		E21,E23,E23			; G5  B5  R6  G6  .w
	packuswb	E22,E23,E5			; B02 R03 G03 B03 R04 G04 B04 R05 .b

	vperm		#$82838383,E4,E16,E22		; Y02 Y03 Y03 Y03 .w
	vperm		#$84848485,E4,E16,E23		; Y04 Y04 Y04 Y05 .w
	vperm		#$23456723,E1,E2,E20		; B'1 R'1 G'1 B'1 .w
	vperm		#$45672345,E2,E3,E21		; R'2 G'2 B'2 R'2 .w
	 storec		E5,d3,(a1)+			; 2.6 Pixels done
	paddw		E20,E22,E22			; R4  G4  B4  R5  .w
	paddw		E21,E23,E23			; G5  B5  R6  G6  .w
	packuswb	E22,E23,E5			; B02 R03 G03 B03 R04 G04 B04 R05 .b

	vperm		#$85858686,E6,E16,E22		; Y15 Y15 Y16 Y16 .w
	vperm		#$86878787,E6,E16,E23		; Y16 Y17 Y17 Y17 .w
	vperm		#$6723cdef,E2,E3,E20		; G'2 B'2 R'3 G'3 .w
	vperm		#$abcdefab,E2,E3,E21		; B'3 R'3 G'3 B'3 .w
	 storec		E5,d3,(a0)+			; 2.6 Pixels done
	paddw		E20,E22,E22			; G5  B5  R6  G6  .w
	 subq.l		#8,d3
	paddw		E21,E23,E23			; B6  R7  G7  B7  .w
	packuswb	E22,E23,E5			; R00 G00 B00 R01 G01 B02 R02 G02 .b

	vperm		#$85858686,E4,E16,E22		; Y05 Y05 Y06 Y06 .w
	vperm		#$86878787,E4,E16,E23		; Y06 Y07 Y07 Y07 .w
	vperm		#$6723cdef,E2,E3,E20		; G'2 B'2 R'3 G'3 .w
	vperm		#$abcdefab,E2,E3,E21		; B'3 R'3 G'3 B'3 .w
	 storec		E5,d3,(a1)+			; 2.6 Pixels done
	paddw		E20,E22,E22			; G5  B5  R6  G6  .w
	paddw		E21,E23,E23			; B6  R7  G7  B7  .w
	packuswb	E22,E23,E5			; R00 G00 B00 R01 G01 B02 R02 G02 .b
	;pand.w	#$f000,e5,e5

	RGB24_CHROMABLOCK	;in: A5/A6, out: E16-E19, trash: A5/A6/D4/D5/E20

	storec		E5,d3,(a0)+			; 2.6 Pixels done
	subq.l		#8,d3		;8 Bytes less
	bgt		.column_loop	;63 cycles for 16 pixels = 3.9375 cycles / pixel

	ifeq	NROWS_IS_1
	 move.l		d6,a3		;restore A3
	 subq.l		#1,d0
	 bgt		.row_loop		;disabled: multiple rows unsupported right now
	endc

	movem.l	(sp)+,d3-d6/a3-a6
	rts

;
; RGBx 32 Bit variant
;
;void jsimd_h2v1_merged_upsample_rgb32_ammx( register int img_width __asm("d2"),
;                            register JSAMPIMAGE iptr    __asm("a2"),
;                            register JSAMPARRAY optr    __asm("a3"),
;                            register int img_row        __asm("d1"),
;                            register int img_nrows      __asm("d0") );
_jsimd_h2v1_merged_upsample_rgb32_ammx:
	ifne	NROWS_IS_1
	 movem.l	d3-d5/a4-a6,-(sp)
	else
	 movem.l	d3-d5/a3-a6,-(sp)
	endc

	;JSAMPIMAGE: 3D pointer (here), first entry is the color component, from there it's a SAMPIMAGE
	;JSAMPARRAY: pointer to pointers for rows

.row_loop:
	;get input pointers
	move.l		(a2),a4		;Y  array
	move.l		4(a2),a5	;Cb array
	move.l		8(a2),a6	;Cr array
	ifne	NROWS_IS_1
	 move.l		(a3),a0		;RGB output
	else
	 move.l		(a3)+,a0	;RGB output 
	endc
	move.l		(a4,d1.l*4),a4	;Y  row
	move.l		d2,d3		;F number of pixels per row
	lsl.l		#2,d3		;F number of pixels per row * 4 = total Bytes (RGB32)
	move.l		(a5,d1.l*4),a5	;Cb row
	move.l		(a6,d1.l*4),a6	;Cr row
	addq.l		#1,d1		;next row input / output

.column_loop:
	;A0 = RGB24 output
	;A4/A5/A6 = YCbCr inputs

	;TODO: make outputs in E16,E17,E18,E19 as R' G' B' 0, saves one vperm per 2 pixels
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
	paddw		E20,E22,E22			; R0  G0  B0  R0  .w
	paddw		E20,E23,E23			; R1  G1  B1  R1  .w
	packuswb	E22,E23,E5			; R00 G00 B00 xxx R01 G01 B01 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done
	subq.l		#8,d3

	vperm		#$82828282,E4,E16,E22		; Y2 Y2 Y2 Y2 .w
	vperm		#$83838383,E4,E16,E23		; Y3 Y3 Y3 Y3 .w
	vperm		#$45672345,E1,E1,E20		; R'1 G'1 B'1 R'1 .w
	paddw		E20,E22,E22			; R2  G2  B2  R2  .w
	paddw		E20,E23,E23			; R3  G3  B3  R3  .w
	packuswb	E22,E23,E5			; R02 G02 B02 xxx R03 G03 B03 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done
	subq.l		#8,d3

	vperm		#$84848484,E4,E16,E22		; Y4 Y4 Y4 Y4 .w
	vperm		#$85858585,E4,E16,E23		; Y5 Y5 Y5 Y5 .w
	vperm		#$45672345,E2,E2,E20		; R'0 G'0 B'0 R'0 .w
	paddw		E20,E22,E22			; R0  G0  B0  R0  .w
	paddw		E20,E23,E23			; R1  G1  B1  R1  .w
	packuswb	E22,E23,E5			; R04 G04 B04 xxx R05 G05 B05 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done
	subq.l		#8,d3

	vperm		#$86868686,E4,E16,E22		; Y6 Y6 Y6 Y6 .w
	vperm		#$87878787,E4,E16,E23		; Y7 Y7 Y7 Y7 .w
	vperm		#$45672345,E3,E3,E20		; R'1 G'1 B'1 R'1 .w
	paddw		E20,E22,E22			; R2  G2  B2  R2  .w
	paddw		E20,E23,E23			; R3  G3  B3  R3  .w
	packuswb	E22,E23,E5			; R06 G06 B06 xxx R07 G07 B07 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done
	subq.l		#8,d3
	bgt	.column_loop	;

	ifne	NROWS_IS_1
	 movem.l	(sp)+,d3-d5/a4-a6
	else
	 subq.l	#1,d0
	 bgt	.row_loop
	 movem.l	(sp)+,d3-d5/a3-a6
	endc

	rts

;
; RGBx 32 Bit variant
;
;void jsimd_h2v2_merged_upsample_rgb32_ammx( register int img_width __asm("d2"),
;                            register JSAMPIMAGE iptr    __asm("a2"),
;                            register JSAMPARRAY optr    __asm("a3"),
;                            register int img_row        __asm("d1"),
;                            register int img_nrows      __asm("d0") );
_jsimd_h2v2_merged_upsample_rgb32_ammx:
	movem.l	d3-d6/a3-a6,-(sp)

	;JSAMPIMAGE: 3D pointer (here), first entry is the color component, from there it's a SAMPIMAGE
	;JSAMPARRAY: pointer to pointers for rows

.row_loop:
	;get input pointers
	move.l		(a2),a4		;Y  array
	move.l		4(a2),a5	;Cb array
	move.l		8(a2),a6	;Cr array
	move.l		(a3)+,a0	;RGB output 1
	move.l		(a3)+,a1	;RGB output 2

	ifeq	NROWS_IS_1
	 move.l		a3,d6		;save A3 (in multi-row mode only)
	endc

	move.l		4(a4,d1.l*8),a3	;Y2 row 
	move.l		(a4,d1.l*8),a4	;Y  row
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
	load		(a4)+,E4			; Y00 Y01 Y02 Y03 Y04 Y05 Y06 Y07 (do here to avoid bubble = 1)
	load		(a3)+,E6			; Y10 Y11 Y12 Y13 Y14 Y15 Y16 Y17 (do here to avoid bubble = 1)

	TRANSHI 	E16-E19,E0:E1			; E0: 000 B'0 R'0 G'0 E1: 000 B'1 R'1 G'1 .w
	TRANSLO 	E16-E19,E2:E3			; E2: 000 B'2 R'2 G'2 E3: 000 B'3 R'3 G'3 .w

	vperm		#$80808080,E6,E16,E22		; 
	vperm		#$81818181,E6,E16,E23		; 
	vperm		#$45672345,E0,E0,E20		; 
	paddw		E20,E22,E22			; 
	paddw		E20,E23,E23			; 
	packuswb	E22,E23,E5			; 
	storec		E5,d3,(a1)+			; 

	vperm		#$80808080,E4,E16,E22		; Y0 Y0 Y0 Y0 .w
	vperm		#$81818181,E4,E16,E23		; Y1 Y1 Y1 Y1 .w
	vperm		#$45672345,E0,E0,E20		; R'0 G'0 B'0 R'0 .w
	paddw		E20,E22,E22			; R0  G0  B0  R0  .w
	paddw		E20,E23,E23			; R1  G1  B1  R1  .w
	packuswb	E22,E23,E5			; R00 G00 B00 xxx R01 G01 B01 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done
	subq.l		#8,d3
	;---------------------------------------

	vperm		#$82828282,E6,E16,E22		; 
	vperm		#$83838383,E6,E16,E23		; 
	vperm		#$45672345,E1,E1,E20		; 
	paddw		E20,E22,E22			; 
	paddw		E20,E23,E23			; 
	packuswb	E22,E23,E5			; 
	storec		E5,d3,(a1)+			; 

	vperm		#$82828282,E4,E16,E22		; Y2 Y2 Y2 Y2 .w
	vperm		#$83838383,E4,E16,E23		; Y3 Y3 Y3 Y3 .w
	vperm		#$45672345,E1,E1,E20		; R'1 G'1 B'1 R'1 .w
	paddw		E20,E22,E22			; R2  G2  B2  R2  .w
	paddw		E20,E23,E23			; R3  G3  B3  R3  .w
	packuswb	E22,E23,E5			; R02 G02 B02 xxx R03 G03 B03 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done
	subq.l		#8,d3
	;-----------------------------------------

	vperm		#$84848484,E6,E16,E22		; 
	vperm		#$85858585,E6,E16,E23		; 
	vperm		#$45672345,E2,E2,E20		; 
	paddw		E20,E22,E22			; 
	paddw		E20,E23,E23			; 
	packuswb	E22,E23,E5			; 
	storec		E5,d3,(a1)+			; 

	vperm		#$84848484,E4,E16,E22		; Y4 Y4 Y4 Y4 .w
	vperm		#$85858585,E4,E16,E23		; Y5 Y5 Y5 Y5 .w
	vperm		#$45672345,E2,E2,E20		; R'0 G'0 B'0 R'0 .w
	paddw		E20,E22,E22			; R0  G0  B0  R0  .w
	paddw		E20,E23,E23			; R1  G1  B1  R1  .w
	packuswb	E22,E23,E5			; R04 G04 B04 xxx R05 G05 B05 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done
	subq.l		#8,d3
	;-------------------------------------------

	vperm		#$86868686,E6,E16,E22		; 
	vperm		#$87878787,E6,E16,E23		; 
	vperm		#$45672345,E3,E3,E20		; 
	paddw		E20,E22,E22			; 
	paddw		E20,E23,E23			; 
	packuswb	E22,E23,E5			; 
	storec		E5,d3,(a1)+			; 

	vperm		#$86868686,E4,E16,E22		; Y6 Y6 Y6 Y6 .w
	vperm		#$87878787,E4,E16,E23		; Y7 Y7 Y7 Y7 .w
	vperm		#$45672345,E3,E3,E20		; R'1 G'1 B'1 R'1 .w
	paddw		E20,E22,E22			; R2  G2  B2  R2  .w
	paddw		E20,E23,E23			; R3  G3  B3  R3  .w
	packuswb	E22,E23,E5			; R06 G06 B06 xxx R07 G07 B07 xxx .b
	storec		E5,d3,(a0)+			; 2 Pixels done

	subq.l		#8,d3		;8 Bytes less
	bgt		.column_loop	;63 cycles for 16 pixels = 3.9375 cycles / pixel

	ifeq	NROWS_IS_1
	 move.l		d6,a3		;restore A3
	 subq.l		#1,d0
	 bgt		.row_loop		;disabled: multiple rows unsupported right now
	endc

	movem.l	(sp)+,d3-d6/a3-a6
	rts


