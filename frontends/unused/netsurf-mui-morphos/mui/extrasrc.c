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

#include <proto/cybergraphics.h>
#include <proto/exec.h>

VOID NewRawDoFmt(CONST_STRPTR format, APTR func, STRPTR buf, ...)
{
	if (func == NULL)
	{
		va_list va;

		va_start(va, buf);
		vsprintf(buf, format, va);
		va_end(va);
	}
}

STATIC LONG do_alpha(LONG a, LONG v) 
{
	LONG tmp  = (a*v);
	return ((tmp<<8) + tmp + 32768)>>16;
}

VOID WritePixelArrayAlpha(APTR src, UWORD srcx, UWORD srcy, UWORD srcmod, struct RastPort *rp, UWORD destx, UWORD desty, UWORD width, UWORD height, ULONG globalalpha)
{
	if (width > 0 && height > 0)
	{
		ULONG *buf = AllocMem(width * sizeof(ULONG), MEMF_ANY);

		if (buf)
		{
			ULONG x, y;

			/* Incorrect but cant bother with alpha channel math for now */
			globalalpha = 255 - (globalalpha >> 24);

			for (y = 0; y < height; y++)
			{
				ULONG *pix;

				ReadPixelArray(buf, 0, 0, width * sizeof(ULONG), rp, destx, desty + y, width, 1, RECTFMT_ARGB);

				pix = (ULONG *)((ULONG)src + (srcy + y) * srcmod + srcx * sizeof(ULONG));

				for (x = 0; x < width; x++)
				{
					ULONG srcpix, dstpix, a, r, g, b;

					srcpix = *pix++;
					dstpix = buf[x];

					a = (srcpix >> 24) & 0xff;
					r = (srcpix >> 16) & 0xff;
					g = (srcpix >> 8) & 0xff;
					b = (srcpix >> 0) & 0xff;

					a = a - globalalpha;  

					if (a > 0)
					{
						ULONG dest_r, dest_g, dest_b;

						dest_r = (dstpix >> 16) & 0xff;
						dest_g = (dstpix >> 8) & 0xff;
						dest_b = (dstpix >> 0) & 0xff;

						dest_r += do_alpha(a, r - dest_r);
						dest_g += do_alpha(a, g - dest_g);
						dest_b += do_alpha(a, b - dest_b);

						dstpix = 0xff000000 | r << 16 | g << 8 | b;
					}

					buf[x] = dstpix;
				}

				WritePixelArray(buf, 0, 0, width * sizeof(ULONG), rp, destx, desty + y, width, 1, RECTFMT_ARGB);
			}

			FreeMem(buf, width * sizeof(ULONG));
		}
	}
}      

ULONG DoSuperNew(struct IClass *cl, APTR obj, ULONG tag1, ...)
{
	return ((APTR)DoSuperMethod(cl, obj, OM_NEW, &tag1, NULL));
}

ULONG UTF8_Decode(CONST_STRPTR s, WCHAR *uchar)
{
	ULONG octets=0;
	WCHAR ucs4=0;
	CONST UBYTE *sb = (UBYTE *)s;

	if (*sb == 0)
	{
		ucs4 = 0;
		octets = 0;
	}
	else if (*sb <= 127)
	{
		ucs4 = *sb++;
		octets = 1;
	}
	else if (*sb <= 223)
	{
		ucs4 = (*sb++ - 192)*64;
		ucs4 += (*sb++ - 128);
		octets = 2;
	}
	else if (*sb <= 239)
	{
		ucs4 = (*sb++ - 192)*4096;
		ucs4 += (*sb++ - 128)*64;
		ucs4 += (*sb++ - 128);
		octets = 3; 
	}
	else if (*sb <= 247)
	{
		ucs4 = (*sb++ - 192)*262144;
		ucs4 += (*sb++ - 128)*4096;
		ucs4 += (*sb++ - 128)*64;
		ucs4 += (*sb++ - 128);
		octets = 4; 
	}

	if (uchar)
		*uchar = ucs4;

	return octets;
}

/* If ACT is not NULL, change the action for SIG to *ACT.
   If OACT is not NULL, put the old action for SIG in *OACT.  */
#if defined(__libnix__) && !defined(__MORPHOS__)
int
sigaction (sig, act, oact)
     int sig;
     const struct sigaction *act;
     struct sigaction *oact;
{
  if (sig <= 0 || sig >= NSIG)
    {
      __set_errno (EINVAL);
      return -1;
    }

  __set_errno (ENOSYS);
  return -1;
}



 int
 __set_errno(int error)
     {
     	if (error == -1)
     		error = EINTR;
     	errno = error;
     	return (error);
     }
     


/* Perform file control operations on FD.  */ 
int
fcntl (fd, cmd)
     int fd;
     int cmd;
{
  if (fd < 0)
    {
      __set_errno (EBADF);
      return -1;
      
    }
printf("AA, fcntl");
  __set_errno (ENOSYS);
  return -1;
}

char *strndup(const char *s,size_t n)
{
	char *res;
	size_t len = strlen(s);

	if(n<len) len=n;
		
	res = malloc(len+1);
	if(!res)
		return(0);

	res[len] = '\0';

	return memcpy(res,s,len);
}

uid_t geteuid(void)
{
	return 0;
}

uid_t getuid(void)
{
	return 0;
}

gid_t getgid(void)
{
	return 0;
}

gid_t getegid(void)
{
	return 0;
}

int tcsetattr(int fildes, int optional_actions, const struct termios *termios_p)
{
	return 0;
}

int tcgetattr(int fildes, struct termios *termios_p)
{
	return 0;
}
#endif
