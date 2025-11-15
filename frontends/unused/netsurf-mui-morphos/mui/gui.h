#ifndef MUI_GUI_H
#define MUI_GUI_H

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

#include <dos/dos.h>

#include "desktop/browser.h"

extern struct SignalSemaphore gfx_semaphore;

/* MorphOS only */
extern LONG altivec_accelerated;

/* Amiga only */
extern LONG global_pen_a;
extern LONG global_pen_b;

struct download
{
	STRPTR filename;
	STRPTR path;
	UQUAD size;
	UQUAD done;
	UBYTE data[0];
};

struct gui_download_window
{
	struct MinNode   node;
	struct download *dl;
	APTR fh;
};

struct gui_window
{
	struct MinNode node;
	struct browser_window *bw;
	LONG pointertype;
	APTR obj;
	APTR win;

	LONG redraw;

	APTR RastPort;
	APTR BitMap;
	APTR Layer;
	APTR LayerInfo;
};

#endif /* MUI_GUI_H */
