#ifndef CLIB_ASYNCIO_PROTOS_H
#define CLIB_ASYNCIO_PROTOS_H

#ifndef LIBRARIES_ASYNCIO_H
#include <libraries/asyncio.h>
#endif


/*****************************************************************************/


#ifdef _DCC

#define _REG(x)	__ ## x
#define _ASM
#define  __regargs

#else

#ifdef __GNUC__


#define _REG(x)
#define _ASM
//#define    __regargs
#define __reg(reg,arg) arg __asm(#reg)
#else

#ifdef __STORM__

#define _REG(x)	register __ ## x
#define _ASM
#define 

#else /* __SASC__ */

#define _REG(x)	register __ ## x
#define _ASM	__asm
#define    __regargs

#endif /* __STORM__ */

#endif /* __GNUC__ */

#endif /* _DCC */


#ifndef ASIO_SHARED_LIB
#ifndef ASIO_REGARGS
#undef _REG
#define _REG(x)
#undef _ASM
#define _ASM

#ifdef _DCC
#undef 
#define  __stkargs
#else
#ifdef __GNUC__

#else
#ifdef __STORM__

#else /* __SASC__ */
//#undef 
#define  __stdargs
#endif /* __STORM__ */
#endif /* __GNUC__ */
#endif /* _DCC */
#endif /* ASIO_REGARGS */

#endif /* ASIO_REGARGS */


/*****************************************************************************/


#ifdef ASIO_NOEXTERNALS
_ASM  AsyncFile *OpenAsync( _REG( a0 ) const STRPTR fileName,_REG( d0 ) OpenModes mode, _REG( d1 ) LONG bufferSize, _REG( a1 ) struct ExecBase *SysBase, _REG( a2 ) struct DosLibrary *DOSBase );
_ASM  AsyncFile *OpenAsyncFromFH( _REG( a0 ) BPTR handle, _REG( d0 ) OpenModes mode, _REG( d1 ) LONG bufferSize, _REG( a1 ) struct ExecBase *SysBase, _REG( a2 ) struct DosLibrary *DOSBase );
#else
_ASM  AsyncFile *OpenAsync( _REG( a0 ) const STRPTR fileName,_REG( d0 ) OpenModes mode, _REG( d1 ) LONG bufferSize );
_ASM  AsyncFile *OpenAsyncFromFH( _REG( a0 ) BPTR handle, _REG( d0 ) OpenModes mode, _REG( d1 ) LONG bufferSize );
#endif
_ASM  LONG       CloseAsync( _REG( a0 ) AsyncFile *file );
_ASM  LONG       PeekAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) APTR buffer, _REG( d0 ) LONG numBytes );
_ASM  LONG       ReadAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) APTR buffer, _REG( d0 ) LONG numBytes );
_ASM  LONG       ReadCharAsync( _REG( a0 ) AsyncFile *file );
_ASM  LONG       ReadLineAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) APTR buffer, _REG( d0 ) LONG size );
_ASM  APTR       FGetsAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) APTR buffer, _REG( d0 ) LONG size );
_ASM  APTR       FGetsLenAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) APTR buffer, _REG( d0 ) LONG size, _REG( a2 ) LONG *length );
_ASM  LONG       WriteAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) APTR buffer, _REG( d0 ) LONG numBytes );
_ASM  LONG       WriteCharAsync( _REG( a0 ) AsyncFile *file, _REG( d0 ) UBYTE ch );
_ASM  LONG       WriteLineAsync( _REG( a0 ) AsyncFile *file, _REG( a1 ) STRPTR line );
_ASM  LONG       SeekAsync( _REG( a0 ) AsyncFile *file, _REG( d0 ) LONG position, _REG( d1 ) SeekModes mode);

#endif /* CLIB_LIBRARIES_ASYNCIO_H */
