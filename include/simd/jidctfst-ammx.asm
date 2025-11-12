; jidctfst-ammx.asm - fast integer IDCT (MMX)
;
; Copyright 2018 Henryk Richter <henryk.richter@gmx.net>
;
; This file should be assembled with VASM.
; VASM is available from http://sun.hasenbraten.de/vasm/
;
; This file contains a fast, not so accurate integer implementation of
; the inverse DCT (Discrete Cosine Transform). The following code 
; resembles the algorithm in IJG's original jidctfst.c; see the jidctfst.c
; for more details towards the scaled AAN.
;
;
	section	".text",code

	include "jconfig68k.i"
	include "jpeglib68k.i"

	machine	ac68080

;
; Options: check for zero AC in vertical stage and skip full processing in that case
;
OPT_ZEROCHECK		EQU	1
OPT_MAXCOEF		EQU	0	;if(1), then A3 is used
OPT_QUICK_DCONLY	EQU	1	;if(1), then A3 is used, also skips 

	xdef	_jsimd_idct_ifast_ammx

;IDCT Contstants, not enough to get even close to IEEE1180 --> but on 68k, compromises are in order...
C4              EQU     1448/4                  ; 1.414213562 << 10     / SQRT(2)     
C6              EQU     784/4                   ; 0.7653668647 << 10    / 2*Sin(Pi/8) 
Q               EQU     -1108/4                 ; 1.0823922 << 10       / -(C2 - C6)  
R               EQU     2676/4                  ; 2.61312593 << 10      / C2 + C6     
FIX_1_082392200 EQU     1108/4                  ; -Q
FIX_2_613125930 EQU     -2676/4                 ; -R
FIX_1_847759065 EQU     1892/4                  ; C2
SCALEDOWN	EQU	8			; for pmul88, i.e. 8/256 = 1/32 (>>5)


;Input:
; A0 - dequant coefficients (shorts)
; A1 - input array, organized 8x8 (shorts)
; A2 - output pointers, one per row
; A3 - cinfo (used only with OPT_MAXCOEF=1 or OPT_QUICK_DCONLY=1, otherwise ignored)
; D0 - output_col - offset to pointers at a2

_jsimd_idct_ifast_ammx:
	ifne	OPT_QUICK_DCONLY
		moveq	#-1,d1
		add.b	JD_MCU_maxcoef+D_MAX_BLOCKS_IN_MCU(a3),d1	;last coeff for current block or "0" for unsure
		beq	dconly						;easy: just dequant DC, write block, return
	endc	;QUICK_DCONLY

	movem.l	d2-d7,-(sp)	;save data regs
	move.l	d0,-(sp)	;save D0 (column offset)

	; code block needed twice, defined as parameterless macro 
	; (insert into code for debugging)
IDCTX	macro
	
	ifne	\1

	 load	   (a1),E16	;Fq(0)	;check population -> DC only check
	 load	 16(a1),E17	;Fq(1)
	 load	 32(a1),E18	;Fq(2)
	 load	    E17,D0
	 load	 48(a1),E19	;Fq(3)
	 por	 E18,D0,D0
	 load	 64(a1),E20	;Fq(4)
	 por	 E19,D0,D0

	 load	 80(a1),E21	;Fq(5)
	 por	 E20,D0,D0

	 load	 96(a1),E22	;Fq(6)
	 por	 E21,D0,D0

	 load	112(a1),E23	;Fq(7)
	 por	 E22,D0,D0

	 addq.l	#8,a1	;next 4 columns
	 por	 E23,D0,D0

	 vperm	 #$01230123,d0,d0,d1	;get upper half into lower half of D1

	 pmull	 (a0),E16,E16 ;F(0) - dequant DC done (TODO: replace E16 by E8, saves one cycle in fast path)

	 or.l	 d1,d0
	 beq	.idctx_zero\0\@		;AC ONLY

	else

	 load		   (a1),E16	;Fq(0)	;
	 load		 16(a1),E17	;Fq(1)
	 load		 32(a1),E18	;Fq(2)
	 load		 48(a1),E19	;Fq(3)
	 load		 64(a1),E20	;Fq(4)
	 load		 80(a1),E21	;Fq(5)
	 load		 96(a1),E22	;Fq(6)
	 load		112(a1),E23	;Fq(7)
	 addq.l		#8,a1	;next 4 columns
	 pmull		(a0),E16,E16	;F(0) - dequant DC done

	endc

	;dequant AC
	pmull   	 16(a0),E17,E17	;F(1)
	pmull   	 32(a0),E18,E18	;F(2)
	pmull   	 48(a0),E19,E19	;F(3)
	pmull   	 64(a0),E20,E20	;F(4)
	pmull   	 80(a0),E21,E21	;F(5)
	pmull   	 96(a0),E22,E22	;F(6)
	pmull   	112(a0),E23,E23	;F(7)
	addq.l	#8,a0	;next 4 columns

	BFLYW		E20,E16,E12	;E12 tmp10 =F(0)+F(4) E13 tmp11=F(0)-F(4)
	BFLYW		E22,E18,E14	;E14 tmp13 =F(2)+F(6) E15 tmp02=F(2)-F(6)
	BFLYW		E23,E17,E8	;E8  z11=F(1)+F(7)     E9 z12=F(1)-F(7)
	BFLYW		E19,E21,E10	;E10 z13=F(5)+F(3)    E11 z10=F(5)-F(3)

	PMUL88.w	#C4,E15,E15	;E15 tmp12=(tmp02*C4)>>8
	BFLYW		E14,E12,E16	;E16 tmp0=tmp10+tmp13 E17 tmp3=tmp10-tmp13
	BFLYW		E10,E8,E18	;E18 tmp7=z11+z13     E19 tmp11=z11-z13
	PSUBW		E14,E15,E15	;E15 tmp12=(tmp02*C4)>>8 - tmp13 = tmp12 - tmp13

	BFLYW		E15,E13,E14	;E14 tmp1=tmp11+tmp12 E15 tmp2=tmp11-tmp12
	PADDW		E11,E9,E8	;E8  z50 = z10+Z12

	PMUL88.w	#FIX_1_847759065,E8,E8	 ; E8  z5 = (z10+z12)*C2
	PMUL88.w	#FIX_2_613125930,E11,E11 ; E11 tmp12 = -(c2+c6) * z10
	PMUL88.w	#FIX_1_082392200,E9,E9	 ; E9  tmp10 = z12*(c2-c6)
	PMUL88.w	#C4,E19,E19		 ; E19 tmp11=(z11-z13)*C4>>8

	PADDW		E8,E11,E11		 ; E11 tmp12 = -(c2+c6) * z10 + z5
	PSUBW		E8,E9,E9	;E9  tmp10 = z12*(c2-c6) - z5
	PSUBW		E18,E11,E20	;E20 tmp6  = tmp12-tmp7
	PSUBW		E20,E19,E19	;E19 tmp5  = tmp11-tmp6
	PADDW		E19,E9,E9	;E9  tmp4  = tmp10+tmp5
		; now 4 butterflies and then a register swap and transpose
		; BFLY doesn't apply here nicely, we'd rather use transpose, hence manual butterflies
		; -> for vertical stage, we can butterfly directly to outputs...
	PSUBW		E9,E17,E11	;E11 = tmp3-tmp4
	PADDW		E9,E17,E12	;E12 = tmp3+tmp4
	PADDW		E18,E16,E8	;E8  = tmp0+tmp7
	PADDW		E20,E14,E9 	;E9  = tmp1+tmp6
	PADDW		E19,E15,E10	;E10 = tmp2+tmp5
	PSUBW		E19,E15,E13	;E13 = tmp2-tmp5
	PSUBW		E20,E14,E14	;E14 = tmp1-tmp6
	PSUBW		E18,E16,E15 	;E15 = tmp0-tmp7

	;Transpose into D0-D7,E0-E7 (for safekeeping)
	TRANSHi		E8-E11,\2:\3	;E0: A0 A1 A2 A3  E1: B0 B1 B2 B3
	TRANSLo		E8-E11,\4:\5    ;E2: C0 C1 C2 C3  E3: D0 D1 D2 D3
	TRANSHi		E12-E15,\6:\7   ;E4: A4 A5 A6 A7  E5: B4 B5 B6 B7
	TRANSLo		E12-E15,\8:\9   ;E6: C4 C5 C6 C7  e7: D4 D5 D6 D7

	ifne	\1
	bra.s		.idctx_end\0\@
.idctx_zero\0\@		;DC ONLY - copy dequantized DC to AC
	load		E16,E17
	load		E16,E18
	load		E16,E19
	TRANSHi		E16-E19,\2:\3
	TRANSLo		E16-E19,\4:\5
	TRANSHi		E16-E19,\6:\7
	TRANSLo		E16-E19,\8:\9
.idctx_end\0\@
	endc

	endm


	;Block before Transpose (columns A-F, rows 0-7)
	;A0 B0 C0 D0  E0 F0 G0 H0 
	;A1 B1 C1 D1  E1 F1 G1 H1 
	;A2 B2 C2 D2  E2 F2 G2 H2 
	;A3 B3 C3 D3  E3 F3 G3 H3 
	;A4 B4 C4 D4  E4 F4 G4 H4 
	;A5 B5 C5 D5  E5 F5 G5 H5 
	;A6 B6 C6 D6  E6 F6 G6 H6 
	;A7 B7 c7 D7  E7 F7 G7 H7 

	;
	;
	;  STEP 1: TRANSFORM ACROSS COLUMNS (vertical)
	;  - data is in adjacent rows in DCT array
	;  - fetch, dequantize
	;  - transform using scaled AAN
	;  - after transform, transpose block for step 2
	;
	;

	;columns A-D
	IDCTX		0,E0,E1,E2,E3,E4,E5,E6,E7 ;A0+=8,A1+=8, used E8-E23/D0/D1, out in E0-E7

	;Transpose into D0-D7 (for safekeeping)
	;TRANSHi		E8-E11,E0:E1	;E0: A0 A1 A2 A3  E1: B0 B1 B2 B3
	;TRANSLo		E8-E11,E2:E3    ;E2: C0 C1 C2 C3  E3: D0 D1 D2 D3
	;TRANSHi		E12-E15,E4:E5   ;E4: A4 A5 A6 A7  E5: B4 B5 B6 B7
	;TRANSLo		E12-E15,E6:E7   ;E6: C4 C5 C6 C7  e7: D4 D5 D6 D7


	ifne	OPT_MAXCOEF
	 move.b	JD_MCU_maxcoef+D_MAX_BLOCKS_IN_MCU(a3),d0	;last coeff for current block or "0" for unsure
	 beq.s	.full_idctx2
	 cmp.b	#13,d0						;directly check for more than 4 columns, don't consult table
	 bhi.s	.full_idctx2
	 peor	D0,D0,D0
	 peor	D1,D1,D1
	 peor	D2,D2,D2
	 peor	D3,D3,D3
	 peor	D4,D4,D4
	 peor	D5,D5,D5
	 peor	D6,D6,D6
	 peor	D7,D7,D7
	 bra	.short_idctx2
.full_idctx2:
	endc

	;columns E-H
	IDCTX		1,D0,D1,D2,D3,D4,D5,D6,D7 ;A0+8,A1+8, used E8-E23/D0/D1, out in D0-D7

	;Transpose into E0-E7 (for safekeeping)
	;TRANSHi		E8-E11,D0:D1	;d0: E0 E1 E2 E3  d1: F0 F1 F2 F3
	;TRANSLo		E8-E11,D2:D3    ;d2: G0 G1 G2 G3  d3: H0 H1 H2 H3
	;TRANSHi		E12-E15,D4:D5   ;d4: E4 E5 E6 E7  d5: F4 F5 F6 F7
	;TRANSLo		E12-E15,D6:D7   ;d6: G4 G5 G6 G7  d7: H4 H5 H6 H7
.short_idctx2:
	;
	;
	;  STEP 2: TRANSFORM ACROSS ROWS (horizontal)
	;  - data is already transposed in D0-D7/E0-E7
	;  - do first butterfly out of loop using targeted registers
	;  - transform using scaled AAN
	;  - after transform, transpose block back to natural order
	;
	;

	;Input:
	; first 8x4 Block
	;  E0 E4
	;  E1 E5
	;  E2 E6
	;  E3 E7
	; 2nd 8x4 Block
	;  D0 D4
	;  D1 D5
	;  D2 D6
	;  D3 D7
	;
	;Output: inverse transform result


IDCTY	macro
	PMUL88.w	#C4,E15,E15	;E15 tmp12=(tmp02*C4)>>8
	BFLYW		E14,E12,E16	;E16 tmp0=tmp10+tmp13 E17 tmp3=tmp10-tmp13
	BFLYW		E10,E8,E18	;E18 tmp7=z11+z13     E19 tmp11=z11-z13
	PSUBW		E14,E15,E15	;E15 tmp12=(tmp02*C4)>>8 - tmp13 = tmp12 - tmp13

	BFLYW		E15,E13,E14	;E14 tmp1=tmp11+tmp12 E15 tmp2=tmp11-tmp12
	PADDW		E11,E9,E8	;E8  z50 = z10+Z12

	PMUL88.w	#FIX_1_847759065,E8,E8	 ; E8  z5 = (z10+z12)*C2
	PMUL88.w	#FIX_2_613125930,E11,E11 ; E11 tmp12 = -(c2+c6) * z10
	PMUL88.w	#FIX_1_082392200,E9,E9	 ; E9  tmp10 = z12*(c2-c6)
	PMUL88.w	#C4,E19,E19		 ; E19 tmp11=(z11-z13)*C4>>8

	PADDW		E8,E11,E11		 ; E11 tmp12 = -(c2+c6) * z10 + z5
	PSUBW		E8,E9,E9	;E9  tmp10 = z12*(c2-c6) - z5
	PSUBW		E18,E11,E20	;E20 tmp6  = tmp12-tmp7
	PSUBW		E20,E19,E19	;E19 tmp5  = tmp11-tmp6
	PADDW		E19,E9,E9	;E9  tmp4  = tmp10+tmp5
		; now 4 butterflies and a following scaledown 
	BFLYW		E9,E17,E12:E13	;E12 tmp3+tmp4 E13 tmp3-tmp4
	BFLYW		E18,E16,E8:E9	;E8  tmp0+tmp7 E9  tmp0-tmp7
	BFLYW		E20,E14,E10:E11	;E10 tmp1+tmp6 E11 tmp1-tmp6
	BFLYW		E19,E15,E14:E15	;E14 tmp2+tmp5 E15 tmp2-tmp5
	endm

		;load inputs, first part into E8-E15
		;D0 D1 D2 D3 E0 E1 E2 E3
		BFLYW	D0,E0,E12	;E12 tmp10 =F(0)+F(4) E13 tmp11=F(0)-F(4)
		BFLYW	D2,E2,E14	;E14 tmp13 =F(2)+F(6) E15 tmp02=F(2)-F(6)
		BFLYW	D3,E1,E8	;E8  z11=F(1)+F(7)    E9  z12=F(1)-F(7)
		BFLYW	E3,D1,E10	;E10 z13=F(5)+F(3)    E11 z10=F(5)-F(3)

		;
		IDCTY		;in E8-15, out E8-15, trash E16-E23
		;

		;scale down (remove fraction)
		pmul88.w	#SCALEDOWN,E8,D0
		pmul88.w	#SCALEDOWN,E10,D1
		pmul88.w	#SCALEDOWN,E14,D2
		pmul88.w	#SCALEDOWN,E13,D3
		pmul88.w	#SCALEDOWN,E12,E0
		pmul88.w	#SCALEDOWN,E15,E1
		pmul88.w	#SCALEDOWN,E11,E2
		pmul88.w	#SCALEDOWN,E9,E3


		;load inputs, first part into E8-E15
		;D0 D1 D2 D3 E0 E1 E2 E3
		BFLYW	D4,E4,E12	;E12 tmp10 =F(0)+F(4) E13 tmp11=F(0)-F(4)
		BFLYW	D6,E6,E14	;E14 tmp13 =F(2)+F(6) E15 tmp02=F(2)-F(6)
		BFLYW	D7,E5,E8	;E8  z11=F(1)+F(7)    E9  z12=F(1)-F(7)
		BFLYW	E7,D5,E10	;E10 z13=F(5)+F(3)    E11 z10=F(5)-F(3)

		;
		IDCTY		;in E8-15, out E8-15, trash E16-E23
		;

		;scale down (remove fraction)
		pmul88.w	#SCALEDOWN,E8,D4
		pmul88.w	#SCALEDOWN,E10,D5
		pmul88.w	#SCALEDOWN,E14,D6
		pmul88.w	#SCALEDOWN,E13,D7
		pmul88.w	#SCALEDOWN,E12,E4
		pmul88.w	#SCALEDOWN,E15,E5
		pmul88.w	#SCALEDOWN,E11,E6
		pmul88.w	#SCALEDOWN,E9,E7


		;transpose again, back to natural
		TRANSHi		D0-D3,E8:E9	;A0 B0 C0 D0   A1 B1 C1 D1
		TRANSLo		D0-D3,E10:E11	;A2 B2 C2 D2   A3 B3 C3 D3
		TRANSHi		D4-D7,E12:E13	;A4 B4 C4 D4   A5 B5 C5 D5
		TRANSLo		D4-D7,E14:E15	;A6 B6 C6 D6   A7 B7 C7 D7

		TRANSHi		E0-E3,E16:E17	;E0 F0 G0 H0   E1 F1 G1 H1
		TRANSLo		E0-E3,E18:E19	;E2 F2 G2 H2   E3 F3 G3 H3
		TRANSHi		E4-E7,E20:E21	;E4 F4 G4 H4   E5 F5 G5 H5
		TRANSLo		E4-E7,E22:E23	;E6 F6 G6 H6   E7 F7 G7 H7

		; block is in natural order again:
		;
		; E8   E16
		; E9   E17
		;E10   E18
		;E11   E19
		;E12   E20
		;E13   E21
		;E14   E22
		;E15   E23
		
		;
		; + 128 (DC offset)
		; pack to 8 Bit and store results
		;
		paddw.w		#128,E8,E8
		move.l		(sp)+,d0	;retrieve line offset from stack

		paddw.w		#128,E16,E16
		move.l		(a2),A0		;first row ptr

		paddw.w		#128,E9,E9
		move.l		4(a2),A1	;2nd row ptr

		paddw.w		#128,E17,E17

		packuswb	E8,E16,(A0,D0.l) ;store to framebuffer

		paddw.w		#128,E10,E10
		move.l		8(A2),A0

		packuswb	E9,E17,(A1,D0.l)

		paddw.w		#128,E18,E18
		move.l		12(A2),A1

		packuswb	E10,E18,(a0,D0.l)

		paddw.w		#128,E11,E11
		move.l		16(a2),a0

		paddw.w		#128,E19,E19
.d2:		move.l		(sp)+,d2			;RESTORE D2

		packuswb	E11,E19,(A1,D0.l)

		paddw.w		#128,E12,E12
		move.l		20(A2),A1

		paddw.w		#128,E20,E20
.d3:		move.l		(sp)+,d3			;RESTORE D3

		packuswb	E12,E20,(a0,D0.l)

		paddw.w		#128,E13,E13
		move.l		24(a2),a0

		paddw.w		#128,E21,E21
.d4:		move.l		(sp)+,d4			;RESTORE D4

		packuswb	E13,E21,(a1,D0.l)

		paddw.w		#128,E14,E14
		move.l		28(a2),a1

		paddw.w		#128,E22,E22
.d5:		move.l		(sp)+,d5			;RESTORE D5

		packuswb	E14,E22,(A0,D0.l)

		paddw.w		#128,E15,E15
.d6:		move.l		(sp)+,d6			;RESTORE D6

		paddw.w		#128,E23,E23
.d7:		move.l		(sp)+,d7			;RESTORE D7

		packuswb	E15,E23,(a1,D0.l)

;	movem.l	(sp)+,d2-d7	;disabled, see RESTORE comment markers above
		rts

;
; simple case: only 1 coefficient present
;
; just dequantize and fill block
;
dconly:
; A0 - dequant coefficients (shorts)
; A1 - input array, organized 8x8 (shorts)
; A2 - output pointers, one per row
; A3 - cinfo (used only with OPT_MAXCOEF=1 or OPT_QUICK_DCONLY=1, otherwise ignored)
; D0 - output_col - offset to pointers at a2
	ifne	1
		move	(a1),d1			;get input coefficient
		muls.w	(a0),d1			;dequant
		asr.l	#5,d1			;remove fractional part
		vperm	#$67676767,d1,d1,e1	;distribute d1 across all 4 .w-slots of e1
		paddw.w	#128,e1,e1		;DC offset
		packuswb e1,e1,e1		;clip() to UInt8
	else
		xref	cliptab_off128
		move	(a1),d1			;get input coefficient
		lea	cliptab_off128,a1
		muls.w	(a0),d1			;dequant
		asr.l	#5,d1			;remove fractional part
		move.b	(a1,d1.w),d1		;clip()
		vperm	 #$77777777,d1,d1,e1	;distribute d1 across all 8 slots of e1
	endc
		move.l	a2,d1			;save A2 (or) move.l	a2,-(sp)
		move.l	(a2)+,a0
	rept	4
		move.l	(a2)+,a1
		store	e1,(a0,d0.l)
		move.l	(a2)+,a0
		store	e1,(a1,d0.l)
	endr
		move.l	d1,a2			;restore A2 (or) move.l (sp)+,a2 (or) lea -36(a2),a2
		rts




