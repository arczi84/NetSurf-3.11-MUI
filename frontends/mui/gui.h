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
#include "utils/errors.h"
#include "netsurf/browser.h"

#include "mui/extrasrc.h"

struct download_context;

extern struct SignalSemaphore gfx_semaphore;

struct RastPort;

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
	struct download_context *ctx;
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

void die(const char *error);

// Eksportuj globalne zmienne i funkcje
extern bool mui_redraw_pending;
extern bool mui_supports_pointertype;
extern bool mui_supports_pushmethod_delay;
void mui_schedule_redraw(struct Data *data, bool full_redraw);
void mui_trigger_content_ready_redraw(Object *obj);

bool mui_redraw_tiles_surface(struct gui_window *g,
	struct RastPort *rp,
	int viewport_w,
	int viewport_h,
	int scroll_x,
	int scroll_y,
	void (*flush_fn)(struct gui_window *g,
		void *ctx,
		int screen_x0,
		int screen_y0,
		int screen_x1,
		int screen_y1),
	void *flush_ctx);
#endif /* MUI_GUI_H */
