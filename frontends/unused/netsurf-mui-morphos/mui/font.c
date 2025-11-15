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

#include <string.h>

#include <graphics/rpattr.h>
#include <hardware/atomic.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/ttengine.h>

#include "css/css.h"
#include "macros/vapor.h"
#include "mui/font.h"
#include "mui/plotters.h"
#include "mui/utils.h"
#include "render/font.h"
#include "utils/utf8.h"

struct fontnode
{
	struct MinNode node;
	ULONG  last_access;
	ULONG  ysize;
	UWORD  weight;
	UWORD  style;
	APTR   familytable;
	APTR   tfont;
	ULONG VOLATILE usecount;
};

STATIC struct SignalSemaphore semaphore;
STATIC struct RastPort fontrp;
STATIC struct MinList fontcache =
{
	(APTR)&fontcache.mlh_Tail, NULL, (APTR)&fontcache
};

void font_init(void)
{
	InitSemaphore(&semaphore);
	#if defined(__MORPHOS__)
	SetRPAttrs(&fontrp, RPTAG_PenMode, FALSE, TAG_DONE);
	#endif
}

void font_cleanup(void)
{
	struct fontnode *next, *node;

	ITERATELISTSAFE(node, next, &fontcache) {
		TT_CloseFont(node->tfont);
		FreeMem(node, sizeof(*node));
	}

	TT_DoneRastPort(&fontrp);
}

void font_cache_check(void)
{
	struct fontnode *next, *node;
	ULONG secs, dummy, time;

	CurrentTime(&secs, &dummy);

	time = secs - 180;	// last access was three minutes ago

	ObtainSemaphore(&semaphore);
	ITERATELISTSAFE(node, next, &fontcache) {
		if (node->usecount == 0 && node->last_access < time) {
			REMOVE(node);
			TT_CloseFont(node->tfont);
			FreeMem(node, sizeof(*node));
		}
	}
	ReleaseSemaphore(&semaphore);
}

void mui_close_font(struct RastPort *rp, APTR tfont)
{
	struct fontnode *node = tfont;
	ULONG dummy;

	CurrentTime(&node->last_access, &dummy);
	ATOMIC_SUB((ULONG *)&node->usecount, 1);
}

STATIC CONST CONST_STRPTR fsans_serif[] =
{
	"Times New Roman", "sans", NULL
};
STATIC CONST CONST_STRPTR fserif[] =
{
	"Comic Sans MS", "sans", NULL
};
STATIC CONST CONST_STRPTR fmonospaced[] =
{
	"Courier New", "sans", NULL
};
STATIC CONST CONST_STRPTR fcursive[] =
{
	"Courier New", "sans", NULL
};
STATIC CONST CONST_STRPTR ffantasy[] =
{
	"Verdana", "sans", NULL
};
STATIC CONST CONST_STRPTR fdefault[] =
{
	"Times New Roman", NULL
};

APTR mui_open_font(struct RastPort *rp, const struct css_style *style)
{
	CONST CONST_STRPTR *table;
	struct fontnode *node;
	ULONG ysize, weight, fontstyle;
	APTR tfont;

	switch(style->font_family) {
	case CSS_FONT_FAMILY_SANS_SERIF:
		table = fsans_serif;
		break;

	case CSS_FONT_FAMILY_SERIF:
		table = fserif;
		break;

	case CSS_FONT_FAMILY_MONOSPACE:
		table = fmonospaced;
		break;

	case CSS_FONT_FAMILY_CURSIVE:
		table = fcursive;
		break;

	case CSS_FONT_FAMILY_FANTASY:
		table = ffantasy;
		break;

	default:
		table = fdefault;
		break;
	}

	fontstyle = TT_FontStyle_Regular;

	switch(style->font_style) {
	case CSS_FONT_STYLE_ITALIC:
	case CSS_FONT_STYLE_OBLIQUE:
		fontstyle = TT_FontStyle_Italic;
		break;
	}

	weight = TT_FontWeight_Normal;

	switch(style->font_weight) {
	case CSS_FONT_WEIGHT_BOLD   : weight = TT_FontWeight_Bold; break;
	case CSS_FONT_WEIGHT_BOLDER : weight = TT_FontWeight_Bold + 100; break;
	case CSS_FONT_WEIGHT_LIGHTER: weight = TT_FontWeight_Normal - 100; break;
	case CSS_FONT_WEIGHT_100: weight = 100; break;
	case CSS_FONT_WEIGHT_200: weight = 200; break;
	case CSS_FONT_WEIGHT_300: weight = 300; break;
	case CSS_FONT_WEIGHT_400: weight = 400; break;
	case CSS_FONT_WEIGHT_500: weight = 500; break;
	case CSS_FONT_WEIGHT_600: weight = 600; break;
	case CSS_FONT_WEIGHT_700: weight = 700; break;
	case CSS_FONT_WEIGHT_800: weight = 800; break;
	case CSS_FONT_WEIGHT_900: weight = 900; break;
	}

#if 0
	switch(style->font_variant)
	{
		default:
			break;
	}
#endif

	ysize = css_len2px(&style->font_size.value.length, style);

//	if (ysize < option_font_min_size)
//		ysize = option_font_min_size;

	tfont = NULL;

	ObtainSemaphore(&semaphore);
	ITERATELIST(node, &fontcache) {
		if (node->ysize == ysize && node->weight == weight && node->style == fontstyle && node->familytable == table) {
			ATOMIC_ADD((ULONG *)&node->usecount, 1);
			tfont = node->tfont;
			break;
		}
	}
	ReleaseSemaphore(&semaphore);

	if (!tfont) {
		node = AllocMem(sizeof(*node), MEMF_ANY);

		if (node) {
			tfont = TT_OpenFont(
					TT_FamilyTable, table,
					TT_FontSize, ysize,
					TT_FontWeight, weight,
				TAG_DONE);

			if (tfont) {
				node->ysize = ysize;
				node->weight = weight;
				node->style = fontstyle;
				node->familytable = (APTR)table;
				node->tfont = tfont;
				node->usecount = 1;

				ObtainSemaphore(&semaphore);
				ADDTAIL(&fontcache, node);
				ReleaseSemaphore(&semaphore);
			} else {
				FreeMem(node, sizeof(*node));
				node = NULL;
				tfont = NULL;
			}
		}
	}

	if (tfont) {
		/* TTEngine V8 required */
		if (TTEngineBase->lib_Version >= 8)
		{
			TT_SetAttrs(rp,
					TT_Antialias, TT_Antialias_On,
					TT_Encoding, TT_Encoding_System_UTF8,
					TT_ColorMap, 1,
				TAG_DONE);
		}
		else
		{
			TT_SetAttrs(rp,
					TT_Antialias, TT_Antialias_On,
					TT_Encoding, TT_Encoding_UTF8,
					//TT_Window, _window(obj),	// fixme
				TAG_DONE);
		}

		TT_SetFont(rp, tfont);
	}

	return node;
}

/**
 * Measure the width of a string.
 *
 * \param  style   css_style for this text, with style->font_size.size ==
 *                 CSS_FONT_SIZE_LENGTH
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  width   updated to width of string[0..length)
 * \return  true on success, false on error and error reported
 */

static bool nsfont_width(const struct css_style *style,
		const char *string, size_t length,
		int *width)
{
	APTR tfont;
	int w;

	tfont = mui_open_font(&fontrp, style);
	w = 0;

	if (tfont) {
		length = utf8_codepoints(string, length);
		w = TT_TextLength(&fontrp, (STRPTR)string, length);
		mui_close_font(&fontrp, tfont);
	}

	*width = w;

	return true;
}

/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param  style        css_style for this text, with style->font_size.size ==
 *                      CSS_FONT_SIZE_LENGTH
 * \param  string       UTF-8 string to measure
 * \param  length       length of string
 * \param  x            x coordinate to search for
 * \param  char_offset  updated to offset in string of actual_x, [0..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

static bool nsfont_position_in_string(const struct css_style *style, const char *string, size_t length, int x, size_t *char_offset, int *actual_x)
{
	int off, act_x;
	APTR tfont;

	tfont = mui_open_font(&fontrp, style);
	off = 0;
	act_x = 0;

	if (tfont) {
		struct TextExtent extent;

		length = utf8_codepoints(string, length);
		off = TT_TextFit(&fontrp, (STRPTR)string, length, &extent, NULL, 1, x, 32767);
		act_x = extent.te_Extent.MaxX;

		mui_close_font(&fontrp, tfont);
	}

	*char_offset = off;
	*actual_x = act_x;

	return true;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  style        css_style for this text, with style->font_size.size ==
 *                      CSS_FONT_SIZE_LENGTH
 * \param  string       UTF-8 string to measure
 * \param  length       length of string
 * \param  x            width available
 * \param  char_offset  updated to offset in string of actual_x, [0..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 *
 * On exit, [char_offset == 0 ||
 *           string[char_offset] == ' ' ||
 *           char_offset == length]
 */

static bool nsfont_split(const struct css_style *style, const char *string, size_t length, int x, size_t *char_offset, int *actual_x)
{
	LONG act_x;
	APTR tfont;

	tfont = mui_open_font(&fontrp, style);
	*char_offset = 0;
	act_x = 0;

	if (tfont) {
		struct TextExtent extent;
		LONG count;

		length = utf8_codepoints(string, length);
		count = TT_TextFit(&fontrp, (STRPTR)string, length, &extent, NULL, 1, x, 32767);

		while (count > 1) {
			WCHAR wc;

			wc = utf8_at_index(string, count, char_offset);

			if (wc == ' ' || wc == ':' || wc == '.' || wc == ',')	// || wc == '\0')
				break;

			count--;
		}

		*char_offset = utf8_codepoints(string, count);
		act_x = TT_TextLength(&fontrp, (STRPTR)string, count);

		mui_close_font(&fontrp, tfont);
	}

	*actual_x = act_x;

	return true;
}

const struct font_functions nsfont =
{
	nsfont_width,
	nsfont_position_in_string,
	nsfont_split
};
