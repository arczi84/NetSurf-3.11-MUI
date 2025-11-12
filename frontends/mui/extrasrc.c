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
#include <stdio.h>
#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>
#include <stdarg.h>

#include "os3.h"


Object *DoSuperNew(struct IClass *cl, Object *obj, Tag tag1, ...)
{
    va_list args;
    struct TagItem *tags;
    Object *res;

    va_start(args, tag1);
    tags = (struct TagItem *)AllocVec(sizeof(struct TagItem) * 64, MEMF_ANY); // max 32 tagów
    if (!tags)
    {
        va_end(args);
        return NULL;
    }

    int i = 0;
    tags[i].ti_Tag = tag1;
    tags[i].ti_Data = va_arg(args, ULONG);
    i++;

    Tag tag;
    while ((tag = va_arg(args, Tag)) != TAG_END && i < 63)
    {
        tags[i].ti_Tag = tag;
        tags[i].ti_Data = va_arg(args, ULONG);
        i++;
    }

    tags[i].ti_Tag = TAG_END;

    va_end(args);

    res = (Object *)DoSuperMethod(cl, obj, OM_NEW, tags);

    FreeVec(tags);
    return res;
}
APTR NewObject(struct IClass * classPtr, CONST_STRPTR classID, ULONG tagList, ...)
{
	return NewObjectA(classPtr, classID, (const struct TagItem *) &tagList);
}
VOID NewRawDoFmt_org(CONST_STRPTR format, APTR func, STRPTR buf, ...)
{
	if (func == NULL)
	{
		va_list va;

		va_start(va, buf);
		vsprintf(buf, format, va);
		va_end(va);
	}
}
VOID NewRawDoFmt(CONST_STRPTR format, APTR func, STRPTR buf, ...)
{
    if (func == NULL)
    {
        va_list va;

        va_start(va, buf);
        vsnprintf(buf, 255, format, va);  // zabezpiecz przed przepełnieniem
        va_end(va);
    }
    else
    {
		printf("NewRawDoFmt: func != NULL, format='%s'\n", format);
        // dodaj obsługę własnej funkcji (hook) jeśli potrzebujesz:
        // w przeciwnym razie ustaw assert lub warning.
    }
}
STATIC LONG do_alpha(LONG a, LONG v) 
{
	LONG tmp  = (a*v);
	return ((tmp<<8) + tmp + 32768)>>16;
}

VOID WritePixelArrayAlpha(APTR src, UWORD srcx, UWORD srcy, UWORD srcmod, struct RastPort *rp, UWORD destx, UWORD desty, UWORD width, UWORD height, ULONG globalalpha)
{
	if (width > 0 && height > 0)
	{
		ULONG *buf = AllocMem(width * sizeof(ULONG), MEMF_ANY);

		if (buf)
		{
			ULONG x, y;

			/* Incorrect but cant bother with alpha channel math for now */
			globalalpha = 255 - (globalalpha >> 24);

			for (y = 0; y < height; y++)
			{
				ULONG *pix;

				ReadPixelArray(buf, 0, 0, width * sizeof(ULONG), rp, destx, desty + y, width, 1, RECTFMT_ARGB);

				pix = (ULONG *)((ULONG)src + (srcy + y) * srcmod + srcx * sizeof(ULONG));

				for (x = 0; x < width; x++)
				{
					ULONG srcpix, dstpix, a, r, g, b;

					srcpix = *pix++;
					dstpix = buf[x];

					a = (srcpix >> 24) & 0xff;
					r = (srcpix >> 16) & 0xff;
					g = (srcpix >> 8) & 0xff;
					b = (srcpix >> 0) & 0xff;

					a = a - globalalpha;  

					if (a > 0)
					{
						ULONG dest_r, dest_g, dest_b;

						dest_r = (dstpix >> 16) & 0xff;
						dest_g = (dstpix >> 8) & 0xff;
						dest_b = (dstpix >> 0) & 0xff;

						dest_r += do_alpha(a, (LONG)r - (LONG)dest_r);
						dest_g += do_alpha(a, (LONG)g - (LONG)dest_g);
						dest_b += do_alpha(a, (LONG)b - (LONG)dest_b);

						if (dest_r > 255) dest_r = 255;
						if (dest_g > 255) dest_g = 255;
						if (dest_b > 255) dest_b = 255;

						dstpix = (dstpix & 0xff000000) |
								 (dest_r << 16) |
								 (dest_g << 8) |
								 dest_b;
					}

					buf[x] = dstpix;
				}

				WritePixelArray(buf, 0, 0, width * sizeof(ULONG), rp, destx, desty + y, width, 1, RECTFMT_ARGB);
			}

			FreeMem(buf, width * sizeof(ULONG));
		}
	}
}      

ULONG UTF8_Decode(CONST_STRPTR s, WCHAR *uchar)
{
	ULONG octets=0;
	WCHAR ucs4=0;
	CONST UBYTE *sb = (UBYTE *)s;

	if (*sb == 0)
	{
		ucs4 = 0;
		octets = 0;
	}
	else if (*sb <= 127)
	{
		ucs4 = *sb++;
		octets = 1;
	}
	else if (*sb <= 223)
	{
		ucs4 = (*sb++ - 192)*64;
		ucs4 += (*sb++ - 128);
		octets = 2;
	}
	else if (*sb <= 239)
	{
		ucs4 = (*sb++ - 192)*4096;
		ucs4 += (*sb++ - 128)*64;
		ucs4 += (*sb++ - 128);
		octets = 3;
	}
	else if (*sb <= 247)
	{
		ucs4 = (*sb++ - 192)*262144;
		ucs4 += (*sb++ - 128)*4096;
		ucs4 += (*sb++ - 128)*64;
		ucs4 += (*sb++ - 128);
		octets = 4;
	}

	if (uchar)
		*uchar = ucs4;

	return octets;
}


#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>

struct Node *GetHead(struct List *l) {
    return l->lh_Head->ln_Succ ? l->lh_Head : (struct Node *)0;
}

struct Node *GetTail(struct List *l) {
    return l->lh_TailPred->ln_Pred ? l->lh_TailPred : (struct Node *)0;
}

struct Node *GetSucc(struct Node *n) {
    return n->ln_Succ->ln_Succ ? n->ln_Succ : (struct Node *)0;
}

struct Node *GetPred(struct Node *n) {
    return n->ln_Pred->ln_Pred ? n->ln_Pred : (struct Node *)0;
}
/*
ULONG ListLength(struct List *list) {
    ULONG count = 0;
    struct Node *n;
    ForeachNode(list, n) {
        count++;
    }
    return count;
}
*/
