/*
 * jidctflt_init.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * Modified 2002-2010 by Guido Vollbeding.
 * libjpeg-turbo Modifications:
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2010, 2015, D. R. Commander.
 * Copyright (C) 2013, MIPS Technologies, Inc., California.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 * Copyright (C) 2018 Henryk Richter
 *
 * Here the floating point iDCT initialization is performed
 * for the AmigaOS-68k target. This part of the code has been
 * moved here to be able compiling the rest of the code without
 * FPU requirement.
 *
*/

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jdct.h"               /* Private declarations for DCT subsystem */
#include "jsimddct.h"
#include "jpegcomp.h"

#ifdef DCT_FLOAT_SUPPORTED

#define _0_125 ((FLOAT_MULT_TYPE)0.125)

GLOBAL(void)
jidctflt_initquant( jpeg_component_info *compptr, JQUANT_TBL *qtbl )
{
        /* For float AA&N IDCT method, multipliers are equal to quantization
         * coefficients scaled by scalefactor[row]*scalefactor[col], where
         *   scalefactor[0] = 1
         *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
         *
         * Please note that the scaledown factor 1/8 will be applied directly
	 * in here to the table, instead of a multiply in the main loop.
         */
   FLOAT_MULT_TYPE *fmtbl = (FLOAT_MULT_TYPE *) compptr->dct_table;
   int row, col, i;
   static const double aanscalefactor[DCTSIZE] = {
         1.0, 1.387039845, 1.306562965, 1.175875602,
         1.0, 0.785694958, 0.541196100, 0.275899379 };

   i = 0;
   for (row = 0; row < DCTSIZE; row++)
   {
      for (col = 0; col < DCTSIZE; col++)
      {
         fmtbl[i] = (FLOAT_MULT_TYPE)
                    ((double) qtbl->quantval[i] *
                    aanscalefactor[row] * aanscalefactor[col] * _0_125 );
         i++;
      }
   }
}

#endif



