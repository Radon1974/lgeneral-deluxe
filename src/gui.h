/***************************************************************************
                          gui.h  -  description
                             -------------------
    begin                : Sun Mar 31 2002
    copyright            : (C) 2001 by Michael Speck
    email                : kulkanie@gmx.net
 ***************************************************************************/
/***************************************************************************
                     Modifications by LGD team 2012+.
 ***************************************************************************/
/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __GUI_H
#define __GUI_H

#include "unit.h"
#include "windows.h"
#include "purchase_dlg.h"
#include "headquarters_dlg.h"
#include "message_dlg.h"

struct _List;

/*
====================================================================
Идентификаторы кнопок
====================================================================
*/
enum {
    ID_INTERN_UP = -1000,
    ID_INTERN_DOWN,
    ID_MENU = 0,
    ID_SCEN_INFO,
    ID_AIR_MODE,
    ID_DEPLOY,
    ID_STRAT_MAP,
    ID_END_TURN,
    ID_CONDITIONS,
    ID_PURCHASE,
    ID_HEADQUARTERS,
    ID_OK,
    ID_CANCEL,
    ID_SUPPLY,
    ID_EMBARK_AIR,
    ID_MERGE,
    ID_UNDO,
    ID_RENAME,
    ID_MODIFY,
    ID_DISBAND,
    ID_SPLIT,
    ID_REPLACEMENTS,
    ID_ELITE_REPLACEMENTS,
    ID_SPLIT_1,
    ID_SPLIT_2,
    ID_SPLIT_3,
    ID_SPLIT_4,
    ID_SPLIT_5,
    ID_SPLIT_6,
    ID_SPLIT_7,
    ID_SPLIT_8,
    ID_SPLIT_9,
    ID_SPLIT_10,
    ID_APPLY_DEPLOY,
    ID_CANCEL_DEPLOY,
    ID_DEPLOY_UP,
    ID_DEPLOY_DOWN,
    ID_SAVE,
    ID_LOAD,
    ID_RESTART,
    ID_CAMP,
    ID_SCEN,
    ID_OPTIONS,
    ID_MOD_SELECT,
    ID_QUIT,
    ID_LOAD_OK,
    ID_LOAD_CANCEL,
    ID_SAVE_OK,
    ID_SAVE_CANCEL,
    ID_NEW_FOLDER,
    ID_C_SUPPLY,
    ID_C_WEATHER,
    ID_C_SHOW_CPU,
    ID_C_SHOW_STRENGTH,
    ID_C_SOUND,
    ID_C_SOUND_INC,
    ID_C_SOUND_DEC,
    ID_C_MUSIC,
    ID_C_GRID,
    ID_C_VMODE,
    ID_SCEN_OK,
    ID_SCEN_CANCEL,
    ID_SCEN_SETUP,
    ID_CAMP_OK,
    ID_CAMP_CANCEL,
    ID_CAMP_SETUP,
    ID_SCEN_SETUP_OK,
    ID_SCEN_SETUP_FOG,
    ID_SCEN_SETUP_SUPPLY,
    ID_SCEN_SETUP_WEATHER,
    ID_SCEN_SETUP_DEPLOYTURN,
    ID_SCEN_SETUP_PURCHASE,
    ID_SCEN_SETUP_MERGE_REPLACEMENTS,
    ID_SCEN_SETUP_CTRL,
    ID_SCEN_SETUP_MODULE,
    ID_CAMP_SETUP_OK,
    ID_CAMP_SETUP_MERGE_REPLACEMENTS,
    ID_CAMP_SETUP_CORE,
    ID_CAMP_SETUP_PURCHASE,
    ID_CAMP_SETUP_WEATHER,
    ID_MODULE_OK,
    ID_MODULE_CANCEL,
    ID_PURCHASE_OK,
    ID_PURCHASE_EXIT,
    ID_PURCHASE_REFUND,
    ID_VMODE_OK,
    ID_VMODE_CANCEL,
    ID_MOD_SELECT_OK,
    ID_MOD_SELECT_CANCEL,
    ID_HEADQUARTERS_CLOSE,
    ID_HEADQUARTERS_GO_TO_UNIT,
    ID_MESSAGE_LIST_UP,
    ID_MESSAGE_LIST_DOWN
};

/*
====================================================================
Графические интерфейсы, шрифты и окна.
====================================================================
*/
typedef struct {
    char *name;         /* имя каталога gui */
    Font *font_std;
    Font *font_status;
    Font *font_error;
    Font *font_turn_info;
    Font *font_brief;
    Image *cursors;
    Label *label_left, *label_center, *label_right;
    Frame *qinfo1, *qinfo2; /* быстрая информация о юните */
    Frame *finfo; /* полная информация о юните */
    Frame *sinfo; /* информация о сценарии */
    Group *unit_buttons; /* кнопки действия юнита */
    Group *split_menu; /* меню количества юнитов для разделения */
    Group *confirm; /* окно подтверждения */
    PurchaseDlg *purchase_dlg;
    HeadquartersDlg *headquarters_dlg;
    Group *deploy_window;
    Edit *edit;
    MessageDlg *message_dlg; /* консоль сообщений */
    Group *base_menu; /* основное меню (переключатель airmode, развернуть, конец хода, меню ...) */
    Group *main_menu; /* главное игровое меню */
    Group *opt_menu;  /* опции */
    FDlg *load_menu;
    FDlg *save_menu;
    SelectDlg *vmode_dlg;
    FDlg *scen_dlg;    /* выбор сценария */
    FDlg *camp_dlg;    /* выбор кампании */
    SDlg *scen_setup;  /* настройка сценария (элементы управления и модули аи) */
    SDlg *camp_setup;  /* настройка кампании (элементы управления) */
    FDlg *module_dlg;  /* выбор модуля аи */
    FDlg *mod_select_dlg; /* выбор модов */
    /* каркасные плитки */
    SDL_Surface *fr_luc, *fr_llc, *fr_ruc, *fr_rlc, *fr_hori, *fr_vert;
    SDL_Surface *brief_frame;   /* окно брифинга */
    SDL_Surface *bkgnd;         /* фон для окна брифинга и главного меню */
    SDL_Surface *wallpaper;     /* обои используются, если фон не заполняет весь экран */
    SDL_Surface *folder_icon;   /* значок папки для списка файлов */
    SDL_Surface *map_frame;     /* окно карты */
    /* звуки */
#ifdef WITH_SOUND
    Wav *wav_click;
    Wav *wav_edit;
#endif
    /* разное */
    int mirror_asymm; /* зеркальные асимметричные окна, чтобы заглянуть за них */
} GUI;

struct MessagePane;

/*
====================================================================
Создание поверхности рамки
====================================================================
*/
SDL_Surface *gui_create_frame( int w, int h );

/*
====================================================================
Создайте графический интерфейс и используйте графику в gfx / themes/path
====================================================================
*/
int gui_load( char *path );
void gui_delete();

/*
====================================================================
Переместите все окна в нужное положение в соответствии с
измерениями экрана.
====================================================================
*/
void gui_adjust();

/*
====================================================================
Измените всю графическую графику GUI на ту, что находится в gfx / theme/path.
====================================================================
*/
int gui_change( const char *path );

/*
====================================================================
Скрыть/нарисовать с экрана/на экран или получить фон
====================================================================
*/
void gui_get_bkgnds();
void gui_draw_bkgnds();
void gui_draw();

/*
====================================================================
Установите курсор.
====================================================================
*/
enum {
    CURSOR_STD = 0,
    CURSOR_SELECT,
    CURSOR_ATTACK,
    CURSOR_MOVE,
    CURSOR_MOUNT,
    CURSOR_DEBARK,
    CURSOR_EMBARK,
    CURSOR_MERGE,
    CURSOR_DEPLOY,
    CURSOR_UNDEPLOY,
    CURSOR_UP,
    CURSOR_DOWN,
    CURSOR_LEFT,
    CURSOR_RIGHT,
    CURSOR_COUNT
};
void gui_set_cursor( int type );

/*
====================================================================
Обрабатывать события.
====================================================================
*/
int gui_handle_motion( int cx, int cy );
int  gui_handle_button( int button_id, int cx, int cy, Button **button );
void gui_update( int ms );

/*
====================================================================
Установите быстрый информационный кадр с информацией об этом юните и установите hide = 0.
====================================================================
*/
void gui_show_quick_info( Frame *qinfo, Unit *unit );

/*
====================================================================
Нарисуйте ожидаемые потери на этикетке.
====================================================================
*/
void gui_show_expected_losses( Unit *att, Unit *def, int att_dam, int def_dam );

/*
====================================================================
Нарисуйте фактические потери на этикетке.
====================================================================
*/
void gui_show_actual_losses( Unit *att, Unit *def,
    int att_suppr, int att_dam, int def_suppr, int def_dam );

/*
====================================================================
Нарисуйте силу или подавление данного блока на данной метке
====================================================================
*/
void gui_show_unit_results( Unit *unit, Label *label, int is_str, int dam );

/*
====================================================================
Показать полное информационное окно.
====================================================================
*/
void gui_show_full_info( Frame *dest, Unit *unit );

/*
====================================================================
Показать информационное окно сценария.
====================================================================
*/
void gui_show_scen_info();

/*
====================================================================
Покажите явные условия победы и используйте для этого информационное окно сценария
.
====================================================================
*/
void gui_show_conds();

/*
====================================================================
Показать окно подтверждения.
====================================================================
*/
void gui_show_confirm( const char *text );

/*
====================================================================
Показывать кнопки юнита на экране x, y (без проверки кнопок)
====================================================================
*/
void gui_show_unit_buttons( int x, int y );

/** Show unit purchase dialogue. */
void gui_show_purchase_window();

/** Show unit purchase dialogue. */
void gui_show_headquarters_window();

/*
====================================================================
Показать окно развертывания и выбрать первый юнит как «deploy_unit».
====================================================================
*/
void gui_show_deploy_window();

/*
====================================================================
Обработайте окно развертывания.
  gui_handle_deploy_motion: 'unit' - это единица
измерения, над
которой в данный момент находится курсор gui_handle_deploy_click: 'new_unit' имеет значение True, если была создана новый юнит.
      выбранный (который является 'deploy_unit') else False
      верните True, если что-то произошло
====================================================================
*/
int gui_handle_deploy_click( int button, int cx, int cy );
void gui_handle_deploy_motion( int cx, int cy, Unit **unit );

/*
====================================================================
Прокрутите список развертывания вверх/вниз.
====================================================================
*/
void gui_scroll_deploy_up();
void gui_scroll_deploy_down();

/*
====================================================================
Обновите список развертывания. Блок либо удаляется, либо добавляется в
left_deploy_units, и окно развертывания обновляется.
====================================================================
*/
void gui_remove_deploy_unit( Unit *unit );
void gui_add_deploy_unit( Unit *unit );

/*
====================================================================
Показать базовое меню на экране x, y (не включает проверку кнопки)
====================================================================
*/
void gui_show_menu( int x, int y );

/*
====================================================================
Визуализируйте имя файла на поверхности. (каталоги, начать с
Астерикс)
====================================================================
*/
void gui_render_file_name( void *item, SDL_Surface *buffer );

/*
====================================================================
Рендеринг сценария / названия кампании на поверхность. (каталоги, начать с
Астерикс)
====================================================================
*/
void gui_render_scen_file_name( void *item, SDL_Surface *buffer );

/*
====================================================================
Обработка выбора файла сценария (отображение информации и
загрузка scen_info из полного пути)
====================================================================
*/
void gui_render_scen_info( const char *path, const char *camp_entry, SDL_Surface *buffer );

/*
====================================================================
Обработайте выбор файла для загрузки.
@camp_entry не используется
====================================================================
*/
void gui_render_load_menu( const char *path, const char *camp_entry, SDL_Surface *buffer );

/*
====================================================================
Обработайте выбор файла для сохранения.
@camp_entry не используется
====================================================================
*/
void gui_render_save_menu( const char *path, const char *camp_entry, SDL_Surface *buffer );

/*
====================================================================
Обработка выбора файла кампании (отображение информации и
загрузка scen_info из полного пути)
====================================================================
*/
void gui_render_camp_info( const char *path, const char *camp_entry, SDL_Surface *buffer );

/*
====================================================================
Обрабатывать загрузку сохраненного файла (диалог подтверждения рендеринга)
====================================================================
*/
void gui_render_load( const char *path, SDL_Surface *buffer );

/*
====================================================================
Создает новую структуру панели сообщений.
====================================================================
*/
struct MessagePane *gui_create_message_pane();

/*
====================================================================
Удаляет заданную структуру панели сообщений.
====================================================================
*/
void gui_delete_message_pane(struct MessagePane *pane);

/*
====================================================================
Sets the text for the message pane.
====================================================================
*/
void gui_set_message_pane_text( struct MessagePane *pane, const char *text );

/*
====================================================================
Задает идентификатор по умолчанию для панели сообщений.
====================================================================
*/
void gui_set_message_pane_default( struct MessagePane *pane, const char *default_id );

/*
====================================================================
Возвращает текущий выбранный идентификатор или 0, если ничего не выбрано.
====================================================================
*/
const char *gui_get_message_pane_selection( struct MessagePane *pane );

/*
====================================================================
Заполняет параметры для данной панели сообщений.
идентификаторы константный тип char * список идентификаторов
descs константный тип char * список текстовое описание будучи сопоставлены идентификаторы.
====================================================================
*/
void gui_set_message_pane_options( struct MessagePane *pane, struct _List *ids, struct _List *descs );

/*
====================================================================
Рисует и заполняет текстом панель сообщений.
====================================================================
*/
void gui_draw_message_pane( struct MessagePane *pane );

/*
====================================================================
Обрабатывает события на панели сообщений.
pane-панель сообщений data
mx, my-экранные координаты
кнопки мыши-кнопка
мыши нажата - 1, если кнопка мыши была нажата, 0, если отпущена (не определено
, если кнопка BUTTON_NONE)
====================================================================
*/
void gui_handle_message_pane_event( struct MessagePane *pane, int mx, int my, int button, int pressed );

/*
====================================================================
Откройте настройку сценария и установите первого игрока в качестве выбранного.
====================================================================
*/
void gui_open_scen_setup();

/*
====================================================================
Откройте настройку кампании.
====================================================================
*/
void gui_open_camp_setup();

/*
====================================================================
Визуализация имени игрока в настройках сценария
====================================================================
*/
void gui_render_player_name( void *item, SDL_Surface *buffer );

/*
====================================================================
Обработайте выбор игрока в настройках.
====================================================================
*/
void gui_handle_player_select( void *item );

/*
====================================================================
Загрузите информацию о модуле.
@camp_entry не используется
====================================================================
*/
void gui_render_module_info( const char *path, const char *camp_entry, SDL_Surface *buffer );

/*
====================================================================
Обработайте выбор папки mod для загрузки.
@camp_entry не используется
====================================================================
*/
void gui_render_mod_select_info( const char *path, const char *camp_entry, SDL_Surface *buffer );

/*
====================================================================
Зеркальное положение асимметричных окон.
====================================================================
*/
void gui_mirror_asymm_windows();

/** Показать выбор видеорежима */
void gui_vmode_dlg_show();

#endif
