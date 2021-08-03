/***************************************************************************
                          headquartes_dlg.h  -  description
                             -------------------
    begin                : Mon Jan 20 2014
    copyright            : (C) 2014 by Andrzej Owsiejczuk
    email                : andre.owsiejczuk@wp.pl
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "lgeneral.h"
#include "sdl.h"
#include "image.h"
#include "list.h"
#include "windows.h"
#include "headquarters_dlg.h"
#include "gui.h"
#include "localize.h"
#include "file.h"
#include "engine.h"
#include "campaign.h"

extern Sdl sdl;
extern GUI *gui;
extern Player *cur_player;
extern List *units;
extern Unit_Info_Icons *unit_info_icons;
extern int nation_flag_width, nation_flag_height;
extern SDL_Surface *nation_flags;
extern Config config;
extern int camp_loaded;

/****** Частный *****/

/** Получите все юниты из списка активных юнитов, которые принадлежат текущему игроку.
 *
 * Возврат как новый список с указателями на глобальный список юнитов (не будет удален 
 * при удалении списка) */
static List *get_active_player_units( void )
{
	List *l = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
	Unit *u = NULL;
	
	if (l == NULL) {
		fprintf( stderr, tr("Out of memory\n") );
		return NULL;
	}
	
	list_reset(units);
	while ((u = list_next(units)))
		if (u->player == cur_player)
			list_add( l, u );
	return l;
}

/** Кнопки обновления и состояние информационного окна. */
static void update_headquarters_info( HeadquartersDlg *hdlg )
{
	SDL_Surface *contents = hdlg->main_group->frame->contents;
	
	SDL_FillRect( contents, 0, 0x0 );
	
	/* если выбран юнит, то возможен центр */
	if ( lbox_get_selected_item( hdlg->units_lbox ) != 0 )
		group_set_active( hdlg->main_group, ID_HEADQUARTERS_GO_TO_UNIT, 1 );
	else
		group_set_active( hdlg->main_group, ID_HEADQUARTERS_GO_TO_UNIT, 0 );

	/* нанесите содержимое */
	frame_apply( hdlg->main_group->frame );
}

/** Отрисовка значка и имени элемента ввода @data в буфер surface @buffer. */
static void render_lbox_unit( void *data, SDL_Surface *buffer )
{
	Unit *e = (Unit*)data;
	char name[13];
	int x = (buffer->w - e->sel_prop->icon_w) / 2, y = (buffer->h - e->sel_prop->icon_h)/2;

	SDL_FillRect( buffer, 0, 0x0 );
	gui->font_std->align = ALIGN_X_CENTER | ALIGN_Y_BOTTOM;
	
    /* иконка */
    DEST( buffer, x, y, e->sel_prop->icon_w, e->sel_prop->icon_h );
    SOURCE( e->sel_prop->icon, 0, 0 );
    blit_surf();
	
    /* национальный флаг */
    DEST( buffer, x - 10, y + 6, nation_flag_width, nation_flag_height );
    SOURCE( nation_flags, e->nation->flag_offset, 0 );
    blit_surf();

    DEST( buffer, x + e->sel_prop->icon_w - 3, y + 6,
          unit_info_icons->str_w, unit_info_icons->str_h )
    if ( (config.use_core_units) && (camp_loaded != NO_CAMPAIGN) && (cur_player->ctrl == PLAYER_CTRL_HUMAN) &&
        ( e->core == AUXILIARY ) )
        SOURCE( unit_info_icons->str,
                unit_info_icons->str_w * ( e->str - 1 ),
                unit_info_icons->str_h * (e->player->strength_row + 1))
    else
        SOURCE( unit_info_icons->str, 
                unit_info_icons->str_w * ( e->str - 1 ),
                unit_info_icons->str_h * e->player->strength_row)
    blit_surf();

	snprintf(name,13,"%s",e->name); /* truncate name */
        write_text( gui->font_std, buffer, buffer->w / 2, buffer->h, name, 255 );
}

/****** Публичный *****/

/** Создайте и верните указатель на диалог штаб-квартиры. Используйте графику из темы
 * путь @theme_path. */
HeadquartersDlg *headquarters_dlg_create( char *theme_path )
{
	HeadquartersDlg *hdlg = NULL;
	char path[MAX_PATH], transitionPath[MAX_PATH];
	int sx, sy;
	
	hdlg = calloc( 1, sizeof(HeadquartersDlg) );
	if (hdlg == NULL)
		return NULL;
	
	/* создать основную группу (= main window) */
    search_file_name( path, 0, "headquarters_buttons", theme_path, "", 'i' );
	hdlg->main_group = group_create( gui_create_frame( 500, 400 ), 160, 
				load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ),
				20, 20, 2, ID_HEADQUARTERS_CLOSE,
				sdl.screen, 0, 0 );
	if (hdlg->main_group == NULL)
		goto failure;
	sx = group_get_width( hdlg->main_group ) - 30; 
	sy = group_get_height( hdlg->main_group ) - 25;
	group_add_button( hdlg->main_group, ID_HEADQUARTERS_CLOSE, sx, sy, 0, 
							tr("Close"), 2 );
	group_add_button( hdlg->main_group, ID_HEADQUARTERS_GO_TO_UNIT, sx - 30, sy, 0, 
							tr("Center On Unit"), 2 );
	group_set_active( hdlg->main_group, ID_HEADQUARTERS_GO_TO_UNIT, 0 );
	
	/* создать список единиц измерения */
    search_file_name( path, 0, "scroll_buttons", theme_path, "", 'i' );
	hdlg->units_lbox = lbox_create( gui_create_frame( 112, 380 ), 160, 6,
			load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
			8, 7, 100, 40, 1, 0x0000ff,
			render_lbox_unit, sdl.screen, 0, 0);
	if (hdlg->units_lbox == NULL)
		goto failure;
	
	return hdlg;
failure:
	fprintf(stderr, tr("Failed to create headquarters dialogue\n"));
	headquarters_dlg_delete(&hdlg);
	return NULL;
}

void headquarters_dlg_delete( HeadquartersDlg **hdlg )
{
	if (*hdlg) {
		HeadquartersDlg *ptr = *hdlg;
		
		group_delete( &ptr->main_group );
		lbox_delete( &ptr->units_lbox );
		free( ptr );
	}
	*hdlg = NULL;
}

/** Верните ширину диалога штаб-квартиры @hdlg, сложив все компоненты. */
int headquarters_dlg_get_width(HeadquartersDlg *hdlg)
{
	return group_get_width( hdlg->main_group ) +
		lbox_get_width( hdlg->units_lbox );
}

/** Верните высоту диалога штаб-квартиры @hdlg, сложив все компоненты. */
int headquarters_dlg_get_height(HeadquartersDlg *hdlg)
{
	return group_get_height( hdlg->main_group );
}

/** Переместите диалог штаб-квартиры @hdlg в положение @px, @py, переместив все отдельно
 * компоненты. */
void headquarters_dlg_move( HeadquartersDlg *hdlg, int px, int py)
{
	int sx = px, sy = py;
	int units_offset = (group_get_height( hdlg->main_group ) -
			lbox_get_height( hdlg->units_lbox )) / 2;
	
	group_move( hdlg->main_group, sx, sy );
	sx += group_get_width( hdlg->main_group );
	lbox_move(hdlg->units_lbox, sx, sy + units_offset);
}

/* Скрыть, нарисовать, нарисовать фон, получить фон для диалога штаб-квартиры @hdlg,
применяя действие ко всем отдельным компонентам. */
void headquarters_dlg_hide( HeadquartersDlg *hdlg, int value)
{
	group_hide(hdlg->main_group, value);
	lbox_hide(hdlg->units_lbox, value);
}
void headquarters_dlg_draw( HeadquartersDlg *hdlg)
{
	group_draw(hdlg->main_group);
	lbox_draw(hdlg->units_lbox);
}
void headquarters_dlg_draw_bkgnd( HeadquartersDlg *hdlg)
{
	group_draw_bkgnd(hdlg->main_group);
	lbox_draw_bkgnd(hdlg->units_lbox);
}
void headquarters_dlg_get_bkgnd( HeadquartersDlg *hdlg)
{
	group_get_bkgnd(hdlg->main_group);
	lbox_get_bkgnd(hdlg->units_lbox);
}

/** Обработайте движение мыши для диалога штаб-квартиры @hdlg, проверив все компоненты.
 * @cx, @cy-абсолютное положение мыши на экране. Возврат 1, если действие было
 * обрабатывается каким-то окном, в противном случае 0. */
int headquarters_dlg_handle_motion( HeadquartersDlg *hdlg, int cx, int cy)
{
	int ret = 1;
	void *item = NULL;
	
	if (!group_handle_motion(hdlg->main_group,cx,cy))
	if (!lbox_handle_motion(hdlg->units_lbox,cx,cy,&item))
		ret = 0;
	return ret;
}

/** Ручка мыши щелкает по экрану позиции @cx,@cy с помощью кнопки @bid. 
 * Верните 1, если была нажата кнопка, которая нуждается в восходящей обработке (например, чтобы
* закрыть диалог), и установите @hbtn. В противном случае обработайте событие внутренне и
 * возвращает 0. */
int headquarters_dlg_handle_button( HeadquartersDlg *hdlg, int bid, int cx, int cy,
		Button **hbtn )
{
	void *item = NULL;
	
	if (group_handle_button(hdlg->main_group,bid,cx,cy,hbtn)) {
		/* поймать и обработать кнопку go_to_unit внутри */
		if ((*hbtn)->id == ID_HEADQUARTERS_GO_TO_UNIT) {
            Unit *cur_unit = lbox_get_selected_item( hdlg->units_lbox );
            engine_focus( cur_unit->x, cur_unit->y, 0 );
            engine_select_unit( cur_unit );
			return 0;
		}
		return 1;
	}
	if (lbox_handle_button(hdlg->units_lbox,bid,cx,cy,hbtn,&item)) {
		if (item) {
			/* очистить выбор юнита */
			update_headquarters_info( hdlg );
			gui_show_full_info( hdlg->main_group->frame, lbox_get_selected_item( hdlg->units_lbox ) );
		}
		return 0;
	}
	return 0;
}

/** Сброс настроек диалога штаб-квартиры для global @cur_player */
void headquarters_dlg_reset( HeadquartersDlg *hdlg )
{
	lbox_set_items( hdlg->units_lbox, get_active_player_units() );
    hdlg->units_lbox->cur_item = 0;
    update_headquarters_info( hdlg );
}
