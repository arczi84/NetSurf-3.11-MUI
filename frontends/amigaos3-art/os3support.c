/*
 * Copyright 2014 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/** \file
 * Compatibility functions for AmigaOS 3
 */

#ifndef __amigaos4__
#include "amiga/os3support.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>


#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include "utils/log.h"

#define SUCCESS (TRUE)
#define FAILURE (FALSE)
#define NO      !

/* Utility */
struct FormatContext
{
	STRPTR	Index;
	LONG	Size;
	BOOL	Overflow;
};


/* C */
char *strlwr(char *str)
{
  size_t i;
  size_t len = strlen(str);

  for(i=0; i<len; i++)
  str[i] = tolower((unsigned char)str[i]);

  return str;
}

char *strsep(char **s1, const char *s2)
{
	char *const p1 = *s1;

	if (p1 != NULL) {
		*s1 = strpbrk(p1, s2);
		if (*s1 != NULL) {
			*(*s1)++ = '\0';
		}
	}
	return p1;
}

void FreeSysObject(ULONG type, APTR obj)
{
	switch(type) {
		case ASOT_PORT:
			DeleteMsgPort(obj);
		break;
		case ASOT_IOREQUEST:
			DeleteIORequest(obj);
		break;
	}
}



#endif

