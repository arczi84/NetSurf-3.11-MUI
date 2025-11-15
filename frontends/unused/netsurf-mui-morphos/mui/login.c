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

#include "desktop/401login.h"
#include "mui/methodstack.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "utils/url.h"

void gui_401login_open(struct browser_window *bw, struct content *c, const char *realm)
{
	char *host;

	if (url_host(c->url, &host) == URL_FUNC_OK)
	{
		methodstack_push_sync(application, 4, MM_Application_Login, bw, c->url, host, realm);
		free(host);
	}
}
