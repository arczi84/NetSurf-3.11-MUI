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

#include <datatypes/textclass.h>
#include <libraries/locale.h>
#include <proto/exec.h>
#include <proto/iffparse.h>
#include <libraries/iffparse.h> 
#include <proto/keymap.h>
#include <proto/locale.h>
//include for size_t
#include <string.h>

#include "desktop/gui_table.h"
#include "desktop/selection.h"
#include "netsurf/clipboard.h"
#include "html/box.h"
#include "mui/gui.h"
#include "utils/utf8.h"
#include "utils/errors.h"

STATIC struct IFFHandle *iffh;
extern struct Library *IFFParseBase;

struct CSet
{
	/* 0=ECMA Latin 1 (std Amiga charset) */
	LONG CodeSet;    
	/* Specification has 7 here but it does not work with it... why? */    
	LONG Reserved[5];
};

#ifndef CODESET_LATIN1
#define CODESET_LATIN1  0
#endif

#ifndef CODESET_UTF8
#define CODESET_UTF8    1
#endif

#ifndef ID_CSET
#define ID_CSET	MAKE_ID('C','S','E','T')
#endif

LONG mui_clipboard_init(void)
{
	LONG rc;

	iffh = AllocIFF();
	//printf("mui_clipboard_init: AllocIFF=%p\n", iffh);
	rc = FALSE;

	if (iffh)
	{
		//printf("mui_clipboard_init: OpenClipboard\n");
		if (iffh->iff_Stream = (IPTR)OpenClipboard(0))
		{
			//printf("mui_clipboard_init: InitIFFasClip\n");
			InitIFFasClip(iffh);
			rc = TRUE;
		}
	}

	return rc;
}

void mui_clipboard_free(void)
{
	if (iffh)
	{
		if(iffh->iff_Stream)
			CloseClipboard((struct ClipboardHandle *)iffh->iff_Stream);

		FreeIFF(iffh);
	}
}

void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
}

void gui_start_selection(struct gui_window *g)
{
}

#if defined(__MORPHOS2__)
STATIC STRPTR ansi_to_utf8(STRPTR src, ULONG srclen, ULONG *dstlen)
{
   static struct KeyMap *keymap;
   CONST_STRPTR ptr;
   STRPTR dst;
   ULONG octets;
	UBYTE c;

   keymap = AskKeyMapDefault();

   ptr = src;
	octets = 0;

	while ((c = *ptr))
   {
      WCHAR wc;
		TEXT buf[4];

		ptr++;

		wc = ToUCS4(c, keymap);
		octets += UTF8_Encode(wc, buf);
	}

	*dstlen = octets;
	dst = NULL;

	if (octets)
	{
		dst = AllocVecTaskPooled(octets + 1);

		if (dst)
		{
		   ptr = src;

			while ((c = *ptr))
		   {
		      WCHAR wc;

				ptr++;

				wc   = ToUCS4(c, keymap);
				dst += UTF8_Encode(wc, dst);
			}

			*dst = '\0';
		}
	}

   return dst;
}

/* Updated function signature and implementation for MorphOS 2 */
nserror utf8_from_local_encoding(const char *string, size_t len, char **result)
{
	ULONG newlen;
	STRPTR dst;

	dst = ansi_to_utf8((STRPTR)string, len, &newlen);
	if (!dst) {
		*result = NULL;
		return NSERROR_NOMEM;
	}

	*result = dst;
	return NSERROR_OK;
}

nserror utf8_to_local_encoding(const char *string, size_t len, char **result)
{
	/* For now, just copy the UTF-8 string as-is on MorphOS 2 */
	char *dst = malloc(len + 1);
	if (!dst) {
		*result = NULL;
		return NSERROR_NOMEM;
	}

	memcpy(dst, string, len);
	dst[len] = '\0';
	*result = dst;
	return NSERROR_OK;
}

#else
/* Stub implementations for other systems */
nserror utf8_from_local_encoding(const char *string, size_t len, char **result)
{
	/* TODO: implement using codesets.library */
	char *dst = malloc(len + 1);
	if (!dst) {
		*result = NULL;
		return NSERROR_NOMEM;
	}

	memcpy(dst, string, len);
	dst[len] = '\0';
	*result = dst;
	return NSERROR_OK;
}

nserror utf8_to_local_encoding(const char *string, size_t len, char **result)
{
	/* TODO: implement using codesets.library */
	char *dst = malloc(len + 1);
	if (!dst) {
		*result = NULL;
		return NSERROR_NOMEM;
	}

	memcpy(dst, string, len);
	dst[len] = '\0';
	*result = dst;
	return NSERROR_OK;
}
#endif

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
	if (OpenIFF(iffh, IFFF_READ) == 0)
	{
		if (StopChunk(iffh, ID_FTXT, ID_CHRS) == 0)
		{
			ULONG charset, textbufsize;
			LONG textlen;
			STRPTR textbuf;

			StopChunk(iffh, ID_FTXT, ID_CSET);

			charset = CODESET_LATIN1;
			textbuf = NULL;
			textlen = 0;

			while (ParseIFF(iffh, IFFPARSE_SCAN) == 0)
			{
				struct ContextNode *cn;

				cn = CurrentChunk(iffh);

				if (cn)
				{
					textbufsize = cn->cn_Size;

					if (cn->cn_Type == ID_FTXT && textbufsize > 0) //was size
					{
						if (cn->cn_ID == ID_CHRS && !textbuf)
						{
							textbuf = AllocMem(textbufsize + 1, MEMF_ANY);

							if (textbuf)
							{
								textlen = ReadChunkBytes(iffh, textbuf, textbufsize);

								if (textlen > 0)
									textbuf[textlen] = '\0';
							}
						}
						else if (cn->cn_ID == ID_CSET)
						{
							struct CSet cset;

							if (ReadChunkBytes(iffh, &cset, sizeof(cset)) >= 4)
							{
								charset = cset.CodeSet == CODESET_UTF8 ? CODESET_UTF8 : CODESET_LATIN1;

								if (textbuf)
									break;
							}
						}
					}
				}
			}

			if (textbuf)
			{
				if (textlen)
				{
					if (charset == CODESET_UTF8)
					{
						//browser_window_paste_text(g->bw, textbuf, textlen, true);
					}
					else
					{
						char *dst;
						nserror error;

						error = utf8_from_local_encoding(textbuf, textlen, &dst);

						if (error == NSERROR_OK && dst)
						{
							#warning "use updated paste function"
							//browser_window_paste_text(g->bw, dst, strlen(dst), true);
							free(dst);
						}
					}
				}

				FreeMem(textbuf, textbufsize + 1); // Fixed memory deallocation
			}
		}
	}

	CloseIFF(iffh);
}

bool gui_empty_clipboard(void)
{
	return true;
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	WriteChunkBytes(iffh, (APTR)text, length);

	if (space)
		WriteChunkBytes(iffh, " ", 1);

	return true;
}

bool gui_commit_clipboard(void)
{
	return true;
}

static bool mui_clipboard_copy(const char *text, size_t length, struct box *box, void *handle, const char *whitespace_text, size_t whitespace_length)
{
	bool rc = false;

	if (PushChunk(iffh, 0, ID_CHRS, IFFSIZE_UNKNOWN) == 0)
	{
		if (whitespace_text)
		{
			if (!gui_add_to_clipboard(whitespace_text, whitespace_length, false))
				return rc;
		}

		if (text)
		{
			/* Check if box is valid before dereferencing */
			bool add_space = (box != NULL) ? box->space != 0 : false;
			if (!gui_add_to_clipboard(text, length, add_space))
				return rc;
		}

		rc = true;
	}
	else
	{
		rc = false;
	}

	PopChunk(iffh);

	return rc;
}

STATIC CONST struct CSet cset = { CODESET_UTF8, 0, 0, 0, 0, 0 };

bool gui_copy_to_clipboard(struct selection *s)
{
	bool rc = false;

	if (OpenIFF(iffh, IFFF_WRITE) == 0)
	{
		if (PushChunk(iffh,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN) == 0)
		{
			if (PushChunk(iffh, 0, ID_CSET, sizeof(cset)) == 0)
			{
				WriteChunkBytes(iffh, (APTR)&cset, sizeof(cset));
				PopChunk(iffh);
			}
#if 0
//disabled
			/* Check if selection is valid before dereferencing */
			if (s != NULL && selection_defined(s) && selection_traverse(s, mui_clipboard_copy, NULL))
			{
				rc = true;
			}
#endif	
		}
		else
		{
			PopChunk(iffh);
		}

		CloseIFF(iffh);
	}

	return rc;
}

static struct gui_clipboard_table clipboard_table = {
	.get = gui_paste_from_clipboard,
	.set = gui_add_to_clipboard,
};

struct gui_clipboard_table *mui_clipboard_table = &clipboard_table;
