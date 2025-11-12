#ifndef MUI_PLOTTERS_H
#define MUI_PLOTTERS_H

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

#include "netsurf/plotters.h"

/* Add the necessary Amiga/MorphOS includes at the top */
#include <exec/types.h>
#include <intuition/screens.h>
#include <graphics/rastport.h>

VOID SetColorAB(struct RastPort *rp, ULONG c, ULONG bg);

struct RenderInfo
{
	#if !defined(__MORPHOS__)
	struct Screen   *screen;
	#endif
	int origin_x;
	int origin_y;
	struct RastPort *rp;
	UWORD width, height;
	UWORD maxwidth, maxheight;
};

extern const struct plotter_table muiplot;
extern struct RenderInfo renderinfo;
extern nserror mui_plotters_init(void);

#endif /* MUI_PLOTTERS_H */
