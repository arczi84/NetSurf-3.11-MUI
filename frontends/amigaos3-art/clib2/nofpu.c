#ifdef NO_FPU

#include <stdint.h>

typedef float DFtype __attribute__ ((mode (DF)));
typedef float SFtype __attribute__ ((mode (SF)));

typedef unsigned int USItype __attribute__ ((mode (SI)));
typedef int SItype __attribute__ ((mode (SI)));

DFtype __floatunsidf (USItype u);
SFtype __floatunsisf (USItype u);

DFtype
__floatunsidf (USItype u)
{
  SItype s = (SItype) u;
  DFtype r = (DFtype) s;
  if (s < 0)
    r += (DFtype)2.0 * (DFtype) ((USItype) 1
				 << (sizeof (USItype) * __CHAR_BIT__ - 1));
  return r;
}

SFtype
__floatunsisf (USItype u)
{
  SItype s = (SItype) u;
  if (s < 0)
    {
      /* As in expand_float, compute (u & 1) | (u >> 1) to ensure
	 correct rounding if a nonzero bit is shifted out.  */
      return (SFtype) 2.0 * (SFtype) (SItype) ((u & 1) | (u >> 1));
    }
  else
    return (SFtype) s;
}

#endif
