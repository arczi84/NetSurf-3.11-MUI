/*
** $VER: TransferAnim_mcc.h V0.0 (17-Mar-99)
**	Copytight © 1999 by Linus McCabe.
** All rights reserved.
*/

#ifndef	TransferAnim_MCC_H
#define	TransferAnim_MCC_H

#ifndef	EXEC_TYPES_H
#include	<exec/types.h>
#endif

#define MUIC_TransferAnim "TransferAnim.mcc"
#define MUIC_TransferAnimP "TransferAnim.mcp"
#define TransferAnimObject MUI_NewObject(MUIC_TransferAnim
#define BoingObject MUI_NewObject(MUIC_TransferAnim, MUIA_TransferAnim_BoingBall, TRUE, End

/* Attributes */

#define MUIA_TransferAnim_File					0xfad10001		/*(ISG)*/
#define MUIA_TransferAnim_Precision				0xfad10002		/*(I..)*/
#define MUIA_TransferAnim_Height					0xfad10003		/*(I.G)*/
#define MUIA_TransferAnim_Width					0xfad10004		/*(I.G)*/
#define MUIA_TransferAnim_FPS						0xfad10005		/*(ISG)*/
#define MUIA_TransferAnim_Millis					0xfad10006		/*(ISG)*/
#define MUIA_TransferAnim_StartFrame			0xfad10007		/*(ISG)*/
#define MUIA_TransferAnim_Delay					0xfad10008		/*(ISG)*/
#define MUIA_TransferAnim_NoLoop					0xfad10009		/*(ISG)*/
#define MUIA_TransferAnim_Bitmap					0xfad1000a		/*(..G)*/
#define MUIA_TransferAnim_BoingBall				0xfad1000b		/*(I..)*/
#define MUIA_TransferAnim_Frames					0x0fad100c		/*(ISG)*/
#define MUIA_TransferAnim_DisabledFrame		0x0fad100d		/*(IS.)*/
#define MUIA_TransferAnim_SelectedFrame		0x0fad100e		/*(IS.)*/
#define MUIA_TransferAnim_Changes				0x0fad100f		/*(I..)*/
#define MUIA_TransferAnim_NoAnim					0xfad10011		/*(ISG)*/
#define MUIA_TransferAnim_Frame					0xfad10012		/*(ISG)*/
#define MUIA_TransferAnim_ViewMode				0xfad10014		/*(ISG)*/
#define MUIA_TransferAnim_Depth					0xfad10015		/*(..G)*/
#define MUIA_TransferAnim_DTObj					0xfad10016		/*(..G)*/
#define MUIA_TransferAnim_Error					0xfad10017		/*(..G)*/
#define MUIA_TransferAnim_MaxWidth				0xfad10018		/*(..G)*/
#define MUIA_TransferAnim_ObjWidth				0xfad10010		/*(I..)*/
#define MUIA_TransferAnim_ObjHeight				0xfad10013		/*(I..)*/
#define MUIA_TransferAnim_DefObjWidth			0xfad10019		/*(I..)*/
#define MUIA_TransferAnim_DefObjHeight			0xfad1001a		/*(I..)*/


/* Methods */

#define MUIM_TransferAnim_StartAnim	0xfad10601
#define MUIM_TransferAnim_StopAnim				0xfad10602
#define MUIM_TransferAnim_NoEvent				0xfad10603

#define MUIV_TransferAnim_Normal					0xfad10a01
#define MUIV_TransferAnim_Scale					0xfad10a02
#define MUIV_TransferAnim_Repeat					0xfad10a03



#endif	/* TransferAnim_MCC_H */
