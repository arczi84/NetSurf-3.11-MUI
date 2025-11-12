#ifndef MUI_EXTRASRC_H
#define MUI_EXTRASRC_H

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

#if !defined(__MORPHOS__)

#include <libraries/mui.h>
#ifdef TTENGINE
#include <libraries/ttengine.h>
#endif
#include <utility/tagitem.h>
#include <exec/types.h>

struct RastPort;


#ifndef MUI_EHF_GUIMODE
#define MUI_EHF_GUIMODE (1 << 1)
#endif

#define WA_PointerType (TAG_USER + 0)

#define POINTERTYPE_NORMAL    0
#define POINTERTYPE_LINK      1
#define POINTERTYPE_INVISIBLE 2

#ifdef TTENGINE
#ifndef TT_Encoding_System_UTF8
#define TT_Encoding_System_UTF8 TT_Encoding_UTF8
#endif
#endif

#ifndef WCHAR_TYPEDEF
#define WCHAR_TYPEDEF
typedef int             WCHAR;
#endif

#if !defined(__AROS__)
/* Popular AROS types */
#ifndef QUAD_TYPEDEF
#define QUAD_TYPEDEF
typedef signed long long   QUAD;
#endif

#ifndef UQUAD_TYPEDEF
#define UQUAD_TYPEDEF
typedef unsigned long long UQUAD;
#endif

typedef unsigned long  IPTR;
#if 0
#define GetHead(_l)  \
({ struct List *l = (struct List *)(_l);  \
 l->lh_Head->ln_Succ ? l->lh_Head : (struct Node *)0;  \
})

#define GetTail(_l)  
({ struct List *l = (struct List *)(_l);  
 l->lh_TailPred->ln_Pred ? l->lh_TailPred : (struct Node *)0;  
})

#define GetSucc(_n)  
({ struct Node *n = (struct Node *)(_n);  
 n->ln_Succ->ln_Succ ? n->ln_Succ : (struct Node *)0;  
})

#define GetPred(_n)  
({ struct Node *n = (struct Node *)(_n);  
 n->ln_Pred->ln_Pred ? n->ln_Pred : (struct Node *)0;  
})

#define ForeachNode(l,n)  
for (  
 n = (void *)(((struct List *)(l))->lh_Head);  
 ((struct Node *)(n))->ln_Succ;  
 n = (void *)(((struct Node *)(n))->ln_Succ)  
)

#define ForeachNodeSafe(l,n,n2)  
for (  
 n = (void *)(((struct List *)(l))->lh_Head);  
 (n2 = (void *)((struct Node *)(n))->ln_Succ);  
 n = (void *)n2  
)

#define ListLength(list,count)  
do {  
 struct Node *n;  
 count = 0;  
 ForeachNode(list,n) count++;  
} while (0)
#endif
#endif

#define ForeachNode(list, node) \
    for (node = (struct Node *)((struct List *)(list))->lh_Head; \
         ((struct Node *)(node))->ln_Succ; \
         node = (struct Node *)((struct Node *)(node))->ln_Succ)

#define ForeachNodeSafe(list, node, next) \
    for (node = (struct Node *)((struct List *)(list))->lh_Head; \
         (next = (struct Node *)((struct Node *)(node))->ln_Succ); \
         node = next)


#define SetNodeName(node,name)  (((struct Node *)(node))->ln_Name = (char *)(name))
#define GetNodeName(node)       (((struct Node *)(node))->ln_Name)



VOID NewRawDoFmt(CONST_STRPTR format, APTR func, STRPTR buf, ...);
#define AllocMemAligned(size, type, align, offset) AllocMem(size, type)
#define AllocVecAligned(size, type, align, offset) AllocVec(size, type)
#define AllocTaskPooled(size)     malloc(size)
#define FreeTaskPooled(ptr, size) free(ptr)
#define AllocVecTaskPooled(size)  malloc(size)
#define FreeVecTaskPooled(ptr)    free(ptr)
ULONG UTF8_Decode(CONST_STRPTR s, WCHAR *uchar);
VOID WritePixelArrayAlpha(APTR src, UWORD srcx, UWORD srcy, UWORD srcmod,
    struct RastPort *rp, UWORD destx, UWORD desty, UWORD width,
    UWORD height, ULONG globalalpha);

//ULONG DoSuperNew(struct IClass *cl, APTR obj, ULONG tag1, ...);
Object *DoSuperNew(struct IClass *cl, Object *obj, Tag tag1, ...);
#endif

#endif /* MUI_EXTRASRC_H */
