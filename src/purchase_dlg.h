/***************************************************************************
                     Modifications by LGD team 2012+.
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __PURCHASEDLG_H
#define __PURCHASEDLG_H

/** Диалог для покупки юнитов или возврата купленных подкреплений. */

typedef struct {
	Group *main_group; /* основная рамка с кнопками покупки / выхода */
	Frame *ulimit_frame; /* информационная рамка с ограничением юнита */
	LBox *nation_lbox; /* нации текущего игрока */
	LBox *uclass_lbox; /* классы юнита */
	LBox *unit_lbox; /* покупаемые единицы в выбранном классе */
	LBox *trsp_lbox; /* покупные транспортеры для выбранной единицы */
	LBox *reinf_lbox; /* купленные, но еще не развернутые единицы */

	Nation *cur_nation; /* указатель в глобальных странах */
	Unit_Class *cur_uclass; /* указатель в глобальных unit_classes */
	Unit_Class *trsp_uclass; /* указатель в глобальных классах юнитов */
    int cur_core_unit_limit; /* количество основных юнитов, которые можно приобрести */
	int cur_unit_limit; /* количество юнитов, которые можно приобрести */
} PurchaseDlg;

/** Интерфейс диалога покупки, подробные комментарии см. В файле C. */

PurchaseDlg *purchase_dlg_create( char *theme_path );
void purchase_dlg_delete( PurchaseDlg **pdlg );

int purchase_dlg_get_width(PurchaseDlg *pdlg);
int purchase_dlg_get_height(PurchaseDlg *pdlg);

void purchase_dlg_move( PurchaseDlg *pdlg, int px, int py);
void purchase_dlg_hide( PurchaseDlg *pdlg, int value);
void purchase_dlg_draw( PurchaseDlg *pdlg);
void purchase_dlg_draw_bkgnd( PurchaseDlg *pdlg);
void purchase_dlg_get_bkgnd( PurchaseDlg *pdlg);

int purchase_dlg_handle_motion( PurchaseDlg *pdlg, int cx, int cy);
int purchase_dlg_handle_button( PurchaseDlg *pdlg, int bid, int cx, int cy,
	Button **pbtn );

void purchase_dlg_reset( PurchaseDlg *pdlg );

#endif
