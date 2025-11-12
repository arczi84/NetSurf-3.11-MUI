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

#ifndef _NETSURF_MUI_FONT_H_
#define _NETSURF_MUI_FONT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <exec/types.h>
#include <exec/nodes.h>
#include <graphics/rastport.h>
#include "utils/errors.h"
#include "netsurf/plot_style.h"
#include "netsurf/plotters.h"
#include "utils/utf8.h"
#include "css/css.h"


typedef struct ft_faceid_s {
    char *fontfile;     /* path to font */
    int index;          /* index of font */
    int cidx;           /* character map index for unicode */
} ft_faceid_t;

struct fontnode {
    struct Node node;
    ULONG ysize;
    ULONG weight;
    ULONG style;
    APTR familytable;
    struct TextFont *sysfont;  /* System font fallback */
    ft_faceid_t *ft_face;      /* FreeType face */
    char font_path[256];       /* Font file path */
    ULONG usecount;
    ULONG last_access;
};


/* External variables */
BOOL use_freetype;
/* Font functions */
void font_init(void);
void font_cleanup(void);
void font_cache_check(void);
BOOL needs_character_conversion(void);
size_t convert_utf8_to_iso8859_2(const char *utf8_str, size_t utf8_len,
    char *iso_buffer, size_t iso_buffer_size);

struct converted_text {
    const char *text;
    size_t length;
    char *buffer;
    size_t *index_map;
};

void mui_prepare_converted_text(const char *string, size_t length,
    struct converted_text *out);
void mui_destroy_converted_text(struct converted_text *text);

/* Fixed function signature to use plot_font_style_t instead of css_style */
APTR mui_open_font(struct RastPort *rp, const plot_font_style_t *fstyle);
void mui_close_font(struct RastPort *rp, APTR tfont);

/* Font function table */
struct font_functions {
    bool (*width)(const plot_font_style_t *fstyle,
                  const char *string, size_t length,
                  int *width);
    bool (*position_in_string)(const plot_font_style_t *fstyle,
                               const char *string, size_t length, int x,
                               size_t *char_offset, int *actual_x);
    bool (*split)(const plot_font_style_t *fstyle,
                  const char *string, size_t length, int x,
                  size_t *char_offset, int *actual_x);
};

extern const struct font_functions nsfont;
struct gui_utf8_table *mui_utf8_table;
extern struct gui_layout_table *mui_layout_table;

#endif /* _NETSURF_MUI_FONT_H_ */
