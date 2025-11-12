#ifndef MUI_BITMAP_H
#define MUI_BITMAP_H

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

#include <exec/types.h>

struct bitmap
{
	struct MinNode node;
	struct BitMap *bitmap;
	WORD  update;
	WORD  opaque;
	UWORD width;
	UWORD height;
	UBYTE *pixdata;
	ULONG modulo;
	ULONG last_access;
	PLANEPTR mask;
	UWORD mask_width;
	UWORD mask_height;
	UWORD mask_modulo;
	BOOL mask_valid;
};

void bitmap_cache_check(void);
void bitmap_cleanup(void);
void bitmap_flush(void *vbitmap, BOOL delayed);

PLANEPTR mui_bitmap_get_mask(struct bitmap *bitmap, struct BitMap *native_bm,
				   UWORD *modulo_out);

extern struct gui_bitmap_table *mui_bitmap_table;

#endif /* MUI_BITMAP_H */
