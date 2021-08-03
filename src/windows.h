/***************************************************************************
                          windows.h  -  description
                             -------------------
    begin                : Tue Mar 21 2002
    copyright            : (C) 2001 by Michael Speck
    email                : kulkanie@gmx.net
 ***************************************************************************/
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

#ifndef __WINDOWS_H
#define __WINDOWS_H

#include "image.h"

enum {
    ARRANGE_COLUMNS = 0,
    ARRANGE_ROWS,
    ARRANGE_INSIDE
};

/*
====================================================================
�����. ����� � ������������ �� ��� �������.
====================================================================
*/
typedef struct {
    Frame *frame;
    Font  *def_font;
} Label;

/*
====================================================================
�������� �������� � �����.
�� ��������� ����� ������ � �����.
������� ������� ��������� - x, y � "surf".
====================================================================
*/
Label *label_create( SDL_Surface *frame, int alpha, Font *def_font, SDL_Surface *surf, int x, int y );
void label_delete( Label **label );

/*
====================================================================
���������� ��������
====================================================================
*/
#define label_hide( label, hide ) buffer_hide( label->frame->img->bkgnd, hide )
#define label_get_bkgnd( label ) buffer_get( label->frame->img->bkgnd )
#define label_draw_bkgnd( label ) buffer_draw( label->frame->img->bkgnd )
#define label_draw( label ) frame_draw( label->frame )

/*
====================================================================
�������� ��������� ������
====================================================================
*/
#define label_move( label, x, y ) buffer_move( label->frame->img->bkgnd, x, y )
#define label_set_surface( label, surf ) buffer_set_surface( label->frame->img->bkgnd, surf )

/*
====================================================================
�������� ����� ��� ���������� ����� � ���������� hide = 0.
���� 'font' ����� NULL, ������������ ��������� ������ �� ���������.
====================================================================
*/
void label_write( Label *label, Font *font, const char *text );

/*
====================================================================
������ ������. ����� � ����������� ��������.
====================================================================
*/
typedef struct {
    SDL_Rect button_rect;   /* ������� �� ����������� ������ */
    SDL_Rect surf_rect;     /* ������ � buffer::surface */
    int id;                 /* ���� ������������ ��� ������� */
    char tooltip_left[32];  /* ������������ ��� ������� ������� */
    char tooltip_center[32];/* ������������ ��� ������� ������� */
    char tooltip_right[32]; /* ������������ ��� ������� ������� */
    char **lock_info;       /* ���������� ���������� ��� ������ ������������ */
    int active;             /* ������, ���� ������ ����� ���� ������������ */
    int down;               /* ���������������� ����� - ������ ��������������� ������� */
    int lock;               /* ����������� ������, ����� ��������� */
    int states;             /* ���������� ��������� ��������� ��� ���� ������; 2 ������ ��������� �������� �������������� */
    int hidden;             /* ��� ������ ������ � ��������� � ���� */
} Button;
typedef struct {
    Frame *frame;       /* ���� ������ */
    SDL_Surface *img;   /* ����������� ������. ������ - ��� ������, ������� - ��� ��������� */
    int w, h;           /* ������ ������ */
    Button *buttons;
    int button_count;   /* ���������� ������ */
    int button_limit;   /* ������ ������ */
    int base_id;
} Group;

/*
====================================================================
�������� ������ ������. �� ������������ ������ ����� ���� ��������� ������ �limit�.
����������� ��������� ������ ������������ � ������.
����������� ������������� ������ ����������� ��� base_id + id.
====================================================================
*/
Group *group_create( SDL_Surface *frame, int alpha, SDL_Surface *img, int w, int h, int limit, int base_id,
                     SDL_Surface *surf, int x, int y );
void group_delete( Group **group );

/*
====================================================================
�������� ������ � ����� x, y � ������. ���� ���������� lock_info, ��� ������ �������� ��������������.
'id' * group :: h - �������� ������ �� ��� y.
====================================================================
*/
int group_add_button( Group *group, int id, int x, int y, char **lock_info, const char *tooltip, int states );

/*
====================================================================
�������� ������ � ����� x, y � ������. ���� ���������� lock_info, ��� ������ �������� ��������������.
'icon_id' ������������ ��� ������ ������ ������ 'id' (���������
��������� ������ ������ ������)
====================================================================
*/
int group_add_button_complex( Group *group, int id, int icon_id, int x, int y,
                              char **lock_info, const char *tooltip, int states );

/*
====================================================================
�������� ������ �� ����������� ��������������.
====================================================================
*/
Button* group_get_button( Group *group, int id );

/*
====================================================================
���������� �������� ��������� ������ �� ����������� ��������������.
====================================================================
*/
void group_set_active( Group *group, int id, int active );

/*
====================================================================
���������� ������� ��������� ������ �� ����������� ��������������.
====================================================================
*/
void group_set_hidden( Group *group, int id, int hidden );

/*
====================================================================
������ ����������.
====================================================================
*/
void group_lock_button( Group *group, int id, int down );

/*
====================================================================
��������� ������� �������� � �������� ������ � �����.
������� True, ���� � ���� �����.
====================================================================
*/
int group_handle_motion( Group *group, int x, int y );
/*
====================================================================
��������� ������� �����. ������� True, ���� ���� ������ ������, � ���������
������.
====================================================================
*/
int group_handle_button( Group *group, int button_id, int x, int y, Button **button );

/*
====================================================================
��������� ������.
====================================================================
*/
#define group_hide( group, hide ) buffer_hide( group->frame->img->bkgnd, hide )
#define group_get_bkgnd( group ) buffer_get( group->frame->img->bkgnd )
#define group_draw_bkgnd( group ) buffer_draw( group->frame->img->bkgnd )
void group_draw( Group *group );

/*
====================================================================
�������� / �������� ���������.
====================================================================
*/
#define group_set_surface( group, surf ) buffer_set_surface( group->frame->img->bkgnd, surf )
#define group_get_geometry(group,x,y,w,h) buffer_get_geometry(group->frame->img->bkgnd,x,y,w,h)
void group_move( Group *group, int x, int y );
#define group_get_width( group ) frame_get_width( (group)->frame )
#define group_get_height( group ) frame_get_height( (group)->frame )

/*
====================================================================
������� True, ���� x, y ��������� �� ��������� ������.
====================================================================
*/
int button_focus( Button *button, int x, int y );


/*
====================================================================
������������� ��������.
====================================================================
*/
typedef struct {
    Label *label;
    char  *text;    /* ��������� ����� */
    int   limit;    /* ������������ ������ ������ */
    int   blink;    /* ���� ������������ �������� ������ (������) */
    int   blink_time; /* ������������ ��� ������������ ������� */
    int   cursor_pos; /* ������� � ������ */
    int   cursor_x;   /* ������� �� �������� */
    int   last_sym;   /* keysym, ���� ��������� ���� (��� -1, ���� ��� ���) */
    int   last_mod;   /* ����� ������������� ��������� ������� */
    int   last_uni;   /* ������ */
    int   delay;      /* �������� ����� ��������� ��������� ��� ������� ������� */
} Edit;

/*
====================================================================
������� �������������.
====================================================================
*/
Edit *edit_create( SDL_Surface *frame, int alpha, Font *font, int limit, SDL_Surface *surf, int x, int y );
void edit_delete( Edit **edit );

/*
====================================================================
���������� ��������
====================================================================
*/
#define edit_hide( edit, hide ) buffer_hide( edit->label->frame->img->bkgnd, hide )
#define edit_get_bkgnd( edit ) buffer_get( edit->label->frame->img->bkgnd )
#define edit_draw_bkgnd( edit ) buffer_draw( edit->label->frame->img->bkgnd )
void edit_draw( Edit *edit );

/*
====================================================================
�������� ��������� ������
====================================================================
*/
#define edit_move( edit, x, y ) buffer_move( edit->label->frame->img->bkgnd, x, y )
#define edit_set_surface( edit, surf ) buffer_set_surface( edit->label->frame->img->bkgnd, surf )

/*
====================================================================
���������� ����� � ������������� ������ x. ���� 'newtext' ����� NULL, �������
������������ �����.
====================================================================
*/
void edit_show( Edit *edit, const char *newtext );

/*
====================================================================
�������� ��������� ����� � ������������ � ��������, �������� ����� � �������������.
====================================================================
*/
void edit_handle_key( Edit *edit, int keysym, int modifier, int unicode );

/*
====================================================================
�������� �������� ������ � �������� ������� ��� �������. ������� True, ����
���� ��� �������.
====================================================================
*/
int edit_update( Edit *edit, int ms );


/*
====================================================================
������
====================================================================
*/
typedef struct _LBox {
    Group *group;       /* ������� ����� + ������ ����� / ���� */
    /* ����� */
    List *items;        /* ������ ��������� */
    void *cur_item;     /* ������� ���������� ������� */
    /* ������ */
    int step;           /* ������� �� ��� ���������� ������, ���� ������ ����� / ���� */
    int cell_offset;    /* ������������� ������� ������������� �������� */
    int cell_count;     /* ���������� ����� */
    int cell_x, cell_y; /* ���������� ������� ������ ������ */
    int cell_w, cell_h; /* ������ ������ */
    int cell_gap;       /* ���������� ����� �������� (������������) */
    int cell_color;     /* ���� ��������� */
    SDL_Surface *cell_buffer; /* �����, ������������ ��� ���������� ����� ������ */
    void  (*render_cb)( void*, SDL_Surface* ); /* ��������� ������� �� ����������� */
} LBox;

/*
====================================================================
�������� ������ � ����������� ��������. ������ ���������
�� ��������� NULL.
====================================================================
*/
LBox *lbox_create( SDL_Surface *frame, int alpha, int border, SDL_Surface *buttons, int button_w, int button_h,
                   int cell_count, int step, int cell_w, int cell_h, int cell_gap, int cell_color,
                   void (*cb)(void*, SDL_Surface*),
                   SDL_Surface *surf, int x, int y );
void lbox_delete( LBox **lbox );

/*
====================================================================
������ ���������
====================================================================
*/
#define lbox_hide( lbox, hide ) buffer_hide( lbox->group->frame->img->bkgnd, hide )
#define lbox_get_bkgnd( lbox ) buffer_get( lbox->group->frame->img->bkgnd )
#define lbox_draw_bkgnd( lbox ) buffer_draw( lbox->group->frame->img->bkgnd )
#define lbox_draw( lbox ) group_draw( lbox->group )

/*
====================================================================
�������� / �������� ��������� ������
====================================================================
*/
#define lbox_set_surface( lbox, surf ) buffer_set_surface( lbox->group->frame->img->bkgnd, surf )
#define lbox_move( lbox, x, y ) group_move( lbox->group, x, y )
#define lbox_get_width( lbox ) group_get_width( (lbox)->group )
#define lbox_get_height( lbox ) group_get_height( (lbox)->group )
#define lbox_get_selected_item( lbox ) ( (lbox)->cur_item )
#define lbox_get_selected_item_index( lbox ) \
	(list_check( (lbox)->items, (lbox)->cur_item ))
#define lbox_clear_selected_item( lbox ) \
	do { (lbox)->cur_item = 0; lbox_apply(lbox); } while (0)
void *lbox_select_first_item( LBox *lbox );
#define lbox_is_empty( lbox ) \
	((lbox)->items == NULL || (lbox)->items->count == 0)
#define lbox_select_item(lbox, item) \
	do { (lbox)->cur_item = (item); lbox_apply(lbox); } while (0)

/*
====================================================================
������������ ������� ������ (lbox->group->frame).
====================================================================
*/
void lbox_apply( LBox *lbox );

/*
====================================================================
������� ������ ������ ��������� (���� ����) � ����������� ����� (�����
������� ���� �������)
====================================================================
*/
void lbox_set_items( LBox *lbox, List *items );
#define lbox_clear_items( lbox ) lbox_set_items( lbox, NULL )

/*
====================================================================
handle_motion ������������� ������, ���� ����� / ���� ��������� �����, � ���������� �������
���� ������� ���� �������.
====================================================================
*/
int lbox_handle_motion( LBox *lbox, int cx, int cy, void **item );
/*
====================================================================
handle_button ��������� ������������ ������ ����� / ���� � ���������� �������
���� ���� ��� ������.
====================================================================
*/
int lbox_handle_button( LBox *lbox, int button_id, int cx, int cy, Button **button, void **item );

/** Render listbox item @item (which is a simple string) to listbox cell
 * surface @buffer. */
void lbox_render_text( void *item, SDL_Surface *buffer );


/*
====================================================================
�������� ������ � �������� ��� ����������� ���������� � �����
(��������, ���������� � ��������)
====================================================================
*/
typedef struct {
    LBox *lbox;                         /* ������ ������ */
    Group *group;                       /* �������������� ����� � �������� �� / ������ */
    char root[1024];                    /* �������� ���������� */
    char subdir[1024];                  /* ������� ������� � �������� �������� */
    void (*file_cb)( const char*, const char*, SDL_Surface* );
                                        /* ����������, ���� ���� ��� ������
                                               ��� ������ ���� */
    int info_x, info_y;                 /* �������������� ������� */
    SDL_Surface *info_buffer;           /* ����� ���������� */
    int button_x, button_y;             /* ��������� ������ ������ (������ ������� ����,
                                           ����� ��������� ����� ������) */
    int button_dist;                    /* ���������� ����� �������� (���������� + ������) */
    int emptyFile;                      /* ���������, ������� �� ���� �������� ������ ������ ���� */
    int arrangement;                    /* ������������ ���� ����
                                           0: ����������� ����� ���� � ������
                                           1: ����������� ���� ��� ������ */
    int dir_only;                       /* ���� �������� ������ ���� ������ ����� */
    char *current_name;                 /* ������� ��������� ���� */
    int file_type;                      /* �������� ������������ ��� ����� � ���� ���������� ���� ����� */
} FDlg;

/*
====================================================================
�������� ����-������. �� ��������� ������ ����.
������ ��� ������ � conf_buttons ����������� ��� ok � cancel.
��� ���� ��������� ������ � conf_buttons ���� �������
���������������, � ��� ������ ����� ���� ��������� � ������� fdlg_add_button ().
====================================================================
*/
FDlg *fdlg_create(
                   SDL_Surface *lbox_frame, int alpha, int border,
                   SDL_Surface *lbox_buttons, int lbox_button_w, int lbox_button_h,
                   int cell_h,
                   SDL_Surface *conf_frame,
                   SDL_Surface *conf_buttons, int conf_button_w, int conf_button_h,
                   int id_ok,
                   void (*lbox_cb)( void*, SDL_Surface* ),
                   void (*file_cb)( const char*, const char*, SDL_Surface* ),
                   SDL_Surface *surf, int x, int y, int arrangement, int emptyFile, int dir_only, int file_type );
void fdlg_delete( FDlg **fdlg );

/*
====================================================================
������ ��������� �����
====================================================================
*/
void fdlg_hide( FDlg *fdlg, int hide );
void fdlg_get_bkgnd( FDlg *fdlg );
void fdlg_draw_bkgnd( FDlg *fdlg );
void fdlg_draw( FDlg *fdlg );

/*
====================================================================
�������� ��������� ����������� ���� �����
====================================================================
*/
void fdlg_set_surface( FDlg *fdlg, SDL_Surface *surf );
void fdlg_move( FDlg *fdlg, int x, int y );

/*
====================================================================
�������� ������. ������� ����� �� conf_buttons.
====================================================================
*/
void fdlg_add_button( FDlg *fdlg, int id, char **lock_info, const char *tooltip );

/*
====================================================================
�������� ������ � ������ � ����� ��������.
====================================================================
*/
void fdlg_open( FDlg *fdlg, const char *root, const char *subdir );

/*
====================================================================
handle_motion ��������� ����� ������
====================================================================
*/
int fdlg_handle_motion( FDlg *fdlg, int cx, int cy );
/*
====================================================================
handle_button
====================================================================
*/
int fdlg_handle_button( FDlg *fdlg, int button_id, int cx, int cy, Button **button );


/*
====================================================================
������ ���������
====================================================================
*/
typedef struct {
    LBox *list;
    Group *ctrl;
    Group *module;
    Group *confirm;
    void (*select_cb)(void*); /* ������� ��� ������ �������� */
    int sel_id; /* id ��������� ������ */
} SDlg;

/*
====================================================================
�������� ������ ���������.
====================================================================
*/
SDlg *sdlg_create( SDL_Surface *list_frame, SDL_Surface *list_buttons,
                   int list_button_w, int list_button_h, int cell_h,
                   SDL_Surface *ctrl_frame, SDL_Surface *ctrl_buttons,
                   int ctrl_button_w, int ctrl_button_h, int id_ctrl,
                   SDL_Surface *mod_frame, SDL_Surface *mod_buttons,
                   int mod_button_w, int mod_button_h, int id_mod,
                   SDL_Surface *conf_frame, SDL_Surface *conf_buttons,
                   int conf_button_w, int conf_button_h, int id_conf,
                   void (*list_render_cb)(void*,SDL_Surface*),
                   void (*list_select_cb)(void*),
                   SDL_Surface *surf, int x, int y );
SDlg *sdlg_camp_create( SDL_Surface *conf_frame, SDL_Surface *conf_buttons,
                   int conf_button_w, int conf_button_h, int id_conf,
                   void (*list_render_cb)(void*,SDL_Surface*),
                   void (*list_select_cb)(void*),
                   SDL_Surface *surf, int x, int y );
void sdlg_delete( SDlg **sdlg );

/*
====================================================================
��������� ������ ���������.
====================================================================
*/
void sdlg_hide( SDlg *sdlg, int hide );
void sdlg_get_bkgnd( SDlg *sdlg );
void sdlg_draw_bkgnd( SDlg *sdlg );
void sdlg_draw( SDlg *sdlg );

/*
====================================================================
������ ��������� ��������
====================================================================
*/
void sdlg_set_surface( SDlg *sdlg, SDL_Surface *surf );
void sdlg_move( SDlg *sdlg, int x, int y );

/*
====================================================================
handle_motion ��������� ����� ������
====================================================================
*/
int sdlg_handle_motion( SDlg *sdlg, int cx, int cy );
/*
====================================================================
handle_button
====================================================================
*/
int sdlg_handle_button( SDlg *sdlg, int button_id, int cx, int cy, Button **button );

/*
====================================================================
������ ������:
������ ��� ������ � ������� ������ �� / ������
====================================================================
*/
typedef struct {
	Group *button_group; /* ������ �� / ������ */
	LBox *select_lbox;
} SelectDlg;

SelectDlg *select_dlg_create(
	SDL_Surface *lbox_frame,
	SDL_Surface *lbox_buttons, int lbox_button_w, int lbox_button_h,
	int lbox_cell_count, int lbox_cell_w, int lbox_cell_h,
	void (*lbox_render_cb)(void*, SDL_Surface*),
	SDL_Surface *conf_frame,
	SDL_Surface *conf_buttons, int conf_button_w, int conf_button_h,
	int id_ok,
	SDL_Surface *surf, int x, int y );
void select_dlg_delete( SelectDlg **sdlg );
int select_dlg_get_width(SelectDlg *sdlg);
int select_dlg_get_height(SelectDlg *sdlg);
void select_dlg_move( SelectDlg *sdlg, int px, int py);
void select_dlg_hide( SelectDlg *sdlg, int value);
void select_dlg_draw( SelectDlg *sdlg);
void select_dlg_draw_bkgnd( SelectDlg *sdlg);
void select_dlg_get_bkgnd( SelectDlg *sdlg);
int select_dlg_handle_motion( SelectDlg *sdlg, int cx, int cy);
int select_dlg_handle_button( SelectDlg *sdlg, int bid, int cx, int cy,
	Button **pbtn );
#define select_dlg_set_items( sdlg, items ) \
	lbox_set_items( sdlg->select_lbox, items )
#define select_dlg_clear_items( sdlg ) \
	lbox_clear_items( sdlg->select_lbox )
#define select_dlg_get_selected_item( sdlg ) \
	lbox_get_selected_item( sdlg->select_lbox )
#define select_dlg_get_selected_item_index( sdlg ) \
	lbox_get_selected_item_index( sdlg->select_lbox )

#endif
