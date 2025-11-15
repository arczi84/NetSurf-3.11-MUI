/*
 * Copyright 2009 Ilkka Lehtoranta
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

#include <string.h>

#include <cybergraphx/cybergraphics.h>
#include <devices/turboprint.h>
#include <dos/dostags.h>
#include <hardware/atomic.h>
#include <proto/cybergraphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include "mui/extrasrc.h"
#include "mui/netsurf.h"
#include "mui/print.h"
#include "utils/utils.h"

/* Note: This is an union, not struct */
union PrinterIO
{
	struct IOStdReq    ios;
	struct IODRPReq    drp;
	struct IOPrtCmdReq pcr;
};

static void dump_bitmap(union PrinterIO *io, struct TagItem *taglist)
{
	struct TagItem *tag, *tstate = taglist;
	struct TPExtIODRP ExtIoDrp;
	ULONG width = 0, height = 0, modulo = 0;
	struct ColorMap *colormap = NULL;
	struct ColorMap *dummycm = NULL;
	UBYTE *argb = NULL;
	APTR rgb = NULL;
	APTR tmp = NULL;

	while ((tag = NextTagItem(&tstate))) {
		switch (tag->ti_Tag) {
		//case PRTTAG_BitMap: bitmap = (APTR)tag->ti_Data; break;
		case PRTTAG_ARGB  : argb = (APTR)tag->ti_Data; break;
		case PRTTAG_RGB   : rgb = (APTR)tag->ti_Data; break;
		case PRTTAG_Width : width = tag->ti_Data; break;
		case PRTTAG_Height: height = tag->ti_Data; break;
		case PRTTAG_Modulo: modulo = tag->ti_Data; break;
		case PRTTAG_ColorMap: colormap = (APTR)tag->ti_Data; break;
		}
	}

	if ((!argb || !rgb) || !width || !height || !modulo)
		return;

	if (!colormap) {
		/* Apparently TurboPrint uses colormap for truecolor bitmaps aswell.
		 * That sucks...
		 */
		dummycm = GetColorMap(256);
		if (!dummycm)
			return;

		colormap = dummycm;
	}

	if (rgb == NULL)
		rgb = tmp = AllocMem(width * height * 3, MEMF_ANY);

	if (rgb) {
		struct RastPort rp;
		struct BitMap bm;
		ULONG x, y;

		if (argb)
		{
			UBYTE *src, *dst;

			dst = rgb;

			for (y = 0; y < height; y++) {
				src = argb;

				for (x = 0; x < width; x++) {
					dst[0] = src[1];
					dst[1] = src[2];
					dst[2] = src[3];

					src += 4;
					dst += 3;
				}

				argb += modulo;
			}
		}

		InitRastPort(&rp);
		InitBitMap(&bm, 1, width, height);
		bm.BytesPerRow = width * 3;
		bm.Planes[0] = rgb;
		rp.BitMap = &bm;

		io->drp.io_Command = PRD_TPEXTDUMPRPORT;
		io->drp.io_RastPort = &rp;
		io->drp.io_ColorMap = colormap;
		io->drp.io_SrcX = 0;
		io->drp.io_SrcY = 0;
		io->drp.io_SrcWidth = width;
		io->drp.io_SrcHeight = height;
		io->drp.io_DestCols = 0;
		io->drp.io_DestRows = 0;
		io->drp.io_Special = 0;

		io->drp.io_Modes = (IPTR)&ExtIoDrp;

		ExtIoDrp.PixAspX = 1;
		ExtIoDrp.PixAspY = 1;
		ExtIoDrp.Mode = TPFMT_RGB24;	//TPFMT_CyberGraphX;

		SendIO((struct IORequest *)io);

		for (;; ) {
			ULONG sigmask;

			sigmask = Wait(1 <<
					io->ios.io_Message.mn_ReplyPort->mp_SigBit
					| SIGBREAKF_CTRL_C);

			if (GetMsg(io->ios.io_Message.mn_ReplyPort))
				break;

			if (sigmask & SIGBREAKF_CTRL_C) {
				AbortIO((struct IORequest *)io);
				WaitIO((struct IORequest *)io);
				break;
			}
		}

		if (tmp)
			FreeMem(rgb, width * height * 3);
	}

	if (dummycm)
		FreeColorMap(dummycm);
}


STATIC VOID printer(struct TagItem *tags)
{
	struct MsgPort *port;
	APTR buf;

	port = CreateMsgPort();

	if (port) {
		union PrinterIO io;

		io.ios.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
		io.ios.io_Message.mn_ReplyPort = port;
		io.ios.io_Message.mn_Length = sizeof(io);

		if (!OpenDevice("printer.device", 0, (struct IORequest *)&io, 0)) {
			dump_bitmap(&io, tags);
			CloseDevice((struct IORequest *)&io);
		} else {
			warn_user("Error:", "Could not open printer.device!");
		}

		DeleteMsgPort(port);
	}

	buf = (APTR)GetTagData(PRTTAG_RGB, NULL, tags);

	FreeVec(buf);
	FreeTagItems(tags);
}

VOID print_doc(struct RastPort *rp, ULONG width, ULONG height)
{
	APTR rgb;

	rgb = AllocVec(width * height * 3, MEMF_ANY);

	if (rgb) {
		struct TagItem *tags;

		tags = AllocateTagItems(4);

		if (tags) {
			struct Message *msg;

			msg = AllocMem(sizeof(*msg), MEMF_ANY);

			if (msg) {
				APTR proc;

				msg->mn_ReplyPort = &StartupMsgPort;

				/* XXX: max width/height is 65536/65536 */
				ReadPixelArray(rgb, 0, 0, width * 3, rp, 0, 0,
						width, height, RECTFMT_RGB);

				tags[0].ti_Tag  = PRTTAG_RGB;
				tags[0].ti_Data = (IPTR)rgb;
				tags[1].ti_Tag  = PRTTAG_Width;
				tags[1].ti_Data = width;
				tags[2].ti_Tag  = PRTTAG_Height;
				tags[2].ti_Data = height;
				tags[3].ti_Tag  = TAG_DONE;

				proc = CreateNewProcTags(
						#if defined(__MORPHOS__)
						NP_CodeType, CODETYPE_PPC,
						NP_StartupMsg, msg,
						NP_PPC_Arg1, tags,
						#endif
						NP_Entry, &printer,
						NP_Name, "NetSurf Printing Process",
						NP_Priority, -1,
						TAG_DONE
						);

				if (proc) {
					ATOMIC_ADD(&thread_count, 1);
					return;
				}

				FreeMem(msg, sizeof(*msg));
			}

			FreeTagItems(tags);
		}

		FreeVec(rgb);
	}
}
