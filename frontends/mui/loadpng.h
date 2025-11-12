#ifndef __LOADPNG_H__
#define __LOADPNG_H__

/* $Id: loadpng.h,v 1.1 2008/06/27 22:01:48 itix Exp $ */

struct nsimage
{
	ULONG width, height, modulo;
	ULONG data[0];
};

APTR png_load(CONST_STRPTR filename);

#endif /* __LOADPNG_H__ */
