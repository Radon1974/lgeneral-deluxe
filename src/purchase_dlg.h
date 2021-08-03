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

/** ������ ��� ������� ������ ��� �������� ��������� ������������. */

typedef struct {
	Group *main_group; /* �������� ����� � �������� ������� / ������ */
	Frame *ulimit_frame; /* �������������� ����� � ������������ ����� */
	LBox *nation_lbox; /* ����� �������� ������ */
	LBox *uclass_lbox; /* ������ ����� */
	LBox *unit_lbox; /* ���������� ������� � ��������� ������ */
	LBox *trsp_lbox; /* �������� ������������ ��� ��������� ������� */
	LBox *reinf_lbox; /* ���������, �� ��� �� ����������� ������� */

	Nation *cur_nation; /* ��������� � ���������� ������� */
	Unit_Class *cur_uclass; /* ��������� � ���������� unit_classes */
	Unit_Class *trsp_uclass; /* ��������� � ���������� ������� ������ */
    int cur_core_unit_limit; /* ���������� �������� ������, ������� ����� ���������� */
	int cur_unit_limit; /* ���������� ������, ������� ����� ���������� */
} PurchaseDlg;

/** ��������� ������� �������, ��������� ����������� ��. � ����� C. */

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
