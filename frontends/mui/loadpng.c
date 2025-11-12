/* $Id: loadpng.c,v 1.2 2008/06/27 22:51:10 itix Exp $ */

/* PNG loader
 *
 * Written by Ilkka Lehtoranta
 * Public Domain
 */

#if !defined(__MORPHOS__)
#include <stdlib.h>
#endif

#include <libraries/iffparse.h>
#include <proto/dos.h>
#include <proto/exec.h>

#if defined(__MORPHOS__)
#include <proto/z.h>
#else
#include <zlib.h>
#endif

#include "mui/extrasrc.h"
#include "mui/gui.h"
#include "mui/loadpng.h"
#include "utils/log.h"

STATIC APTR png_find_chunk(ULONG *img, ULONG id, LONG length)
{
	APTR chunk = NULL;

	while (length > 12)
	{
		ULONG l;

		chunk = img;

		l = img[0] + 12;

		#if defined(__MORPHOS__)
		if (img[1] == id)
			break;
		#else
		{
			ULONG id2;
			UBYTE *p;

			p = (UBYTE *)&img[1];

			id2 = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];

			if (id2 == id)
				break;
		}
		#endif

		chunk = NULL;

		img = (APTR)((ULONG)img + l);
		length -= l;
	}

	return chunk;
}

STATIC ULONG PaethPredictor(ULONG a, ULONG b, ULONG c)
{
	// a = left, b = above, c = upper left
	LONG p, pa, pb, pc, retval;

	p = a + b - c;
	pa = abs(p - a);
	pb = abs(p - b);
	pc = abs(p - c);

	retval = c;

	if (pa <= pb && pa <= pc)
		retval = a;
	else if (pb <= pc)
		retval = b;

	return retval;
}

static void swap_pixels(ULONG *dst, UBYTE *src, ULONG pixels, ULONG y)
{
	ULONG filter = *src++;

	if (filter == 0 || (filter == 2 && y == 0))
	{
		do
		{
			*dst++ = src[3] << 24 | src[0] << 16 | src[1] << 8 | src[2];

			pixels--;
			src += 4;
		}
		while (pixels > 0);
	}
	else
	{
		if (filter == 1)
		{
			UBYTE a, r, g, b;

			a = 0;
			r = 0;
			g = 0;
			b = 0;

			do
			{
				a += src[3];
				r += src[0];
				g += src[1];
				b += src[2];

				pixels--;
				*dst++ = a << 24 | r << 16 | g << 8 | b;
				src += 4;
			}
			while (pixels > 0);
		}
		else if (filter == 2)
		{
			ULONG modulo = pixels * 4;

			do
			{
				UBYTE a, r, g, b, *ptr;

				ptr = (UBYTE *)dst;

				a = src[3] + ptr[0 - modulo];
				r = src[0] + ptr[1 - modulo];
				g = src[1] + ptr[2 - modulo];
				b = src[2] + ptr[3 - modulo];

				pixels--;
				*dst++ = a << 24 | r << 16 | g << 8 | b;
				src += 4;
			}
			while (pixels > 0);
		}
		else if (filter == 3)
		{
			ULONG modulo;
			UBYTE a, r, g, b;

			a = 0;
			r = 0;
			g = 0;
			b = 0;
			modulo = pixels * 4;

			do
			{
				ULONG ta, tr, tg, tb;
				UBYTE *ptr = (UBYTE *)dst;

				ta = a;
				tr = r;
				tg = g;
				tb = b;

				if (y > 0)
				{
					ta += ptr[0 - modulo];
					tr += ptr[1 - modulo];
					tg += ptr[2 - modulo];
					tb += ptr[3 - modulo];
				}

				a = ta / 2;
				r = tr / 2;
				g = tg / 2;
				b = tb / 2;

				a += src[3];
				r += src[0];
				g += src[1];
				b += src[2];

				pixels--;
				*dst++ = a << 24 | r << 16 | g << 8 | b;
				src += 4;
			}
			while (pixels > 0);
		}
		else if (filter == 4)
		{
			ULONG flag, modulo;
			UBYTE a, r, g, b;

			a = 0;
			r = 0;
			g = 0;
			b = 0;
			flag = 0;
			modulo = pixels * 4;

			do
			{
				UBYTE *ptr = (UBYTE *)dst;

				a = PaethPredictor(a, y ? ptr[0 - modulo] : 0, y && flag ? ptr[-4 - modulo] : 0);
				r = PaethPredictor(r, y ? ptr[1 - modulo] : 0, y && flag ? ptr[-3 - modulo] : 0);
				g = PaethPredictor(g, y ? ptr[2 - modulo] : 0, y && flag ? ptr[-2 - modulo] : 0);
				b = PaethPredictor(b, y ? ptr[3 - modulo] : 0, y && flag ? ptr[-1 - modulo] : 0);

				a += src[3];
				r += src[0];
				g += src[1];
				b += src[2];

				pixels--;
				*dst++ = a << 24 | r << 16 | g << 8 | b;
				src += 4;
				flag = 1;
			}
			while (pixels > 0);
		}
	}
}

APTR png_load(CONST_STRPTR filename)
{
	struct nsimage *image;
	BPTR fh;

	fh = Open(filename, MODE_OLDFILE);
	if (fh == 0)
	{
		LOG(("Failed to open PNG file %s", filename));
		return NULL;
	}
	image = NULL;

	if (fh)
	{
		struct FileInfoBlock fib;

		if (ExamineFH(fh, &fib))
		{
			ULONG *buffer, size;

			size = fib.fib_Size;

			if (size > 8 + 12 * 2 + 7)
			{
				buffer = AllocMem(size, MEMF_ANY);

				if (buffer)
				{
					ULONG *idat, width, height;

					Read(fh, buffer, size);

					width = buffer[4];
					height = buffer[5];

					idat = png_find_chunk(buffer + 2, MAKE_ID('I','D','A','T'), size - 8);

					if (idat)
					{
						z_stream strm;
						int ret;

						strm.zalloc = Z_NULL;
						strm.zfree = Z_NULL;
						strm.opaque = Z_NULL;
						strm.avail_in = 0;
						strm.next_in = 0;

						#if 1//defined(__MORPHOS__)
						ret = inflateInit2(&strm, 15);
						#else
						ret = inflateInit2_(&strm, 15, 0, 0);
						#endif

						if (ret == Z_OK)
						{
							UBYTE *outbuf;
							ULONG outsize;

							strm.avail_in = idat[0];
							strm.next_in = (APTR)(idat + 2);

							outsize = width * sizeof(ULONG) + 1;

							outbuf = AllocMem(outsize, MEMF_ANY);

							if (outbuf)
							{
								ULONG modulo;

								#if defined(__MORPHOS__)
								modulo = altivec_accelerated ? (width + 15) & ~15 : width;

								image = AllocVecAligned(modulo * sizeof(ULONG) * height + sizeof(*image), MEMF_ANY, 16, 0);
								#else
								modulo = width;

								image = AllocVec(modulo * sizeof(ULONG) * height + sizeof(*image), MEMF_ANY);
								#endif

								if (image)
								{
									ULONG y = 0, *imgbuf;

									image->width = width;
									image->height = height;
									image->modulo = modulo;
									imgbuf = image->data;

									do
									{
										strm.avail_out = outsize;
										strm.next_out = outbuf;

										ret = inflate(&strm, Z_NO_FLUSH);

										switch (ret)
										{
											default:
												if (strm.avail_out == 0)
													swap_pixels(imgbuf, outbuf, width, y++);

												imgbuf += modulo;
												break;

											case Z_NEED_DICT:
											case Z_DATA_ERROR:
											case Z_MEM_ERROR:
											case Z_STREAM_ERROR:
												inflateEnd(&strm);
												strm.avail_out = 1;
												break;
										}
									}
									while (ret != Z_STREAM_END || strm.avail_out == 0);
								}

								FreeMem(outbuf, outsize);
							}

							inflateEnd(&strm);
						}
					}

					FreeMem(buffer, size);
				}
			}
		}

		Close(fh);
	}

	return image;
}
