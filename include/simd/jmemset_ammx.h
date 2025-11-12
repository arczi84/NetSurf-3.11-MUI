/*
 * jmemset_ammx.h
 *
 * Copyright 2018 Henryk Richter <henryk.richter@gmx.net>
 *
 */

#ifndef _JMEMSET_AMMX_H
#define _JMEMSET_AMMX_H

/*--------------------- MEMCPY/BZERO -------------------------------*/

void jsimd_memset_switch_ammx( register int ammxon  __asm("d0") );
void jsimd_bzero_ammx( register void *ptr  __asm("a0"),
                       register long bytes __asm("d0") );
void jsimd_memcpy_ammx( register void *dest __asm("a1"),
                        register void *src  __asm("a0"),
                        register long bytes __asm("d0") );


#endif /* _JMEMSET_AMMX_H */
