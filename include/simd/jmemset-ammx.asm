; jdmemset-ammx.asm - memset and memcpy replacements
;
; Copyright 2018 Henryk Richter <henryk.richter@gmx.net>
;
; This file should be assembled with VASM.
; VASM is available from http://sun.hasenbraten.de/vasm/
;
;
;
	machine	ac68080

	section	".text",code

	nop

	xdef	_jsimd_bzero_ammx
	xdef	_jsimd_memcpy_ammx
	xdef	_jsimd_memset_switch_ammx


;------------------------------------------------------------------------------
; in: D0 = switch AMMX Memset/Memcpy on (1) or off (0)
;
; close your eyes, this is self-modifying code (you may scream now...)
_jsimd_memset_switch_ammx:

	move	#jsimd_clear_68k-clearbranch-2,d1
	tst.w	d0
	beq.s	.noena1
	move	#jsimd_clear_ammx-clearbranch-2,d1
.noena1:
	move	d1,clearbranch+2

	move	#jsimd_copy_68k-copybranch-2,d1
	tst.w	d0
	beq.s	.noena2
	moveq	#jsimd_copy_ammx-copybranch-2,d1
.noena2:
	move	d1,copybranch+2


	rts


;------------------------------------------------------------------------------
; in: A0 = pointer
;     D0 = number of bytes to clear
_jsimd_bzero_ammx:

clearbranch:
	bra.w	jsimd_clear_68k

;---- AMMX version ---------
jsimd_clear_ammx:

	peor	d1,d1,d1
.loop:
	storec	d1,d0,(a0)+
	subq.l	#8,d0
	bgt.s	.loop
	
	rts
;---- 68k version ----------
jsimd_clear_68k:
	move.l	a0,d1		;
	and.w	#3,d1		;alignment offset
	suba.l	a1,a1

	cmp.l	#7,d0		;this is the minimum for the faster loop
	blt.s	.byteclear

	; align to long word
	jmp	.aln_offs(pc,d1.w*2)
.aln_offs:
	bra.s	.main_clr	;.aln0
	bra.s	.aln1
	bra.s	.aln2
	bra.s	.aln3
.aln1:
	sf	(a0)+
	move.w	a1,(a0)+
	subq.l	#3,d0
	bra.s	.main_clr
.aln2:
	move.w	a1,(a0)+
	subq.l	#2,d0
	bra.s	.main_clr
.aln3:
	sf	(a0)+
	subq.l	#1,d0
	;
	;A0 is aligned to multiples of 4
	;
.main_clr:			;we ensured that at least 7 bytes are cleared
	moveq	#32,d1
	sub.l	d1,d0
	blt.s	.endlongclear
.longclear:
	rept	8
	move.l	a1,(a0)+
	endr
	sub.l	d1,d0
	bge.s	.longclear
.endlongclear
	add.l	d1,d0
	beq.s	.end

	subq.l	#4,d0
	blt.s	.end32bitclear
.main_clr_loop:
	move.l	a1,(a0)+
	subq.l	#4,d0
	bgt.s	.main_clr_loop
.end32bitclear:
	addq.l	#4,d0
.byteclear
        ble.s   .end
	sf	(a0)+
	subq.l  #1,d0
	bra.s	.byteclear
.end:
	rts


;------------------------------------------------------------------------------
; in: A1 = Out Ptr
;     A0 = In Ptr
;     D0 = number of bytes to copy
_jsimd_memcpy_ammx:

copybranch:
	bra.w	jsimd_copy_68k

;---- AMMX version ---------
jsimd_copy_ammx:

.loop:
	load	(a0)+,d1
	storec	d1,d0,(a1)+
	subq.l	#8,d0
	bgt.s	.loop
	
	rts
;---- 68k version ----------
jsimd_copy_68k:

	move.l	d0,d1
	and.w	#3,d0

	lsr.l	#2,d1
	beq.s	.bytecopy

	bra.s	.wordcopy
.wordloop:
	move.l	(a0)+,(a1)+
.wordcopy:
	dbf	d1,.wordloop
	bra.s	.bytecopy

.byteloop
	move.b	(a0)+,(a1)+
.bytecopy:
	dbf	d0,.byteloop

	rts



