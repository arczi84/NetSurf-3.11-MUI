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
#undef NO_INLINE_STDARG
#include <proto/ttengine.h>

#include "css/css.h"
#include "netsurf/plot_style.h"
#include "macros/vapor.h"
#include "netsurf/utf8.h"
#include "netsurf/layout.h"
#include "mui/font.h"
#include "mui/plotters.h"
#include "mui/utils.h"
#include "html/font.h"
#include "utils/utf8.h"
#include "utils/log.h"
#include "os3.h"

#define FONT_LOG
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
    time = secs - 180;  // last access was three minutes ago
    
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
    
    if (node) {
        CurrentTime(&node->last_access, &dummy);
        ATOMIC_SUB((ULONG *)&node->usecount, 1);
    }
}

STATIC CONST CONST_STRPTR fsans_serif[] =
{
    "DejaVuSans", "Arial", "sans", NULL
};

STATIC CONST CONST_STRPTR fserif[] =
{
    "DejaVuSerif", "Times New Roman", "serif", NULL
};

STATIC CONST CONST_STRPTR fmonospaced[] =
{
    "DejaVuSansMono", "Courier New", "monospace", NULL
};

STATIC CONST CONST_STRPTR fcursive[] =
{
    "Comic Sans MS", "cursive", NULL
};

STATIC CONST CONST_STRPTR ffantasy[] =
{
    "Impact", "fantasy", NULL
};

STATIC CONST CONST_STRPTR fdefault[] =
{
    "DejaVuSans", "Arial", NULL
};

/* Fixed function signature to match header */
APTR mui_open_font(struct RastPort *rp, const plot_font_style_t *fstyle)
{
    CONST CONST_STRPTR *table;
    struct fontnode *node;
    ULONG css_size, weight, fontstyle;
    ULONG tt_size;
    APTR tfont;

    FONT_LOG(("DEBUG: mui_open_font called."));
    FONT_LOG(("DEBUG: TTEngineBase = %p.", TTEngineBase));
    FONT_LOG(("DEBUG: fstyle = %p.", fstyle));

    if (!TTEngineBase || !fstyle) {
        FONT_LOG(("ERROR: Missing TTEngineBase or fstyle."));
        return NULL;
    }

    switch (fstyle->family) {
    case PLOT_FONT_FAMILY_SANS_SERIF:
        table = fsans_serif;
        break;
    case PLOT_FONT_FAMILY_SERIF:
        table = fserif;
        break;
    case PLOT_FONT_FAMILY_MONOSPACE:
        table = fmonospaced;
        break;
    case PLOT_FONT_FAMILY_CURSIVE:
        table = fcursive;
        break;
    case PLOT_FONT_FAMILY_FANTASY:
        table = ffantasy;
        break;
    default:
        table = fdefault;
        break;
    }

    FONT_LOG(("DEBUG: Font family: %d, table first: %s.", fstyle->family, table[0]));

    fontstyle = (fstyle->flags & FONTF_ITALIC) ? TT_FontStyle_Italic : TT_FontStyle_Regular;

    if (fstyle->weight >= 600) {
        weight = TT_FontWeight_Bold;
    } else if (fstyle->weight <= 300) {
        weight = TT_FontWeight_Normal - 100;
    } else {
        weight = TT_FontWeight_Normal;
    }

    css_size = plot_style_fixed_to_int(fstyle->size);
    if (css_size < 8) css_size = 8;
    if (css_size > 72) css_size = 72;
    tt_size = css_size << 6;  /* 26.6 fixed point */

    FONT_LOG(("DEBUG: Font params: css_size=%lu tt_size=%lu weight=%lu style=%lu.",
        css_size, tt_size, weight, fontstyle));

    tfont = NULL;

    ObtainSemaphore(&semaphore);
    ITERATELIST(node, &fontcache) {
        if (node->ysize == css_size &&
            node->weight == weight &&
            node->style == fontstyle &&
            node->familytable == table) {
            ATOMIC_ADD((ULONG *)&node->usecount, 1);
            tfont = node;
            FONT_LOG(("DEBUG: Found cached font: %p.", tfont));
            break;
        }
    }
    ReleaseSemaphore(&semaphore);

    if (!tfont) {
        FONT_LOG(("DEBUG: No cached font, creating new."));
        node = AllocMem(sizeof(*node), MEMF_ANY | MEMF_CLEAR);
        if (node) {
            node->tfont = TT_OpenFont(
                TT_FamilyTable, table,
                TT_FontSize, tt_size,
                TT_FontWeight, weight,
                TT_FontStyle, fontstyle,
                TAG_DONE);

            if (!node->tfont) {
                static CONST CONST_STRPTR fallback_files[] = {
                    "FONTS:_TRUETYPE/DejaVuSans.ttf",
                    "FONTS:_TRUETYPE/Arial.ttf",
                    "FONTS:DejaVuSans.ttf",
                    "FONTS:Arial.ttf",
                    "FONTS:Times.ttf",
                    NULL
                };

                for (int i = 0; fallback_files[i]; i++) {
                    FONT_LOG(("DEBUG: Trying font file: %s.", fallback_files[i]));
                    node->tfont = TT_OpenFont(
                        TT_FontFile, (ULONG)fallback_files[i],
                        TT_FontSize, tt_size,
                        TT_FontWeight, weight,
                        TT_FontStyle, fontstyle,
                        TAG_DONE);
                    if (node->tfont) {
                        FONT_LOG(("DEBUG: Success with file %s.", fallback_files[i]));
                        break;
                    }
                }
            }

            if (node->tfont) {
                node->ysize = css_size;
                node->weight = weight;
                node->style = fontstyle;
                node->familytable = (APTR)table;
                node->usecount = 1;

                ObtainSemaphore(&semaphore);
                ADDTAIL(&fontcache, node);
                ReleaseSemaphore(&semaphore);

                tfont = node;
            } else {
                FONT_LOG(("ERROR: All TT_OpenFont attempts failed - check font files in FONTS:_TRUETYPE/."));
                FreeMem(node, sizeof(*node));
            }
        } else {
            FONT_LOG(("ERROR: AllocMem failed for font node."));
        }
    }

    if (tfont) {
        struct fontnode *fnode = (struct fontnode *)tfont;
        if (TTEngineBase->lib_Version >= 8) {
            TT_SetAttrs(rp,
                TT_Antialias, TT_Antialias_On,
                TT_Encoding, TT_Encoding_System_UTF8,
                TT_ColorMap, 1,
                TAG_DONE);
        } else {
            TT_SetAttrs(rp,
                TT_Antialias, TT_Antialias_Off,
                TT_Encoding, TT_Encoding_Default,
                TAG_DONE);
        }

        TT_SetFont(rp, fnode->tfont);
    } else {
        FONT_LOG(("ERROR: No font loaded - text won't render."));
    }

    FONT_LOG(("DEBUG: mui_open_font returning: %p.", tfont));
    return tfont;
}

/**
 * Measure the width of a string.
 */
static bool nsfont_width(const plot_font_style_t *fstyle,
        const char *string, size_t length,
        int *width)
{
    APTR tfont;
    struct fontnode *node;
    int w;
    
    tfont = mui_open_font(&fontrp, fstyle);
    w = 0;
    
    if (tfont) {
        node = (struct fontnode *)tfont;
        length = utf8_bounded_length(string, length);
        w = TT_TextLength(&fontrp, (STRPTR)string, length);
        mui_close_font(&fontrp, tfont);
    } else {
        FONT_LOG(("ERROR: No font - width=0."));
    }
    
    *width = w;
    return true;
}

/**
 * Find the position in a string where an x coordinate falls.
 */
static bool nsfont_position_in_string(const plot_font_style_t *fstyle, 
        const char *string, size_t length, int x, 
        size_t *char_offset, int *actual_x)
{
    int off, act_x;
    APTR tfont;
    struct fontnode *node;
    
    tfont = mui_open_font(&fontrp, fstyle);
    off = 0;
    act_x = 0;
    
    if (tfont) {
        struct TextExtent extent;
        node = (struct fontnode *)tfont;
        length = utf8_bounded_length(string, length);
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
 */
static bool nsfont_split(const plot_font_style_t *fstyle, 
        const char *string, size_t length, int x, 
        size_t *char_offset, int *actual_x)
{
    LONG act_x;
    APTR tfont;
    struct fontnode *node;
    
    tfont = mui_open_font(&fontrp, fstyle);
    *char_offset = 0;
    act_x = 0;
    
    if (tfont) {
        struct TextExtent extent;
        LONG count;
        node = (struct fontnode *)tfont;
        length = utf8_bounded_length(string, length);
        count = TT_TextFit(&fontrp, (STRPTR)string, length, &extent, NULL, 1, x, 32767);
        
        while (count > 1) {
            char c = string[count - 1];
            if (c == ' ' || c == ':' || c == '.' || c == ',')
                break;
            count--;
        }
        
        *char_offset = count;
        act_x = TT_TextLength(&fontrp, (STRPTR)string, count);
        mui_close_font(&fontrp, tfont);
    }
    
    *actual_x = act_x;
    return true;
}

/* Fixed structure definition */
static const struct font_functions nsfont =
{
    .width = nsfont_width,
    .position_in_string = nsfont_position_in_string,
    .split = nsfont_split
};

static struct gui_layout_table layout_table = {
    .width = nsfont_width,
    .position = nsfont_position_in_string,
    .split = nsfont_split
};

struct gui_utf8_table *mui_utf8_table = NULL;
struct gui_layout_table *mui_layout_table = &layout_table;