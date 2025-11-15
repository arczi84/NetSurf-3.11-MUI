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

#include <proto/graphics.h>

#include "content/urldb.h"
#include "desktop/browser.h"
#include "mui/bitmap.h"

bool thumbnail_create(struct content *content, struct bitmap *bitmap, const char *url)
{
#if 1
	// must think about this again...
	return false;
#else
	bool rc;

	#warning not sure about this
	if (bitmap->bitmap == NULL)
		bitmap->bitmap = AllocBitMap(bitmap->width, bitmap->height, 32,  BMF_MINPLANES, renderinfo.rp->BitMap);

	rc = false;

	if (bitmap->bitmap)
	{
		struct BitScaleArgs bsa;

		content_redraw(content, 0, 0, content->width, content->width,
			0, 0, content->width, content->width, 1.0, 0xFFFFFF);

		bsa.bsa_SrcX = 0;
		bsa.bsa_SrcY = 0;
		bsa.bsa_SrcWidth = content->width;
		bsa.bsa_SrcHeight = content->width;
		bsa.bsa_DestX = 0;
		bsa.bsa_DestY = 0;
		bsa.bsa_XSrcFactor = content->width;
		bsa.bsa_XDestFactor = bitmap->width;
		bsa.bsa_YSrcFactor = content->width;
		bsa.bsa_YDestFactor = bitmap->height;
		bsa.bsa_SrcBitMap = foobar_bitmap;
		bsa.bsa_DestBitMap = bitmap->bitmap;
		bsa.bsa_Flags = 0;

		BitMapScale(&bsa);

		rc = true;

		if (url)
			urldb_set_thumbnail(url, bitmap);
	}

	return rc;
#endif
}
