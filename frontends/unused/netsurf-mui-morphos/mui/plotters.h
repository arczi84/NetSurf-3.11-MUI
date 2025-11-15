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

#include "desktop/plotters.h"

struct RenderInfo
{
	#if !defined(__MORPHOS__)
	struct Screen   *screen;
	#endif

	struct RastPort *rp;
	UWORD width, height;
	UWORD maxwidth, maxheight;
};

extern const struct plotter_table muiplot;
extern struct RenderInfo renderinfo;

#endif /* MUI_PLOTTERS_H */
