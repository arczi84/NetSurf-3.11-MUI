#include <exec/exec.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
//#include <timeradd.h>
#include "utils/sys_time.h"

#define timerisset(tvp) ((tvp)->tv_sec != 0 || (tvp)->tv_usec != 0)
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)


long long int strtoll(const char *nptr, char **endptr, int base)
{
	return (long long int)strtol(nptr, endptr, base);
}

ULONG __slab_max_size = 2048; /* Enable clib2's slab allocator */

