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
#include <proto/keymap.h>
#include <proto/locale.h>

#include "desktop/gui.h"
#include "desktop/selection.h"
#include "mui/gui.h"
#include "utils/utf8.h"

STATIC struct IFFHandle *iffh;

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
	rc = FALSE;

	if (iffh)
	{
		if (iffh->iff_Stream = (IPTR)OpenClipboard(0))
		{
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
#endif

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
	if (OpenIFF(iffh, IFFF_READ) == 0)
	{
		if (StopChunk(iffh, ID_FTXT, ID_CHRS) == 0)
		{
			ULONG charset, textubfsize;
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

					if (cn->cn_Type == ID_FTXT && size > 0)
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
						browser_window_paste_text(g->bw, textbuf, textlen, true);
					}
					else
					{
						STRPTR dst;
						ULONG newlen;

						#if defined(__MORPHOS2__)
						dst = ansi_to_utf8(textbuf, textlen, &newlen);

						if (dst)
						{
							browser_window_paste_text(g->bw, dst, newlen, true);
							FreeVecTaskPooled(dst);
						}
						#else
			#warning implement me using codesets.library
			#if 0
						utf8_from_local_encoding(textbuf, textlen, &dst);

						if (dst)
							newlen = strlen(dst);

						if (dst)
						{
							browser_window_paste_text(g->bw, dst, newlen, true);
							FreeVecTaskPooled(dst);
						}
			#endif
						#endif
					}
				}

				FreeMem(textbuf + 1);
			}
		}
	}

	CloseIFF(iffh);
}

bool gui_empty_clipboard(void)
{
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

static bool mui_clipboard_copy(const char *text, size_t length, struct box *box, void *handle, const char *whitespace_text,size_t whitespace_length)
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
			if (!gui_add_to_clipboard(text, length, box->space))
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

			if (s->defined && selection_traverse(s, mui_clipboard_copy, NULL))
			{
				rc = true;
			}
		}
		else
		{
			PopChunk(iffh);
		}

		CloseIFF(iffh);
	}

	return rc;
}
