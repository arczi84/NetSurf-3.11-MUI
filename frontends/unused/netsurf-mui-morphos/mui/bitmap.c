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

#include <stdbool.h>
#include <sys/types.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "image/bitmap.h"
#include "macros/vapor.h"
#include "mui/bitmap.h"
#include "mui/extrasrc.h"

STATIC struct MinList bitmaplist =
{
	(APTR)&bitmaplist.mlh_Tail, NULL, (APTR)&bitmaplist
};

void bitmap_cleanup(void)
{
	struct bitmap *next, *node;

	ITERATELISTSAFE(node, next, &bitmaplist) {
		bitmap_destroy(node);
	}
}

void bitmap_cache_check(void)
{
	struct bitmap *node;
	ULONG secs, dummy, time;

	CurrentTime(&secs, &dummy);

	time = secs - 180;	// last access was three minutes ago

	ITERATELIST(node, &bitmaplist) {
		if (node->last_access < time) {
			FreeBitMap(node->bitmap);
			node->bitmap = NULL;
		}
	}
}

/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bm;

	bm = AllocMem(sizeof(*bm), MEMF_ANY);

	if (bm)
	{
		ULONG modulo;

		modulo = width * sizeof(ULONG);
		//XXX: AltiVec alignment, some internal decoders fail to respect modulo
		//modulo = (width * sizeof(ULONG) + 15) & ~15;

		bm->pixdata = AllocMemAligned(modulo * height, state & BITMAP_CLEAR_MEMORY ? MEMF_CLEAR : MEMF_ANY, 16, 0);

		if (bm->pixdata)
		{
			bm->bitmap = NULL;
			bm->width  = width;
			bm->height = height;
			bm->modulo = modulo;
			bm->opaque = state & BITMAP_OPAQUE ? TRUE : FALSE;
			ADDTAIL(&bitmaplist, &bm->node);
		}
		else
		{
			FreeMem(bm, sizeof(*bm));
			bm = NULL;
		}
	}

	return bm;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */

unsigned char *bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->pixdata;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->modulo;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *bitmap)
{
	struct bitmap *bm = bitmap;

	REMOVE(&bm->node);
	FreeBitMap(bm->bitmap);
	FreeMem(bm->pixdata, bm->modulo * bm->height);
	FreeMem(bm, sizeof(*bm));
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \return true on success, false on error and error reported
 */
bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *bitmap)
{
	struct bitmap *bm = bitmap;
	bm->update = 1;
}


/**
 * The bitmap image can be suspended.
 *
 * \param  bitmap  	a bitmap, as returned by bitmap_create()
 * \param  private_word	a private word to be returned later
 * \param  suspend	the function to be called upon suspension
 * \param  resume	the function to be called when resuming
 */
void bitmap_set_suspendable(void *bitmap, void *private_word,
		void (*invalidate)(void *bitmap, void *private_word))
{
}

/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *bitmap, bool opaque)
{
	struct bitmap *bm = bitmap;
	bm->opaque = opaque;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->opaque ? true : false;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->opaque;
}

int bitmap_get_width(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->width;
}

int bitmap_get_height(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->height;
}


/**
 * Find the bytes per pixel of a bitmap
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixel
 */

size_t bitmap_get_bpp(void *vbitmap)
{
	return 4;
}

void bitmap_flush(void *vbitmap, BOOL delayed)
{
	struct bitmap *bm = vbitmap;

	if (delayed)
	{
		ULONG dummy;

		CurrentTime(&bm->last_access, &dummy);
	}
	else
	{
		FreeBitMap(bm->bitmap);
		bm->bitmap = NULL;
	}
}
