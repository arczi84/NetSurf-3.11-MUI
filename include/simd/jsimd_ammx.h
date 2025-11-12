/*
 * jsimd_ammx.h
 *
 * Copyright 2018 Henryk Richter <henryk.richter@gmx.net>
 *
 */

#ifndef _JSIMD_AMMX_H
#define _JSIMD_AMMX_H

/*-------------- STARTUP/SHUTDOWN ROUTINES -------------------------*/

#ifdef LIBJPEG_SIMD_PRIVATE_STORAGE
//GLOBAL(int) jdsimd_init( j_decompress_ptr cinfo );
//GLOBAL(int) jdsimd_exit( j_decompress_ptr cinfo );
#endif

#define AMMX_PRIV_VRES	0
#define AMMX_PRIV_ONOFF	1
#define AMMX_PRIV_ONOFF_MARKER 0xAFFE0000

#if 0
/* see jmemset_ammx.h */
/*--------------------- MEMCPY/BZERO -------------------------------*/

void jsimd_memset_switch_ammx( register int ammxon  __asm("d0") );
void jsimd_bzero_ammx( register void *ptr  __asm("a0"),
                       register long bytes __asm("d0") );
void jsimd_memcpy_ammx( register void *dest __asm("a1"),
                        register void *src  __asm("a0"),
                        register long bytes __asm("d0") );
#endif

/*---------------------- DCT ROUTINES ------------------------------*/

/* scaled AAN integer - fast but less accurate */
void jsimd_idct_ifast_ammx( register short *dequanttab __asm("a0"),
                            register short *coefblock  __asm("a1"),
			    register JSAMPARRAY optr   __asm("a2"),
			    register int   output_col  __asm("d0")
			    /* ,register j_decompress_ptr cinfo __asm("a3")*/ ); /* see jidctfst-ammx.asm (OPT_MAXCOEF) */ 

/* float iDCT init */
GLOBAL(void)
jidctflt_initquant( jpeg_component_info *compptr, JQUANT_TBL *qtbl );
/* float iDCT */


/*----------------- Color Conversion Routines ----------------------*/

#define AMMX_YCC_RGB_CONVERT_TYPE(_a_) void (*_a_)(register int __asm("d2"),\
                                                   register JSAMPIMAGE __asm("a2"),\
                                                   register JSAMPARRAY __asm("a3"),\
                                                   register int __asm("d1"),\
                                                   register int __asm("d0"));

/* 4:4:4 YCbCr to RGB24 */
void jsimd_ycc_rgb_convert_ammx( register int img_width __asm("d2"),
			    register JSAMPIMAGE iptr    __asm("a2"),
                            register JSAMPARRAY optr    __asm("a3"),
			    register int img_row        __asm("d1"),
			    register int img_nrows      __asm("d0") );

/* 4:2:2 YCbCr to RGB24 (multi-row support) */
void jsimd_h2v1_merged_upsample_ammx( register int img_width __asm("d2"),
                            register JSAMPIMAGE iptr    __asm("a2"),
                            register JSAMPARRAY optr    __asm("a3"),
                            register int img_row        __asm("d1"),
                            register int img_nrows      __asm("d0") );

/* 4:2:0 YCbCr to RGB24 */
void jsimd_h2v2_merged_upsample_ammx( register int img_width __asm("d2"),
                            register JSAMPIMAGE iptr    __asm("a2"),
                            register JSAMPARRAY optr    __asm("a3"),
                            register int img_row        __asm("d1"),
                            register int img_nrows      __asm("d0") );


/* 4:4:4 YCbCr to RGB32 RGBx */
void jsimd_ycc_rgbx_convert_ammx( register int img_width __asm("d2"),
			    register JSAMPIMAGE iptr    __asm("a2"),
                            register JSAMPARRAY optr    __asm("a3"),
			    register int img_row        __asm("d1"),
			    register int img_nrows      __asm("d0") );

/* 4:2:2 YCbCr to RGB32 (multi-row support) */
void jsimd_h2v1_merged_upsample_rgb32_ammx( register int img_width __asm("d2"),
                            register JSAMPIMAGE iptr    __asm("a2"),
                            register JSAMPARRAY optr    __asm("a3"),
                            register int img_row        __asm("d1"),
                            register int img_nrows      __asm("d0") );

/* 4:2:0 YCbCr to RGB32 */
void jsimd_h2v2_merged_upsample_rgb32_ammx( register int img_width __asm("d2"),
                            register JSAMPIMAGE iptr    __asm("a2"),
                            register JSAMPARRAY optr    __asm("a3"),
                            register int img_row        __asm("d1"),
                            register int img_nrows      __asm("d0") );


/*----------------- Scaling Routines -------------------------------*/



#endif /* _JSIMD_AMMX_H */
