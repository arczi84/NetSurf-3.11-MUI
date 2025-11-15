#ifndef MORPHOS_APPLICATIONCLASS_H
#define MORPHOS_APPLICATIONCLASS_H

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

enum
{
	WINDOW_SETTINGS,
	WINDOW_DOWNLOADS,
	WINDOW_HOTLIST,
};

enum
{
	MNA_NEW_WINDOW = 1000,
	MNA_NEW_PAGE,
	MNA_OPEN_LOCAL_FILE,
	MNA_RELOAD,
	MNA_SAVE_AS_TEXT,
	MNA_SAVE_AS_SOURCE,
	MNA_SAVE_AS_PDF,
	MNA_PRINT,
	MNA_ABOUT,
	MNA_CLOSE_PAGE,
	MNA_CLOSE_WINDOW,
	MNA_COPY,
	MNA_SELECT_ALL,
	MNA_FIND,
	MNA_DOWNLOADS_WINDOW,
	MNA_HOTLIST_WINDOW,
	MNA_NETSURF_SETTINGS,
	MNA_MUI_SETTINGS
};

#endif /* MORPHOS_APPLICATIONCLASS_H */
