/*
 * jsimd_ammx.c
 *
 * Copyright 2018 Henryk Richter <henryk.richter@gmx.net>
 *
 * based on jsimd_none.c, with the following copyrights:
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2009-2011, 2014, D. R. Commander.
 * Copyright (C) 2015, Matthieu Darbois.
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 * This file contains the Apollo Core AMMX checks and redirectors regarding
 * conditional 64 Bit AMMX SIMD usage.
 */
#if 1
#include <vampire/vampire.h>
#include <proto/vampire.h>
#include <proto/exec.h>
#include <exec/execbase.h>

/* defined in exec (differently), invalidate before starting with jpeglib stuff */
#undef GLOBAL 

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "../jsimd.h"
#include "jdct.h"
#include "jsimddct.h"

#include "jsimd_ammx.h"
#include "jmemset_ammx.h"

#ifdef UPSAMPLE_MERGING_MULTIROW
#define MERGED_EXTRA_ARG ,JDIMENSION nrows
#else
#define MERGED_EXTRA_ARG
#endif

struct Library *gVampireBase; /* we can overwrite this several times, no problem -> it's a resource */

/* check whether SIMD is available (1) or unavailable (0) */
LOCAL(int)
cinit_simd (void)
{
 struct Library *VampireBase = gVampireBase;

 /* init wasn't called ? */
 if( !VampireBase )
 	return 0;

 if( VRES_ERROR != V_EnableAMMX( V_AMMX_V2 ) )
 	return 1;
 else
	return 0;

#if 0
 tmp = OpenResource( "vampire.resource" );
 if( !tmp )
 	return	0;
 /* did we enable AMMX before ? */
 if( VampireBase == tmp )
 	return 1;
 VampireBase = tmp;
 /*if( !(VampireBase = OpenResource( "vampire.resource" ) ) )
 	return	0;*/

 if( VampireBase->lib_Version >= 45 )
 {
  res = V_EnableAMMX( V_AMMX_V2 );
  if( res != VRES_ERROR )
  {
  	AMMX_PrevState = res;
	/* disable at destroy() */
	/*if( res != VRES_AMMX_WAS_ON )
		V_EnableAMMX( V_AMMX_DISABLE );*/
   return 1;
  }
 }

 AMMX_PrevState = VRES_ERROR;
 VampireBase = (0); /* ignore VampireBase, if <V45 */
#endif

 return 0;
}

#ifndef LIBJPEG_SIMD_PRIVATE_STORAGE
#error "jsimd_ammx.c requires LIBJPEG_SIMD_PRIVATE_STORAGE defined"
#else
#if LIBJPEG_SIMD_PRIVATE_STORAGE < 2 
#error "jsimd_ammx.c requires LIBJPEG_SIMD_PRIVATE_STORAGE with at least 2"
#endif
#endif


LOCAL(int) ammx_init ( long privdata[] )
{
	struct Library *SysBase = *((struct Library **)(0x4));
	struct Library *VampireBase;
	int res;

	privdata[ AMMX_PRIV_ONOFF ] = 0;
	privdata[ AMMX_PRIV_VRES ] = (0);

	/* check whether we have Vampire.resource */
	VampireBase = OpenResource( "vampire.resource" );
	if( !VampireBase )
	{
		gVampireBase = (0); /* sorry, no SIMD */
		return 0;
	}

	if( VampireBase->lib_Version >= 45 )
	{
	  res = V_EnableAMMX( V_AMMX_V2 );
	  if( res != VRES_ERROR )
	  {
	  	/* set Marker only when we actually need to disable AMMX (i.e. it was off before) */
		if( res == VRES_OK )
			privdata[ AMMX_PRIV_ONOFF ] = AMMX_PRIV_ONOFF_MARKER | VRES_OK;

		jsimd_memset_switch_ammx( 1 );

		privdata[ AMMX_PRIV_VRES ]  = (long)VampireBase;
		gVampireBase = VampireBase;
		return 1;
	  }
	}

	gVampireBase = (0); /* sorry, no SIMD */
	return 0;
}

LOCAL(int) ammx_exit ( long privdata[] )
{
	long t=privdata[AMMX_PRIV_ONOFF];
	struct Library *VampireBase;

	if( ( t & 0xffff0000 ) != AMMX_PRIV_ONOFF_MARKER )
		return 0;
	t &= 0xffff;	/* keep lower 16 Bit only */
	
	/* previous init was unsuccessful ? */
	if( !gVampireBase )
		return 0;
	if( gVampireBase != (void*)privdata[AMMX_PRIV_VRES] )
		return 0;

	VampireBase = gVampireBase;

	/* shut down AMMX, if necessary */
	if( t == VRES_OK )
		V_EnableAMMX( V_AMMX_DISABLE );

	privdata[AMMX_PRIV_ONOFF] = 0;

	return 1;
}

GLOBAL(int) jdsimd_init( j_decompress_ptr cinfo )
{
	return ammx_init( cinfo->simd_private );
}

GLOBAL(int) jdsimd_exit( j_decompress_ptr cinfo )
{
	return ammx_exit( cinfo->simd_private );
}

GLOBAL(int) jcsimd_init( j_compress_ptr cinfo )
{
	return ammx_init( cinfo->simd_private );
}

GLOBAL(int) jcsimd_exit( j_compress_ptr cinfo )
{
	return ammx_exit( cinfo->simd_private );
}


GLOBAL(int)
jsimd_can_rgb_ycc (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_rgb_gray (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb (void)
{
  /* check constants */
  if (BITS_IN_JSAMPLE != 8)
  	return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
  	return 0;
  if (sizeof(JDIMENSION) != 4)
  	return 0;

  return cinit_simd();
//  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb565 (void)
{
  return 0;
}

GLOBAL(int)
jsimd_c_can_null_convert (void)
{
  return 0;
}

GLOBAL(void)
jsimd_rgb_ycc_convert (j_compress_ptr cinfo,
                       JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                       JDIMENSION output_row, int num_rows)
{
}

GLOBAL(void)
jsimd_rgb_gray_convert (j_compress_ptr cinfo,
                        JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                        JDIMENSION output_row, int num_rows)
{
}

GLOBAL(void)
jsimd_ycc_rgb_convert (j_decompress_ptr cinfo,
                       JSAMPIMAGE input_buf, JDIMENSION input_row,
                       JSAMPARRAY output_buf, int num_rows)
{
  AMMX_YCC_RGB_CONVERT_TYPE(ammxfct);

  ammxfct = jsimd_ycc_rgb_convert_ammx;

  if( cinfo->out_color_space != JCS_RGB )
  {
  	if( (cinfo->out_color_space != JCS_EXT_RGB) &&
	    (cinfo->out_color_space != JCS_EXT_BGR) /* FIXME: we are ignoring BGR right now and output RGB */
	  ) /* EXT_RGB == JCS_RGB with Amiga build, keep default routine */
	{
		/* FIXME: JCS_EXT_RGBX,JCS_EXT_XRGB,JCS_EXT_BGRX,JCS_EXT_XBGR all map to RGBx routine here */
/*  		if( (cinfo->out_color_space == JCS_EXT_RGBX) || (cinfo->out_color_space == JCS_EXT_RGBA) ) */
  			ammxfct = jsimd_ycc_rgbx_convert_ammx;
	}
  }

 ammxfct( cinfo->output_width, input_buf, output_buf, input_row, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb565_convert (j_decompress_ptr cinfo,
                          JSAMPIMAGE input_buf, JDIMENSION input_row,
                          JSAMPARRAY output_buf, int num_rows)
{
}

GLOBAL(void)
jsimd_c_null_convert (j_compress_ptr cinfo,
                      JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                      JDIMENSION output_row, int num_rows)
{
}

GLOBAL(int)
jsimd_can_h2v2_downsample (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_downsample (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_h2v2_smooth_downsample (void)
{
  return 0;
}

GLOBAL(void)
jsimd_h2v2_downsample (j_compress_ptr cinfo, jpeg_component_info *compptr,
                       JSAMPARRAY input_data, JSAMPARRAY output_data)
{
}

GLOBAL(void)
jsimd_h2v2_smooth_downsample (j_compress_ptr cinfo,
                              jpeg_component_info *compptr,
                              JSAMPARRAY input_data, JSAMPARRAY output_data)
{
}

GLOBAL(void)
jsimd_h2v1_downsample (j_compress_ptr cinfo, jpeg_component_info *compptr,
                       JSAMPARRAY input_data, JSAMPARRAY output_data)
{
}

GLOBAL(int)
jsimd_can_h2v2_upsample (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_upsample (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_int_upsample (void)
{
  return 0;
}

GLOBAL(void)
jsimd_int_upsample (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                      JSAMPARRAY input_data, JSAMPARRAY *output_data_ptr)
{
}

GLOBAL(void)
jsimd_h2v2_upsample (j_decompress_ptr cinfo,
                     jpeg_component_info *compptr,
                     JSAMPARRAY input_data,
                     JSAMPARRAY *output_data_ptr)
{
}

GLOBAL(void)
jsimd_h2v1_upsample (j_decompress_ptr cinfo,
                     jpeg_component_info *compptr,
                     JSAMPARRAY input_data,
                     JSAMPARRAY *output_data_ptr)
{
}

GLOBAL(int)
jsimd_can_h2v2_fancy_upsample (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_fancy_upsample (void)
{
  return 0;
}

GLOBAL(void)
jsimd_h2v2_fancy_upsample (j_decompress_ptr cinfo,
                           jpeg_component_info *compptr,
                           JSAMPARRAY input_data,
                           JSAMPARRAY *output_data_ptr)
{
}

GLOBAL(void)
jsimd_h2v1_fancy_upsample (j_decompress_ptr cinfo,
                           jpeg_component_info *compptr,
                           JSAMPARRAY input_data,
                           JSAMPARRAY *output_data_ptr)
{
}

GLOBAL(int)
jsimd_can_h2v2_merged_upsample (void)
{
#if 0
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
#endif
  /* pass SIMD check through (1/0) */
  return cinit_simd();
  //return 0;
}

GLOBAL(int)
jsimd_can_h2v1_merged_upsample (void)
{
#if 0
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
#endif
  /* pass SIMD check through (1/0) */
  return cinit_simd();
  //return 0;
}

GLOBAL(void)
jsimd_h2v2_merged_upsample (j_decompress_ptr cinfo,
                            JSAMPIMAGE input_buf,
                            JDIMENSION in_row_group_ctr,
                            JSAMPARRAY output_buf
			    MERGED_EXTRA_ARG
			    )
{
  AMMX_YCC_RGB_CONVERT_TYPE(ammxfct);

  ammxfct = jsimd_h2v2_merged_upsample_ammx;

  if( cinfo->out_color_space != JCS_RGB )
  {
  	if( (cinfo->out_color_space != JCS_EXT_RGB) &&
	    (cinfo->out_color_space != JCS_EXT_BGR) /* FIXME: we are ignoring BGR right now and output RGB */
	  ) /* EXT_RGB == JCS_RGB with Amiga build, keep default routine */
	{
		/* FIXME: JCS_EXT_RGBX,JCS_EXT_XRGB,JCS_EXT_BGRX,JCS_EXT_XBGR all map to RGBx routine here */
/*  		if( (cinfo->out_color_space == JCS_EXT_RGBX) || (cinfo->out_color_space == JCS_EXT_RGBA) ) */
  			ammxfct = jsimd_h2v2_merged_upsample_rgb32_ammx;
	}
  }

 /* the AMMX routine could do multiple rows, jpeglib does only one *sniff* */
 ammxfct( cinfo->output_width, input_buf, output_buf, in_row_group_ctr, 1);
}


GLOBAL(void)
jsimd_h2v1_merged_upsample (j_decompress_ptr cinfo,
                            JSAMPIMAGE input_buf,
                            JDIMENSION in_row_group_ctr,
                            JSAMPARRAY output_buf
			    MERGED_EXTRA_ARG)
{
  AMMX_YCC_RGB_CONVERT_TYPE(ammxfct);

  ammxfct = jsimd_h2v1_merged_upsample_ammx;

  if( cinfo->out_color_space != JCS_RGB )
  {
  	if( (cinfo->out_color_space != JCS_EXT_RGB) &&
	    (cinfo->out_color_space != JCS_EXT_BGR) /* FIXME: we are ignoring BGR right now and output RGB */
	  ) /* EXT_RGB == JCS_RGB with Amiga build, keep default routine */
	{
		/* FIXME: JCS_EXT_RGBX,JCS_EXT_XRGB,JCS_EXT_BGRX,JCS_EXT_XBGR all map to RGBx routine here */
/*  		if( (cinfo->out_color_space == JCS_EXT_RGBX) || (cinfo->out_color_space == JCS_EXT_RGBA) ) */
  			ammxfct = jsimd_h2v1_merged_upsample_rgb32_ammx;
	}
  }

 /* the AMMX routine could do multiple rows, jpeglib does only one *sniff* */
 ammxfct( cinfo->output_width, input_buf, output_buf, in_row_group_ctr, 1);
}

GLOBAL(int)
jsimd_can_convsamp (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_convsamp_float (void)
{
  return 0;
}

GLOBAL(void)
jsimd_convsamp (JSAMPARRAY sample_data, JDIMENSION start_col,
                DCTELEM *workspace)
{
}

GLOBAL(void)
jsimd_convsamp_float (JSAMPARRAY sample_data, JDIMENSION start_col,
                      FAST_FLOAT *workspace)
{
}

GLOBAL(int)
jsimd_can_fdct_islow (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_fdct_ifast (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_fdct_float (void)
{
  return 0;
}

GLOBAL(void)
jsimd_fdct_islow (DCTELEM *data)
{
}

GLOBAL(void)
jsimd_fdct_ifast (DCTELEM *data)
{
}

GLOBAL(void)
jsimd_fdct_float (FAST_FLOAT *data)
{
}

GLOBAL(int)
jsimd_can_quantize (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_quantize_float (void)
{
  return 0;
}

GLOBAL(void)
jsimd_quantize (JCOEFPTR coef_block, DCTELEM *divisors,
                DCTELEM *workspace)
{
}

GLOBAL(void)
jsimd_quantize_float (JCOEFPTR coef_block, FAST_FLOAT *divisors,
                      FAST_FLOAT *workspace)
{
}

GLOBAL(int)
jsimd_can_idct_2x2 (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_idct_4x4 (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_idct_6x6 (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_idct_12x12 (void)
{
  return 0;
}

GLOBAL(void)
jsimd_idct_2x2 (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
}

GLOBAL(void)
jsimd_idct_4x4 (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
}

GLOBAL(void)
jsimd_idct_6x6 (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
}

GLOBAL(void)
jsimd_idct_12x12 (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
}

GLOBAL(int)
jsimd_can_idct_islow (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_idct_ifast (void)
{
  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (IFAST_SCALE_BITS != 2)
    return 0;
  if (DCTSIZE != 8)
    return 0;
 
  /* pass SIMD check through (1/0) */
  return cinit_simd();
}

/* iDCT FLOAT is only available when ExecBase has the FPU flags */
GLOBAL(int)
jsimd_can_idct_float (void)
{
  struct ExecBase *SysBase = *((struct ExecBase **)(0x4));

  if( SysBase->AttnFlags & AFF_68881 )
   return 1;

  return 0;
}

GLOBAL(void)
jsimd_idct_islow (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
}

GLOBAL(void)
jsimd_idct_ifast (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
 jsimd_idct_ifast_ammx( compptr->dct_table, coef_block, output_buf, output_col /*, cinfo*/);

}

#if 0
/* see jidctflt_68k.c */
GLOBAL(void)
jsimd_idct_float (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
}
#endif

GLOBAL(int)
jsimd_can_huff_encode_one_block (void)
{
  return 0;
}

GLOBAL(JOCTET*)
jsimd_huff_encode_one_block (void *state, JOCTET *buffer, JCOEFPTR block,
                             int last_dc_val, c_derived_tbl *dctbl,
                             c_derived_tbl *actbl)
{
  return NULL;
}
#endif