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

/** \file
 * Various support functions.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/utsname.h>

#include <exec/rawfmt.h>
#include <exec/resident.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/locale.h>
#include <proto/muimaster.h>

#if !defined(__MORPHOS__)
#include <proto/socket.h>
#endif

#include "mui/extrasrc.h"
#include "utils/messages.h"

STRPTR GetNameFromLock(BPTR lock)
{
	STRPTR p;
	ULONG size = 256;

	for (;;) {
		if (!(p = AllocVecTaskPooled(size)))
			break;

		if (NameFromLock(lock, p, size))
			break;

		FreeVecTaskPooled(p);

		size += 128;
		p = NULL;

		if (IoErr() != ERROR_LINE_TOO_LONG)
			break;
	}

	return p;
}

STRPTR BuildFileName(CONST_STRPTR path, CONST_STRPTR file)
{
	ULONG length;
	STRPTR p;

	length = strlen(path) + strlen(file) + 4;

	p = AllocVecTaskPooled(length);

	if (p) {
		strcpy(p, path);
		AddPart(p, file, length);
	}

	return p;
}

STRPTR DupStr(CONST_STRPTR string)
{
	ULONG length;
	STRPTR p;

	length = strlen(string) + 1;

	p = malloc(length);

	if (p)
		bcopy(string, p, length);

	return  p;
}

WCHAR utf8_at_index(CONST_STRPTR str, LONG idx, size_t *bytelength)
{
	ULONG octets, length;
	WCHAR wc;

	length = 0;

	while ((octets = UTF8_Decode(str, &wc))) {
		idx--;
		str += octets;
		length += octets;

		if (idx < 0)
			break;
	}

	*bytelength = length;

	return wc;
}

ULONG utf8_codepoints(CONST_STRPTR str, LONG maxlen)
{
	ULONG count, octets;
	WCHAR dummy;

	count = 0;

	while (maxlen > 0 && (octets = UTF8_Decode(str, &dummy))) {
		count++;
		maxlen -= octets;
		str += octets;
	}

	return count;
}

ULONG utf8_octets(CONST_STRPTR str, LONG maxlen)
{
	ULONG count, octets;
	WCHAR dummy;

	count = 0;

	while (maxlen > 0 && (octets = UTF8_Decode(str, &dummy))) {
		count += octets;
		maxlen--;
		str += octets;
	}

	return count;
}

STRPTR utf8_to_amiga(CONST_STRPTR src)
{
	STRPTR dst;
	ULONG length;

	length = utf8_codepoints(src, 0xffff) + 1;
	dst = AllocVec(length, MEMF_ANY);

	if (dst) {
		ULONG octets;
		STRPTR p;
		WCHAR wc;

		p = dst;

		while ((octets = UTF8_Decode(src, &wc))) {
			if (wc > 255)
				wc = '?';

			src += octets;
			*p++ = wc;
		}

		*p = '\0';
	}

	return dst;
}

void warn_user(const char *warning, const char *detail)
{
	// app, win missing
	#if defined(__MORPHOS__)
	MUI_Request(NULL, NULL, 0, "NetSurf", "*_OK", "%s\n%s", messages_get(warning), detail);
	#else
	MUI_Request(NULL, NULL, 0, "NetSurf", "OK", "%s\n%s", messages_get(warning), detail);
	#endif
}

void die(const char *error)
{
	#if defined(__MORPHOS__)
	MUI_RequestA(NULL, NULL, 0, "NetSurf", "*_OK", (STRPTR)error, NULL);
	#else
	MUI_RequestA(NULL, NULL, 0, "NetSurf", "OK", (STRPTR)error, NULL);
	#endif
	exit(1);
}

char *url_to_path1(const char *url)
{
	char *tmps,*unesc;
	CURL *curl;

	if (tmps = strchr(url,'/'))
	{
		if (tmps = strchr(tmps+1,'/'))
		{
			if (tmps = strchr(tmps+1,'/'))
			{
				if (curl = curl_easy_init())
				{
					unesc = curl_easy_unescape(curl, tmps+1, 0, NULL);
					tmps = strdup(unesc);
					curl_free(unesc);
					curl_easy_cleanup(curl);
					return tmps;
				}
			}
		}
	}

	return strdup((char *)url);
}

char *path_to_url1(const char *path)
{
	char *r = malloc(strlen(path) + 8 + 1);

	if (r)
	{
		strcpy(r, "file:///");
		strcat(r, path);
	}

	return r;
}

int uname1(struct utsname *uts)
{
	struct Resident *res;
	ULONG ver, rev;

	res = FindResident("MorphOS");
	ver = 0;
	rev = 0;

	if (res)
	{
		ver = res->rt_Version;
		#if defined(__MORPHOS__)
		rev = res->rt_Revision;
		#endif
	}

	NewRawDoFmt("%d.%d", (APTR)RAWFMTFUNC_STRING, uts->release, ver, rev);

	#if defined(__MORPHOS__)
	strcpy(uts->sysname, "MorphOS");
	#elif defined(__AROS__)
	strcpy(uts->sysname, "AROS");
	#else
	strcpy(uts->sysname, "Amiga");
	#endif
	strcpy(uts->nodename, "amiga");
	strcpy(uts->machine, "m68k");
}

float strtof(const char* s, char** endptr)
{
	const char *p = s;
	float value = 0.;
	int sign = +1;
	float factor;
	unsigned int expo;

	while (isspace(*p))
		p++;

	switch (*p)
	{
		case '-': sign = -1;
		case '+': p++;
		default : break;
	}

	while ((unsigned int)(*p - '0') < 10u)
		value = value*10 + (*p++ - '0');

	if (*p == '.')
	{
		factor = 1.;

		p++;
		while ((unsigned int)(*p - '0') < 10u)
		{
			factor *= 0.1;
			value  += (*p++ - '0') * factor;
		}
	}

	if ((*p | 32) == 'e')
	{
		expo   = 0;
		factor = 10.L;

		switch (*++p)
		{
			case '-': factor = 0.1;
			case '+': p++;
				break;

			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				break;

			default :
				value = 0.L;
				p     = s;
				goto done;
		}

		while ((unsigned int)(*p - '0') < 10u)
			expo = 10 * expo + (*p++ - '0');

		while (1)
		{
			if (expo & 1)
				value *= factor;

			if ((expo >>= 1) == 0)
				break;

			factor *= factor;
		}
	}

done:
	if (endptr != NULL)
		*endptr = (char*)p;

	return value * sign;
}

#warning "select() is not implemented, using WaitSelect() instead."

int select1(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exeptfds,
	struct timeval *timeout)
{
  return WaitSelect(nfds, readfds, writefds, exeptfds, timeout, NULL);
}

void __chkabort(void) { }
