#ifndef MUI_EXTRASRC_H
#define MUI_EXTRASRC_H

/*
 * Copyright 2009 Ilkka Lehtoranta <ilkleht@isoveli.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__MORPHOS__)

#include <libraries/mui.h>
#ifdef TTENGINE
#include <libraries/ttengine.h>
#endif
#include "amiga/os3support.h"
#ifndef MUI_EHF_GUIMODE
#define MUI_EHF_GUIMODE (1 << 1)
#endif

#define RAWFMTFUNC_STRING 0
#define POINTERTYPE_NORMAL 0
#define POINTERTYPE_SELECTLINK 0
#define POINTERTYPE_AIMING 0
#define POINTERTYPE_MOVE 0
#define POINTERTYPE_BUSY 0
#define POINTERTYPE_HELP 0
#define POINTERTYPE_NOTAVAILABLE 0
#define POINTERTYPE_WORKING 0
#define POINTERTYPE_INVISIBLE 0


#ifdef TTENGINE
#ifndef TT_Encoding_System_UTF8
#define TT_Encoding_System_UTF8 TT_Encoding_UTF8
#endif
#endif

#ifndef WCHAR_TYPEDEF
#define WCHAR_TYPEDEF
typedef int             WCHAR;
#endif

#if !defined(__AROS__)
/* Popular AROS types */
#ifndef QUAD_TYPEDEF
#define QUAD_TYPEDEF
typedef signed long long   QUAD;
#endif

#ifndef UQUAD_TYPEDEF
#define UQUAD_TYPEDEF
typedef unsigned long long UQUAD;
#endif

typedef unsigned long  IPTR;
#endif

VOID NewRawDoFmt(CONST_STRPTR format, APTR func, STRPTR buf, ...);
#define AllocMemAligned(size, type, align, offset) AllocMem(size, type)
#define AllocVecAligned(size, type, align, offset) AllocVec(size, type)
#define AllocTaskPooled(size)     malloc(size)
#define FreeTaskPooled(ptr, size) free(ptr)
#define AllocVecTaskPooled(size)  malloc(size)
#define FreeVecTaskPooled(ptr)    free(ptr)
ULONG UTF8_Decode(CONST_STRPTR s, WCHAR *uchar);

#endif

#endif /* MUI_EXTRASRC_H */

