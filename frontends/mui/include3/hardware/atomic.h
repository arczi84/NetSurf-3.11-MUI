#if defined(__MORPHOS__)
#include_next <hardware/atomic.h>
#else
#include <proto/exec.h>

static inline VOID _ATOMIC_ADD(LONG *ptr, LONG add)
{
	LONG val;
	Forbid();
	val = *ptr;
	val += add;
	*ptr = val;
	Permit();
}

#define ATOMIC_ADD(ptr,val) _ATOMIC_ADD(ptr,val)
#define ATOMIC_SUB(ptr,val) _ATOMIC_ADD(ptr,-(val))

#endif
