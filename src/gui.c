/***************************************************************************
                          gui.c  -  description
                             -------------------
    begin                : Sun Mar 31 2002
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

#include "lgeneral.h"
#include "event.h"
#include "parser.h"
#include "date.h"
#include "nation.h"
#include "unit.h"
#include "file.h"
#include "map.h"
#include "list.h"
#include "nation.h"
#include "unit_lib.h"
#include "player.h"
#include "slot.h"
#include "windows.h"
#include "gui.h"
#include "scenario.h"
#include "campaign.h"
#include "localize.h"

/*
====================================================================
Внешние
====================================================================
*/
extern Sdl sdl;
extern int hex_w, hex_h;
extern Player *cur_player;
extern Unit_Info_Icons *unit_info_icons;
extern int nation_flag_width, nation_flag_height;
extern SDL_Surface *nation_flags;
extern Unit *cur_unit;
extern Unit_Class *unit_classes;
extern int trgt_type_count;
extern Trgt_Type *trgt_types;
extern Mov_Type *mov_types;
extern Scen_Info *scen_info;
extern char scen_message[128];
extern Weather_Type *weather_types;
extern int turn;
extern int deploy_turn;
extern int deploy_unit_id;
extern Unit *deploy_unit;
extern List *avail_units;
extern List *left_deploy_units;
extern List *players;
extern Setup setup;
extern int vcond_check_type;
extern VCond *vconds;
extern int vcond_count;
extern Config config;
extern int camp_loaded;
extern int cur_weather;
extern StrToFlag fct_units[];

/*
====================================================================
Локальные
====================================================================
*/

GUI *gui = 0;

int cursor; /* текущий идентификатор курсора */
/* горячие точки курсора */
typedef struct {
    int x,y;
} HSpot;
HSpot hspots[CURSOR_COUNT] = {
    {0,0},
    {0,0},
    {11,11},
    {0,0},
    {11,11},
    {11,11},
    {11,11},
    {11,11},
    {11,11},
    {11,11},
    {11,11}
};

Font *log_font = 0; /* этот шрифт используется для отображения загрузки сценария
                       info и инициируется gui_load () */

int deploy_offset = 0; /* смещение в списке развертывания */
int deploy_border = 10;
int deploy_show_count = 7; /* количество отображаемых единиц в списке */

/*
====================================================================
Добавьте юниты в окно развертывания.
====================================================================
*/
void gui_add_deploy_units( SDL_Surface *contents )
{
    int i;
    Unit *unit;
    int deploy_border = 10;
    int sx = deploy_border, sy = deploy_border;
    SDL_FillRect( contents, 0, 0x0 );
    for ( i = 0; i < deploy_show_count; i++ ) {
        unit = list_get( left_deploy_units, deploy_offset + i );
        if ( unit ) {
            if ( unit == deploy_unit ) {
                DEST( contents, sx, sy, hex_w, hex_h );
                fill_surf( 0xbbbbbb );
            }
            DEST( contents,
                  sx + ( ( hex_w - unit->sel_prop->icon_w ) >> 1 ),
                  sy + ( ( hex_h - unit->sel_prop->icon_h ) >> 1 ),
                  unit->sel_prop->icon_w, unit->sel_prop->icon_h );
            SOURCE( unit->sel_prop->icon, 0, 0 );
            blit_surf();
            sy += hex_h;
        }
    }
}

/*
====================================================================
Общественные
====================================================================
*/

/*
====================================================================
Создайте поверхность рамки
====================================================================
*/
SDL_Surface *gui_create_frame( int w, int h )
{
    int i;
    SDL_Surface *surf = create_surf( w, h, SDL_SWSURFACE );
    SDL_FillRect( surf, 0, 0x0 );
    /* верхняя, нижняя горизонтальный луч */
    for ( i = 0; i < w; i += gui->fr_hori->w ) {
        DEST( surf, i, 0, gui->fr_hori->w, gui->fr_hori->h );
        SOURCE( gui->fr_hori, 0, 0 );
        blit_surf();
        DEST( surf, i, h - gui->fr_hori->h, gui->fr_hori->w, gui->fr_hori->h );
        SOURCE( gui->fr_hori, 0, 0 );
        blit_surf();
    }
    /* левый, правый вертикальный луч */
    for ( i = 0; i < h; i += gui->fr_vert->h ) {
        DEST( surf, 0, i, gui->fr_vert->w, gui->fr_vert->h );
        SOURCE( gui->fr_vert, 0, 0 );
        blit_surf();
        DEST( surf, w - gui->fr_vert->w, i, gui->fr_vert->w, gui->fr_vert->h );
        SOURCE( gui->fr_vert, 0, 0 );
        blit_surf();
    }
    /* левый верхний угол */
    DEST( surf, 0, 0, gui->fr_luc->w, gui->fr_luc->h );
    SOURCE( gui->fr_luc, 0, 0 );
    blit_surf();
    /* левый нижний угол */
    DEST( surf, 0, h - gui->fr_llc->h, gui->fr_llc->w, gui->fr_llc->h );
    SOURCE( gui->fr_llc, 0, 0 );
    blit_surf();
    /* правый верхний угол */
    DEST( surf, w - gui->fr_ruc->w, 0, gui->fr_ruc->w, gui->fr_ruc->h );
    SOURCE( gui->fr_ruc, 0, 0 );
    blit_surf();
    /* правый нижний угол */
    DEST( surf, w - gui->fr_rlc->w, h - gui->fr_rlc->h, gui->fr_rlc->w, gui->fr_rlc->h );
    SOURCE( gui->fr_rlc, 0, 0 );
    blit_surf();
    return surf;
}

/*
====================================================================
Создайте графический интерфейс и используйте графику в <dir> / Theme / ...
====================================================================
*/
int gui_load( char *dir )
{
    char str[128];
    int i;
    int sx, sy;
    char path[MAX_PATH], path2[MAX_PATH], path3[MAX_PATH], path4[MAX_PATH], transitionPath[MAX_PATH];
    char **button_temp;

    gui_delete();
    gui = calloc( 1, sizeof( GUI ) );
    /* имя */
    gui->name = strdup( dir );
    /* каркасная плитка */
    search_file_name( path, 0, "fr_luc", dir, "Theme", 'i' );
    gui->fr_luc = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 );
    SDL_SetColorKey( gui->fr_luc, 0, 0 );
    search_file_name( path, 0, "fr_llc", dir, "Theme", 'i' );
    gui->fr_llc = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 );
    SDL_SetColorKey( gui->fr_llc, 0, 0 );
    search_file_name( path, 0, "fr_ruc", dir, "Theme", 'i' );
    gui->fr_ruc = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 );
    SDL_SetColorKey( gui->fr_ruc, 0, 0 );
    search_file_name( path, 0, "fr_rlc", dir, "Theme", 'i' );
    gui->fr_rlc = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 );
    SDL_SetColorKey( gui->fr_rlc, 0, 0 );
    search_file_name( path, 0, "fr_hori", dir, "Theme", 'i' );
    gui->fr_hori = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 );
    SDL_SetColorKey( gui->fr_hori, 0, 0 );
    search_file_name( path, 0, "fr_vert", dir, "Theme", 'i' );
    gui->fr_vert = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 );
    SDL_SetColorKey( gui->fr_vert, 0, 0 );
    /* информационная рамка и фон */
    search_file_name( path, 0, "bkgnd", dir, "Theme", 'i' );
    if ( ( gui->bkgnd = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto sdl_failure;
    search_file_name( path, 0, "brief_frame", dir, "Theme", 'i' );
    if ( ( gui->brief_frame = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto sdl_failure;
    search_file_name( path, 0, "wallpaper", dir, "Theme", 'i' );
    if ( ( gui->wallpaper = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto sdl_failure;
    /* рамка карты */
    const SDL_PixelFormat *fmt = SDL_GetVideoSurface()->format;
    gui->map_frame = SDL_CreateRGBSurface( SDL_SWSURFACE, sdl.screen->w, sdl.screen->h- 30,
                                           fmt->BitsPerPixel,
                                           fmt->Rmask,fmt->Gmask,fmt->Bmask,fmt->Amask );
    /* папка */
    search_file_name( path, 0, "folder", dir, "Theme", 'i' );
    if ( ( gui->folder_icon = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto sdl_failure;
    /* основные шрифты */
    search_file_name( path, 0, "font_std", dir, "Theme", 'i' );
    strcpy( path2, path );
    gui->font_std = load_font( path2 );
    search_file_name( path, 0, "font_status", dir, "Theme", 'i' );
    strcpy( path2, path );
    gui->font_status = load_font( path2 );
    search_file_name( path, 0, "font_error", dir, "Theme", 'i' );
    strcpy( path2, path );
    gui->font_error = load_font( path2 );
    search_file_name( path, 0, "font_turn_info", dir, "Theme", 'i' );
    strcpy( path2, path );
    gui->font_turn_info = load_font( path2 );
    search_file_name( path, 0, "font_brief", dir, "Theme", 'i' );
    strcpy( path2, path );
    gui->font_brief = load_font( path2 );
    /* курсоры */
    search_file_name( path, 0, "cursors", dir, "Theme", 'i' );
    if ( ( gui->cursors = image_create( load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ),
                22, 22, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    /* информационные ярлыки */
    gui->label_left = 0;
    gui->label_center = 0;
    gui->label_right = 0;
    /* быстрая информация об устройстве */
    if ( ( gui->qinfo1 = frame_create( gui_create_frame( 240, 60 ), 160, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    frame_hide( gui->qinfo1, 1 );
    if ( ( gui->qinfo2 = frame_create( gui_create_frame( 240, 60 ), 160, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    frame_hide( gui->qinfo2, 1 );
    /* полная информация об объекте */
    if ( ( gui->finfo = frame_create( gui_create_frame( 500, 350 ), 160, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    frame_hide( gui->finfo, 1 );
    /* информация о сценарии */
    if ( ( gui->sinfo = frame_create( gui_create_frame( 400, 360 ), 160, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    frame_hide( gui->sinfo, 1 );
    /* окно подтверждения */
    search_file_name( path2, 0, "confirm_buttons", dir, "Theme", 'i' );
    if ( ( gui->confirm = group_create( gui_create_frame( 200, 80 ), 160, load_surf( path2,
                SDL_SWSURFACE, 0, 0, 0, 0 ), 20, 20, 6, ID_OK, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    sx = gui->confirm->frame->img->img->w - 60; sy = gui->confirm->frame->img->img->h - 30;
    group_add_button( gui->confirm, ID_OK, sx, sy, 0, tr("Accept"), 2 ); sx += 30;
    group_add_button( gui->confirm, ID_CANCEL, sx, sy, 0, tr("Cancel"), 2 );
    group_hide( gui->confirm, 1 );
    /* кнопки устройства */
    search_file_name( path2, 0, "unit_buttons", dir, "Theme", 'i' );
    if ( ( gui->unit_buttons = group_create( gui_create_frame( 30, 260 ), 160, load_surf( path2,
                SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24, 10, ID_SUPPLY, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    sx = 3; sy = 3;
    group_add_button( gui->unit_buttons, ID_UNDO, sx, sy, 0, tr("Undo Turn [u]"), 2 ); sy += 40;
    group_add_button( gui->unit_buttons, ID_SUPPLY, sx, sy, 0, tr("Supply Unit [s]"), 2 ); sy += 30;
    group_add_button( gui->unit_buttons, ID_EMBARK_AIR, sx, sy, 0, tr("Air Embark"), 2 ); sy += 30;
    group_add_button( gui->unit_buttons, ID_MERGE, sx, sy, 0, tr("Merge Unit [j]"), 2 );
    group_add_button( gui->unit_buttons, ID_REPLACEMENTS, sx, sy, 0, tr("Replacements"), 2 ); sy += 30;
    group_add_button( gui->unit_buttons, ID_SPLIT, sx, sy, 0, tr("Split Unit [x+1..9]"), 2 );
    group_add_button( gui->unit_buttons, ID_ELITE_REPLACEMENTS, sx, sy, 0, tr("Elite Replacements"), 2 ); sy += 30;
    group_add_button( gui->unit_buttons, ID_RENAME, sx, sy, 0, tr("Rename Unit"), 2 ); sy += 30;
    group_add_button( gui->unit_buttons, ID_MODIFY, sx, sy, 0, tr("Modify Unit"), 2 ); sy += 40;
    group_add_button( gui->unit_buttons, ID_DISBAND, sx, sy, 0, tr("Disband Unit"), 2 );
    group_hide( gui->unit_buttons, 1 );
    /* разделенное меню */
    search_file_name( path2, 0, "strength_buttons", dir, "Theme", 'i' );
    if ( ( gui->split_menu = group_create( gui_create_frame( 26, 186 ), 160, load_surf( path2,
                SDL_SWSURFACE, 0, 0, 0, 0 ), 20, 20, 10, ID_SPLIT_1, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    sx = 3; sy = 3;
    for ( i = 0; i < 9; i++ ) {
        sprintf( str, tr("Split Up %d Strength"), i+1 );
        group_add_button( gui->split_menu, ID_SPLIT_1 + i, sx, sy, 0, str, 2 );
        sy += 20;
    }
    group_hide( gui->split_menu, 1 );
    /* окно развертывания */
    search_file_name( path2, 0, "deploy_buttons", dir, "Theme", 'i' );
    if ( ( gui->deploy_window = group_create( gui_create_frame( 80, 440 ), 160, load_surf( path2,
                SDL_SWSURFACE, 0, 0, 0, 0 ), 20, 20, 6, ID_APPLY_DEPLOY, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    sx = gui->deploy_window->frame->img->img->w - 65;
    sy = gui->deploy_window->frame->img->img->h - 60;
    group_add_button( gui->deploy_window, ID_DEPLOY_UP, sx, sy, 0, tr("Scroll Up"), 2 );
    group_add_button( gui->deploy_window, ID_DEPLOY_DOWN, sx + 30, sy, 0, tr("Scroll Down"), 2 ); sy += 30;
    group_add_button( gui->deploy_window, ID_APPLY_DEPLOY, sx, sy, 0, tr("Apply Deployment"), 2 );
    group_add_button( gui->deploy_window, ID_CANCEL_DEPLOY, sx + 30, sy, 0, tr("Cancel Deployment"), 2 );
    group_hide( gui->deploy_window, 1 );
    /* окно штаб-квартиры */
    sprintf( path2, "%s/Theme", dir );
    if ( (gui->headquarters_dlg = headquarters_dlg_create( path2 )) == NULL)
	    goto failure;
    headquarters_dlg_hide( gui->headquarters_dlg, 1 );
    /* редактировать */
    if ( ( gui->edit = edit_create( gui_create_frame( 440, 30 ), 160, gui->font_std, MAX_NAME, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    edit_hide( gui->edit, 1 );
    /* диалог сообщения */
    gui->message_dlg = 0;
    /* базовое меню */
    search_file_name( path2, 0, "menu0_buttons", dir, "Theme", 'i' );
    if ( ( gui->base_menu = group_create( gui_create_frame( 30, 270 ), 160, load_surf( path2,
                SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24, 9, ID_MENU, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    sx = 3; sy = 3;
    group_add_button( gui->base_menu, ID_AIR_MODE, sx, sy, 0, tr("Switch Air/Ground [t]"), 2 ); sy += 30;
    group_add_button( gui->base_menu, ID_STRAT_MAP, sx, sy, 0, tr("Strategic Map [o]"), 2 ); sy += 30;
    group_add_button( gui->base_menu, ID_PURCHASE, sx, sy, 0, tr("Request Reinforcements"), 2 ); sy += 30;
    group_add_button( gui->base_menu, ID_DEPLOY, sx, sy, 0, tr("Deploy Reinforcements [d]"), 2 ); sy += 30;
    group_add_button( gui->base_menu, ID_HEADQUARTERS, sx, sy, 0, tr("Headquarters [h]"), 2 ); sy += 30;
    group_add_button( gui->base_menu, ID_SCEN_INFO, sx, sy, 0, tr("Scenario Info [i]"), 2 ); sy += 30;
    group_add_button( gui->base_menu, ID_CONDITIONS, sx, sy, 0, tr("Victory Conditions"), 2 ); sy += 30;
    group_add_button( gui->base_menu, ID_END_TURN, sx, sy, 0, tr("End Turn [e]"), 2 ); sy += 30;
    group_add_button( gui->base_menu, ID_MENU, sx, sy, 0, tr("Main Menu"), 2 );
    group_hide( gui->base_menu, 1 );
    /* главное меню */
    search_file_name( path2, 0, "menu1_buttons", dir, "Theme", 'i' );
    if ( ( gui->main_menu = group_create( gui_create_frame( 30, 240 ), 160, load_surf( path2,
                SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24, 8, ID_SAVE, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    sx = 3; sy = 3;
    group_add_button( gui->main_menu, ID_SAVE, sx, sy, 0, tr("Save Game"), 2 ); sy += 30;
    group_add_button( gui->main_menu, ID_LOAD, sx, sy, 0, tr("Load Game"), 2 ); sy += 30;
    group_add_button( gui->main_menu, ID_RESTART, sx, sy, 0, tr("Restart Scenario"), 2 ); sy += 30;
    group_add_button( gui->main_menu, ID_CAMP, sx, sy, 0, tr("Load Campaign"), 2 ); sy += 30;
    group_add_button( gui->main_menu, ID_SCEN, sx, sy, 0, tr("Load Scenario"), 2 ); sy += 30;
    group_add_button( gui->main_menu, ID_OPTIONS, sx, sy, 0, tr("Options"), 2 ); sy += 30;
    group_add_button( gui->main_menu, ID_MOD_SELECT, sx, sy, 0, tr("Mod Select"), 2 ); sy += 30;
    group_add_button( gui->main_menu, ID_QUIT, sx, sy, 0, tr("Quit Game"), 2 );
    group_hide( gui->main_menu, 1 );
    /* меню загрузки */
    search_file_name( path, 0, "confirm_buttons", dir, "Theme", 'i' );
    search_file_name( path2, 0, "scroll_buttons", dir, "Theme", 'i' );
    gui->load_menu = fdlg_create( gui_create_frame( 600, 356 ), 160, 10,
                                  load_surf( path2, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                  20,
                                  gui_create_frame( 600, 46),
                                  load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 20, 20,
                                  ID_LOAD_OK,
                                  gui_render_file_name, gui_render_load_menu,
                                  sdl.screen, 0, 0, ARRANGE_ROWS, 0, 0, LIST_ALL );
    fdlg_hide( gui->load_menu, 1 );
    /* меню сохранения */
    search_file_name( path, 0, "save_dlg_buttons", dir, "Theme", 'i' );
    gui->save_menu = fdlg_create( gui_create_frame( 600, 356 ), 160, 10,
                                  load_surf( path2, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                  20,
                                  gui_create_frame( 600, 50),
                                  load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                  ID_SAVE_OK,
                                  gui_render_file_name, gui_render_save_menu,
                                  sdl.screen, 0, 0, ARRANGE_ROWS, 1, 0, LIST_ALL );
    sx = 15; sy = 15;
    group_add_button( gui->save_menu->group, ID_NEW_FOLDER, sx, sy, 0, tr("Create New Folder"), 2 );
    fdlg_hide( gui->save_menu, 1 );
    /* опции */
    search_file_name( path2, 0, "menu2_buttons", dir, "Theme", 'i' );
    if ( ( gui->opt_menu = group_create( gui_create_frame( 30, 300 - 60 ), 160, load_surf( path2,
                SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24, 10, ID_C_SUPPLY, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    sx = 3; sy = 3;
    //group_add_button( gui->opt_menu, ID_C_SUPPLY, sx, sy, 1, "Unit Supply" ); sy += 30;
    //group_add_button( gui->opt_menu, ID_C_WEATHER, sx, sy, 1, "Weather Influence" ); sy += 30;
    button_temp = calloc( 2, sizeof( char * ) );
    button_temp[0] = strdup( "Hex Grid: OFF" );
    button_temp[1] = strdup( "Hex Grid: ON" );
    group_add_button( gui->opt_menu, ID_C_GRID, sx, sy, button_temp, tr("Hex Grid [g]"), 2 ); sy += 30;
    free( button_temp[0] );
    free( button_temp[1] );
    free( button_temp );
    button_temp = calloc( 2, sizeof( char * ) );
    button_temp[0] = strdup( "Show CPU Turn: OFF" );
    button_temp[1] = strdup( "Show CPU Turn: ON" );
    group_add_button( gui->opt_menu, ID_C_SHOW_CPU, sx, sy, button_temp, tr("Show CPU Turn"), 2 ); sy += 30;
    free( button_temp[0] );
    free( button_temp[1] );
    free( button_temp );
    button_temp = calloc( 2, sizeof( char * ) );
    button_temp[0] = strdup( "Show Unit Strength: OFF" );
    button_temp[1] = strdup( "Show Unit Strength: ON" );
    group_add_button( gui->opt_menu, ID_C_SHOW_STRENGTH, sx, sy, button_temp, tr("Show Unit Strength [.]"), 2 ); sy += 30;
    free( button_temp[0] );
    free( button_temp[1] );
    free( button_temp );
    button_temp = calloc( 2, sizeof( char * ) );
    button_temp[0] = strdup( "Sound: OFF" );
    button_temp[1] = strdup( "Sound: ON" );
    group_add_button( gui->opt_menu, ID_C_SOUND, sx, sy, button_temp, tr("Sound"), 2 ); sy += 30;
    group_add_button( gui->opt_menu, ID_C_SOUND_INC, sx, sy, 0, tr("Sound Volume Up"), 2 ); sy += 30;
    group_add_button( gui->opt_menu, ID_C_SOUND_DEC, sx, sy, 0, tr("Sound Volume Down"), 2 ); sy += 30;
    free( button_temp[0] );
    free( button_temp[1] );
    free( button_temp );
    button_temp = calloc( 2, sizeof( char * ) );
    button_temp[0] = strdup( "Music: OFF" );
    button_temp[1] = strdup( "Music: ON" );
    group_add_button( gui->opt_menu, ID_C_MUSIC, sx, sy, button_temp, tr("Music"), 2 ); sy += 30;
    free( button_temp[0] );
    free( button_temp[1] );
    free( button_temp );
    group_add_button( gui->opt_menu, ID_C_VMODE, sx, sy, 0, tr("Video Mode [v]"), 2 );
    group_hide( gui->opt_menu, 1 );
    /* диалоговой режим видео  */
    search_file_name( path, 0, "scroll_buttons", dir, "Theme", 'i' );
    search_file_name( path2, 0, "confirm_buttons", dir, "Theme", 'i' );
    gui->vmode_dlg = select_dlg_create( gui_create_frame( 210, 120 ),
			    load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
			    8, 190, 12, lbox_render_text,
			    gui_create_frame( 210, 30 ),
			    load_surf( path2, SDL_SWSURFACE, 0, 0, 0, 0 ), 20, 20,
			    ID_VMODE_OK, sdl.screen, 0, 0);
    select_dlg_hide( gui->vmode_dlg, 1 );
    /* сценарий диалога */
    search_file_name( path, 0, "scen_dlg_buttons", dir, "Theme", 'i' );
    search_file_name( path2, 0, "scroll_buttons", dir, "Theme", 'i' );
    gui->scen_dlg = fdlg_create( gui_create_frame( 180, 270 ), 160, 10,
                                 load_surf( path2, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                 20,
                                 gui_create_frame( 260, 270),
                                 load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                 ID_SCEN_OK,
                                 gui_render_scen_file_name, gui_render_scen_info,
                                 sdl.screen, 0, 0, ARRANGE_COLUMNS, 0, 0, LIST_SCENARIOS );
    fdlg_add_button( gui->scen_dlg, ID_SCEN_SETUP, 0, tr("Player Setup") );
    fdlg_hide( gui->scen_dlg, 1 );
    /* диалог кампании */
    search_file_name( path, 0, "scen_dlg_buttons", dir, "Theme", 'i' );
    search_file_name( path2, 0, "scroll_buttons", dir, "Theme", 'i' );
    gui->camp_dlg = fdlg_create( gui_create_frame( 180, 270 ), 160, 10,
                                 load_surf( path2, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                 20,
                                 gui_create_frame( 260, 270),
                                 load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                 ID_CAMP_OK,
                                 gui_render_scen_file_name, gui_render_camp_info,
                                 sdl.screen, 0, 0, ARRANGE_COLUMNS, 0, 0, LIST_CAMPAIGNS );
    fdlg_add_button( gui->camp_dlg, ID_CAMP_SETUP, 0, tr("Player Setup") );
    fdlg_hide( gui->camp_dlg, 1 );
    /* окно настройки сценария */
    search_file_name( path, 0, "scroll_buttons", dir, "Theme", 'i' );
    search_file_name( path2, 0, "ctrl_buttons", dir, "Theme", 'i' );
    search_file_name( path3, 0, "module_buttons", dir, "Theme", 'i' );
    search_file_name( path4, 0, "scen_setup_confirm_buttons", dir, "Theme", 'i' );
    gui->scen_setup = sdlg_create(
                             gui_create_frame( 120, 120 ), load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ),
                             24, 24, 20,
                             gui_create_frame( 255, 40 ),  load_surf( path2, SDL_SWSURFACE, 0, 0, 0, 0 ),
                             24, 24, ID_SCEN_SETUP_CTRL,
                             gui_create_frame( 255, 40 ),  load_surf( path3, SDL_SWSURFACE, 0, 0, 0, 0 ),
                             24, 24, ID_SCEN_SETUP_MODULE,
                             gui_create_frame( 255, 40 ),  load_surf( path4, SDL_SWSURFACE, 0, 0, 0, 0 ),
                             24, 24, ID_SCEN_SETUP_OK,
                             gui_render_player_name, gui_handle_player_select,
                             sdl.screen, 0, 0 );
    sdlg_hide( gui->scen_setup, 1 );
    /* окно настройки кампании */
    search_file_name( path, 0, "camp_setup_confirm_buttons", dir, "Theme", 'i' );
    gui->camp_setup = sdlg_camp_create(
                             gui_create_frame( 180, 40 ),  load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ),
                             24, 24, ID_CAMP_SETUP_OK,
                             gui_render_player_name, gui_handle_player_select,
                             sdl.screen, 0, 0 );
    sdlg_hide( gui->camp_setup, 1 );
    /* диалог модуля */
    search_file_name( path, 0, "confirm_buttons", dir, "Theme", 'i' );
    search_file_name( path2, 0, "scroll_buttons", dir, "Theme", 'i' );
    gui->module_dlg = fdlg_create( gui_create_frame( 120, 240 ), 160, 10,
                                 load_surf( path2, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                 20,
                                 gui_create_frame( 220, 240),
                                 load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 20, 20,
                                 ID_MODULE_OK,
                                 gui_render_file_name, gui_render_module_info,
                                 sdl.screen, 0, 0, ARRANGE_COLUMNS, 0, 0, LIST_ALL );
    fdlg_hide( gui->module_dlg, 1 );
    /* диалог о покупке */
    sprintf( path2, "%s/Theme", dir );
    if ( (gui->purchase_dlg = purchase_dlg_create( path2 )) == NULL)
	    goto failure;
    purchase_dlg_hide( gui->purchase_dlg, 1 );
    /* диалог выбора мода */
    search_file_name( path, 0, "confirm_buttons", dir, "Theme", 'i' );
    search_file_name( path2, 0, "scroll_buttons", dir, "Theme", 'i' );
    gui->mod_select_dlg = fdlg_create( gui_create_frame( 180, 208 ), 160, 10,
                                 load_surf( path2, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
                                 20,
                                 gui_create_frame( 231, 208),
                                 load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 20, 20,
                                 ID_MOD_SELECT_OK,
                                 gui_render_file_name, gui_render_mod_select_info,
                                 sdl.screen, 0, 0, ARRANGE_COLUMNS, 0, 1, LIST_ALL );
    fdlg_hide( gui->mod_select_dlg, 1 );
    /* отрегулировать позиции */
    gui_adjust();
    /* звуки */
#ifdef WITH_SOUND
    search_file_name( path, 0, "click", dir, "Sound", 's' );
    gui->wav_click = wav_load( path, 1 );
    search_file_name( path, 0, "edit", dir, "Sound", 's' );
    gui->wav_edit = wav_load( path, 1 );
#endif
    log_font = gui->font_std;
    return 1;
sdl_failure:
    fprintf( stderr, "SDL says: %s\n", SDL_GetError() );
failure:
    gui_delete();
    return 0;
}
void gui_delete()
{
    if ( gui ) {
        if ( gui->name ) free( gui->name );
        free_surf( &gui->bkgnd );
        free_surf( &gui->brief_frame );
        free_surf( &gui->wallpaper );
        free_surf( &gui->map_frame );
        free_surf( &gui->fr_luc );
        free_surf( &gui->fr_llc );
        free_surf( &gui->fr_ruc );
        free_surf( &gui->fr_rlc );
        free_surf( &gui->fr_hori );
        free_surf( &gui->fr_vert );
        free_surf( &gui->folder_icon );
        free_font( &gui->font_std );
        free_font( &gui->font_status );
        free_font( &gui->font_error );
        free_font( &gui->font_turn_info );
        free_font( &gui->font_brief );
        image_delete( &gui->cursors );
        label_delete( &gui->label_left );
        label_delete( &gui->label_center );
        label_delete( &gui->label_right );
        frame_delete( &gui->qinfo1 );
        frame_delete( &gui->qinfo2 );
        frame_delete( &gui->finfo );
        frame_delete( &gui->sinfo );
        group_delete( &gui->confirm );
        group_delete( &gui->unit_buttons );
        group_delete( &gui->split_menu );
        group_delete( &gui->deploy_window );
        group_delete( &gui->base_menu );
        group_delete( &gui->main_menu );
        fdlg_delete( &gui->load_menu );
        fdlg_delete( &gui->save_menu );
        group_delete( &gui->opt_menu );
        select_dlg_delete( &gui->vmode_dlg );
        fdlg_delete( &gui->scen_dlg );
        fdlg_delete( &gui->camp_dlg );
        sdlg_delete( &gui->scen_setup );
        sdlg_delete( &gui->camp_setup );
        fdlg_delete( &gui->module_dlg );
        fdlg_delete( &gui->mod_select_dlg );
        edit_delete( &gui->edit );
        message_dlg_delete( &gui->message_dlg );
        purchase_dlg_delete( &gui->purchase_dlg );
        headquarters_dlg_delete( &gui->headquarters_dlg );
#ifdef WITH_SOUND
        wav_free( gui->wav_click );
        wav_free( gui->wav_edit );
#endif
        free( gui ); gui = 0;
    }
}

/*
====================================================================
Переместите все окна в нужное положение согласно экрану
измерения.
====================================================================
*/
void gui_adjust()
{
    /* рамка карты */
    const SDL_PixelFormat *fmt = SDL_GetVideoSurface()->format;
    SDL_free( gui->map_frame );
    gui->map_frame = SDL_CreateRGBSurface( SDL_SWSURFACE, sdl.screen->w, sdl.screen->h- 30,
                                           fmt->BitsPerPixel,
                                           fmt->Rmask,fmt->Gmask,fmt->Bmask,fmt->Amask );
    /* информационные ярлыки */
    if ( gui->label_left != 0 )
        label_delete( &gui->label_left );
    if ( gui->label_center != 0 )
        label_delete( &gui->label_center );
    if ( gui->label_right != 0 )
        label_delete( &gui->label_right );
    gui->label_left = label_create( gui_create_frame( sdl.screen->w / 3, 30 ), 160, gui->font_std, sdl.screen, 0, 0 );
    gui->label_right = label_create( gui_create_frame( sdl.screen->w / 3, 30 ), 160, gui->font_std, sdl.screen, 0, 0 );
    gui->label_center = label_create( gui_create_frame( sdl.screen->w - 2 * sdl.screen->w / 3, 30 ),
                                             160, gui->font_std, sdl.screen, 0, 0 );
    label_move( gui->label_center, sdl.screen->w / 3, 0 );
    label_move( gui->label_right, sdl.screen->w - sdl.screen->w / 3, 0 );
    char str[MAX_NAME];
    snprintf( str, MAX_NAME, tr("Mod: %s"), config.mod_name );
    label_write( gui->label_left, gui->font_std, str );
    /* информация об устройстве */
    frame_move( gui->qinfo1, 10, sdl.screen->h - 10 - gui->qinfo1->img->img->h );
    frame_move( gui->qinfo2, 10, sdl.screen->h - 20 - gui->qinfo1->img->img->h * 2 );
    /* полная информация */
    frame_move( gui->finfo, ( sdl.screen->w - gui->finfo->img->img->w ) >> 1, ( sdl.screen->h - gui->finfo->img->img->h ) >> 1 );
    /* основное меню */
    group_move( gui->base_menu, sdl.screen->w - 10 - gui->base_menu->frame->img->img->w,
                                sdl.screen->h - 10 - gui->base_menu->frame->img->img->h );
    /* информация о сценарии */
    frame_move( gui->sinfo, ( sdl.screen->w - gui->sinfo->img->img->w ) >> 1, ( sdl.screen->h - gui->sinfo->img->img->h ) >> 1 );
    /* окно подтверждения */
    group_move( gui->confirm, ( sdl.screen->w - gui->confirm->frame->img->img->w ) >> 1, ( sdl.screen->h - gui->confirm->frame->img->img->h ) >> 1 );
    /* окно развертывания */
    group_move( gui->deploy_window, ( sdl.screen->w - gui->deploy_window->frame->img->img->w ) - 10,
                ( sdl.screen->h - gui->deploy_window->frame->img->img->h ) / 2 );
    /* edit */
    edit_move( gui->edit, (sdl.screen->w - gui->edit->label->frame->img->img->w ) >> 1, 50 );
    /* диалог сообщения */
    if ( gui->message_dlg != 0 )
        message_dlg_delete( &gui->message_dlg );
    char path[MAX_PATH];
    sprintf( path, "Default/Theme" );
    gui->message_dlg = message_dlg_create( path );
    message_dlg_move( gui->message_dlg, 2, sdl.screen->h - message_dlg_get_height(gui->message_dlg) - 2);
    message_dlg_hide( gui->message_dlg, 1 );
    message_dlg_draw_text( gui->message_dlg );
    /* выберите диалоги */
    select_dlg_move( gui->vmode_dlg,
                    (sdl.screen->w - select_dlg_get_width(gui->vmode_dlg)) /2,
                    (sdl.screen->h - select_dlg_get_height(gui->vmode_dlg)) /2);
    /* диалог загрузки меню */
    fdlg_move( gui->load_menu, ( sdl.screen->w - gui->load_menu->group->frame->img->img->w ) / 2,
               ( sdl.screen->h - ( gui->load_menu->group->frame->img->img->h +
                                                  gui->load_menu->lbox->group->frame->img->img->h ) ) / 2 );
    /* сохранить диалог меню */
    fdlg_move( gui->save_menu, ( sdl.screen->w - gui->save_menu->group->frame->img->img->w ) / 2,
               ( sdl.screen->h - ( gui->save_menu->group->frame->img->img->h +
                                                  gui->save_menu->lbox->group->frame->img->img->h ) ) / 2 );
    /* сценарий диалога */
    fdlg_move( gui->scen_dlg, ( sdl.screen->w - ( gui->scen_dlg->group->frame->img->img->w  +
                                                  gui->scen_dlg->lbox->group->frame->img->img->w ) ) / 2,
               ( sdl.screen->h - gui->scen_dlg->group->frame->img->img->h ) / 2 );
    /* диалог кампании */
    fdlg_move( gui->camp_dlg, ( sdl.screen->w - ( gui->camp_dlg->group->frame->img->img->w  +
                                                  gui->camp_dlg->lbox->group->frame->img->img->w ) ) / 2,
               ( sdl.screen->h - gui->camp_dlg->group->frame->img->img->h ) / 2 );
    /* настройка сценария */
    sdlg_move( gui->scen_setup, ( sdl.screen->w - ( gui->scen_setup->list->group->frame->img->img->w + gui->scen_setup->ctrl->frame->img->img->w ) ) / 2,
                           ( sdl.screen->h - ( gui->scen_setup->list->group->frame->img->img->h ) ) / 2 );
    /* настройка кампании */
    sdlg_move( gui->camp_setup, ( sdl.screen->w - ( gui->camp_setup->confirm->frame->img->img->w ) ) / 2,
                           ( sdl.screen->h - ( gui->camp_setup->confirm->frame->img->img->h ) ) / 2 );
    /* диалог модуля */
    fdlg_move( gui->module_dlg, ( sdl.screen->w - ( gui->module_dlg->group->frame->img->img->w  +
                                                  gui->module_dlg->lbox->group->frame->img->img->w ) ) / 2,
               ( sdl.screen->h - gui->module_dlg->group->frame->img->img->h ) / 2 );
    /* сценарий диалога */
    fdlg_move( gui->mod_select_dlg, ( sdl.screen->w - ( gui->mod_select_dlg->group->frame->img->img->w  +
                                                  gui->mod_select_dlg->lbox->group->frame->img->img->w ) ) / 2,
               ( sdl.screen->h - gui->mod_select_dlg->group->frame->img->img->h ) / 2 );

    /* диалог о покупке */
    purchase_dlg_move(gui->purchase_dlg,
		(sdl.screen->w - purchase_dlg_get_width(gui->purchase_dlg)) /2,
		(sdl.screen->h - purchase_dlg_get_height(gui->purchase_dlg)) /2);

    /* окно штаб-квартиры */
    headquarters_dlg_move( gui->headquarters_dlg,
                (sdl.screen->w - headquarters_dlg_get_width(gui->headquarters_dlg)) / 2,
                (sdl.screen->h - headquarters_dlg_get_height(gui->headquarters_dlg)) / 2);
}

/*
====================================================================
Измените всю графическую графику GUI на ту, что находится в gfx / theme/path.
====================================================================
*/
/*int gui_change( const char *path )
{
}*/

/*
====================================================================
Скрыть / нарисовать с экрана/на экран
====================================================================
*/
void gui_get_bkgnds()
{
    label_get_bkgnd( gui->label_left );
    label_get_bkgnd( gui->label_center );
    label_get_bkgnd( gui->label_right );
    frame_get_bkgnd( gui->qinfo1 );
    frame_get_bkgnd( gui->qinfo2 );
    frame_get_bkgnd( gui->finfo );
    frame_get_bkgnd( gui->sinfo );
    group_get_bkgnd( gui->base_menu );
    group_get_bkgnd( gui->main_menu );
    fdlg_get_bkgnd( gui->load_menu );
    fdlg_get_bkgnd( gui->save_menu );
    group_get_bkgnd( gui->opt_menu );
    group_get_bkgnd( gui->unit_buttons);
    group_get_bkgnd( gui->split_menu );
    edit_get_bkgnd( gui->edit );
    message_dlg_get_bkgnd( gui->message_dlg );
    group_get_bkgnd( gui->confirm );
    group_get_bkgnd( gui->deploy_window );
    select_dlg_get_bkgnd( gui->vmode_dlg );
    fdlg_get_bkgnd( gui->scen_dlg );
    fdlg_get_bkgnd( gui->camp_dlg );
    fdlg_get_bkgnd( gui->module_dlg );
    fdlg_get_bkgnd( gui->mod_select_dlg );
    sdlg_get_bkgnd( gui->scen_setup );
    sdlg_get_bkgnd( gui->camp_setup );
    purchase_dlg_get_bkgnd( gui->purchase_dlg );
    headquarters_dlg_get_bkgnd( gui->headquarters_dlg );
    image_get_bkgnd( gui->cursors );
}
void gui_draw_bkgnds()
{
    label_draw_bkgnd( gui->label_left );
    label_draw_bkgnd( gui->label_center );
    label_draw_bkgnd( gui->label_right );
    frame_draw_bkgnd( gui->qinfo1 );
    frame_draw_bkgnd( gui->qinfo2 );
    frame_draw_bkgnd( gui->finfo );
    frame_draw_bkgnd( gui->sinfo );
    group_draw_bkgnd( gui->base_menu );
    group_draw_bkgnd( gui->main_menu );
    fdlg_draw_bkgnd( gui->load_menu );
    fdlg_draw_bkgnd( gui->save_menu );
    group_draw_bkgnd( gui->opt_menu );
    group_draw_bkgnd( gui->unit_buttons);
    group_draw_bkgnd( gui->split_menu );
    edit_draw_bkgnd( gui->edit );
    message_dlg_draw_bkgnd( gui->message_dlg );
    group_draw_bkgnd( gui->confirm );
    group_draw_bkgnd( gui->deploy_window );
    select_dlg_draw_bkgnd( gui->vmode_dlg );
    fdlg_draw_bkgnd( gui->scen_dlg );
    fdlg_draw_bkgnd( gui->camp_dlg );
    fdlg_draw_bkgnd( gui->module_dlg );
    fdlg_draw_bkgnd( gui->mod_select_dlg );
    sdlg_draw_bkgnd( gui->scen_setup );
    sdlg_draw_bkgnd( gui->camp_setup );
    purchase_dlg_draw_bkgnd( gui->purchase_dlg );
    headquarters_dlg_draw_bkgnd( gui->headquarters_dlg );
    image_draw_bkgnd( gui->cursors );
}
void gui_draw()
{
    label_draw( gui->label_left );
    label_draw( gui->label_center );
    label_draw( gui->label_right );
    frame_draw( gui->qinfo1 );
    frame_draw( gui->qinfo2 );
    frame_draw( gui->finfo );
    frame_draw( gui->sinfo );
    group_draw( gui->base_menu );
    group_draw( gui->main_menu );
    fdlg_draw( gui->load_menu );
    fdlg_draw( gui->save_menu );
    group_draw( gui->opt_menu );
    group_draw( gui->unit_buttons);
    group_draw( gui->split_menu );
    edit_draw( gui->edit );
    message_dlg_draw( gui->message_dlg );
    group_draw( gui->confirm );
    group_draw( gui->deploy_window );
    select_dlg_draw( gui->vmode_dlg );
    fdlg_draw( gui->scen_dlg );
    fdlg_draw( gui->camp_dlg );
    fdlg_draw( gui->module_dlg );
    fdlg_draw( gui->mod_select_dlg );
    sdlg_draw( gui->scen_setup );
    sdlg_draw( gui->camp_setup );
    purchase_dlg_draw( gui->purchase_dlg );
    headquarters_dlg_draw( gui->headquarters_dlg );
    image_draw( gui->cursors );
}

/*
====================================================================
Переместить курсор.
====================================================================
*/
void gui_move_cursor( int cx, int cy )
{
    if ( cx - hspots[cursor].x < 0 ) cx = hspots[cursor].x;
    if ( cy - hspots[cursor].y < 0 ) cy = hspots[cursor].y;
    if ( cx + hspots[cursor].x >= sdl.screen->w ) cx = sdl.screen->w - hspots[cursor].x;
    if ( cy + hspots[cursor].y >= sdl.screen->h ) cy = sdl.screen->h - hspots[cursor].y;
/*    if ( cx - hspots[cursor].x + gui->cursors->bkgnd->surf_rect.w >= sdl.screen->w )
        cx = sdl.screen->w - gui->cursors->bkgnd->surf_rect.w + hspots[cursor].x;
    if ( cy - hspots[cursor].y + gui->cursors->bkgnd->surf_rect.h >= sdl.screen->h )
        cy = sdl.screen->h - gui->cursors->bkgnd->surf_rect.h + hspots[cursor].y;*/
    image_move( gui->cursors, cx - hspots[cursor].x, cy - hspots[cursor].y );
}

/*
====================================================================
Установить курсор.
====================================================================
*/
void gui_set_cursor( int type )
{
    int move = 0;
    int x = -1, y;
    if ( type >= CURSOR_COUNT ) type = 0;
    if ( cursor != type ) {
        x = gui->cursors->bkgnd->surf_rect.x + hspots[cursor].x;
        y = gui->cursors->bkgnd->surf_rect.y + hspots[cursor].y;
        move = 1;
    }
    cursor = type;
    if ( move )
        gui_move_cursor( x, y );
    image_set_region( gui->cursors, 22 * type, 0, 22, 22 );
}

/*
====================================================================
Обрабатывать события.
====================================================================
*/
int gui_handle_motion( int cx, int cy )
{
    int ret = 1;
    if ( !group_handle_motion( gui->base_menu, cx, cy ) )
    if ( !group_handle_motion( gui->main_menu, cx, cy ) )
    if ( !fdlg_handle_motion( gui->load_menu, cx, cy ) )
    if ( !fdlg_handle_motion( gui->save_menu, cx, cy ) )
    if ( !group_handle_motion( gui->opt_menu, cx, cy ) )
    if ( !group_handle_motion( gui->unit_buttons, cx, cy ) )
    if ( !group_handle_motion( gui->split_menu, cx, cy ) )
    if ( !group_handle_motion( gui->confirm, cx, cy ) )
    if ( !message_dlg_handle_motion( gui->message_dlg, cx, cy ) )
    if ( !select_dlg_handle_motion( gui->vmode_dlg, cx, cy ) )
    if ( !fdlg_handle_motion( gui->scen_dlg, cx, cy  ) )
    if ( !fdlg_handle_motion( gui->camp_dlg, cx, cy  ) )
    if ( !fdlg_handle_motion( gui->module_dlg, cx, cy  ) )
    if ( !fdlg_handle_motion( gui->mod_select_dlg, cx, cy  ) )
    if ( !sdlg_handle_motion( gui->scen_setup, cx, cy  ) )
    if ( !sdlg_handle_motion( gui->camp_setup, cx, cy  ) )
    if ( !purchase_dlg_handle_motion( gui->purchase_dlg, cx, cy  ) )
    if ( !headquarters_dlg_handle_motion( gui->headquarters_dlg, cx, cy  ) )
        ret = 0;
    /* указатель */
    gui_move_cursor( cx, cy );
    return ret;
}
/** Если возвращается значение true, то кнопка @должна быть настроена для обработки вверх по потоку. */
int  gui_handle_button( int button_id, int cx, int cy, Button **button )
{
    int ret = 1;
    *button = 0;
    if ( !group_handle_button( gui->base_menu, button_id, cx, cy, button ) )
    if ( !group_handle_button( gui->main_menu, button_id, cx, cy, button ) )
    if ( !fdlg_handle_button( gui->load_menu, button_id, cx, cy, button ) )
    if ( !fdlg_handle_button( gui->save_menu, button_id, cx, cy, button ) )
    if ( !group_handle_button( gui->opt_menu, button_id, cx, cy, button ) )
    if ( !group_handle_button( gui->unit_buttons, button_id, cx, cy, button ) )
    if ( !group_handle_button( gui->split_menu, button_id, cx, cy, button ) )
    if ( !group_handle_button( gui->confirm, button_id, cx, cy, button ) )
    if ( !group_handle_button( gui->deploy_window, button_id, cx, cy, button ) )
    if ( !message_dlg_handle_button( gui->message_dlg, button_id, cx, cy, button ) )
    if ( !select_dlg_handle_button( gui->vmode_dlg, button_id, cx, cy, button ) )
    if ( !fdlg_handle_button( gui->scen_dlg, button_id, cx, cy, button ) )
    if ( !fdlg_handle_button( gui->camp_dlg, button_id, cx, cy, button ) )
    if ( !fdlg_handle_button( gui->module_dlg, button_id, cx, cy, button ) )
    if ( !fdlg_handle_button( gui->mod_select_dlg, button_id, cx, cy, button ) )
    if ( !sdlg_handle_button( gui->scen_setup, button_id, cx, cy, button ) )
    if ( !sdlg_handle_button( gui->camp_setup, button_id, cx, cy, button ) )
    if ( !purchase_dlg_handle_button( gui->purchase_dlg, button_id, cx, cy, button ) )
    if ( !headquarters_dlg_handle_button( gui->headquarters_dlg, button_id, cx, cy, button ) )
        ret = 0;
    return ret;
}
void gui_update( int ms )
{
}

/*
====================================================================
Установите окно быстрой информации с информацией об этом устройстве и установите hide = 0.
====================================================================
*/
void gui_show_quick_info( Frame *qinfo, Unit *unit )
{
    int i, len;
    char str[64];
    /* Очистка */
    SDL_FillRect( qinfo->contents, 0, 0x0 );
    /* иконка */
    DEST( qinfo->contents,
          6 + ( ( hex_w - unit->prop.icon_w ) >> 1 ), ( ( qinfo->contents->h - unit->prop.icon_h ) >> 1 ),
          unit->prop.icon_w, unit->prop.icon_h );
    SOURCE( unit->prop.icon, 0, 0 );
    blit_surf();
    /* сила */
    if ( unit->prop.icon_type == UNIT_ICON_FIXED )
        DEST( qinfo->contents,
              6 + ( ( hex_w - unit_info_icons->str_w ) >> 1 ),
              qinfo->contents->h - unit_info_icons->str_h - 5,
              unit_info_icons->str_w, unit_info_icons->str_h )
    else
        DEST( qinfo->contents,
              6 + ( ( hex_w - unit_info_icons->str_w ) >> 1 ),
              ( ( qinfo->contents->h - unit->prop.icon_h ) >> 1 ) + unit->prop.icon_h,
              unit_info_icons->str_w, unit_info_icons->str_h )
    if ( cur_player && player_is_ally( cur_player, unit->player ) )
        if ( (config.use_core_units) && (camp_loaded != NO_CAMPAIGN) && (cur_player->ctrl == PLAYER_CTRL_HUMAN) &&
            ( unit->core == AUXILIARY ) )
            SOURCE( unit_info_icons->str,
                    unit_info_icons->str_w * ( unit->str - 1 ),
                    unit_info_icons->str_h * (unit->player->strength_row + 1))
        else
            SOURCE( unit_info_icons->str,
                    unit_info_icons->str_w * ( unit->str - 1 ),
                    unit_info_icons->str_h * unit->player->strength_row)
    else
        SOURCE( unit_info_icons->str, unit_info_icons->str_w * ( unit->str - 1 ),
                unit_info_icons->str_h * unit->player->strength_row )
    blit_surf();
    /* национальный флаг */
    DEST( qinfo->contents, 6, 6, nation_flag_width, nation_flag_height );
    SOURCE( nation_flags, unit->nation->flag_offset, 0 );
    blit_surf();
    /* имя */
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    write_text( gui->font_std, qinfo->contents, 12 + hex_w, 10, unit->name, 255 );
    write_text( gui->font_std, qinfo->contents, 12 + hex_w, 22, unit->prop.name, 255 );
    /* статус отображения боеприпасов, топлива, окопанности в нижнем левом информационном окне */
    gui->font_status->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    if ( cur_player && !player_is_ally( unit->player, cur_player ) )
        len = sprintf( str, GS_AMMO "%i " GS_FUEL "%i " GS_ENTR "%i  " GS_NOEXP GS_NOEXP GS_NOEXP GS_NOEXP GS_NOEXP " ", unit->cur_ammo, unit->cur_fuel, unit->entr );
//закоментировано не отображение у врагов боеприпасов и топлива
//len = sprintf( str, GS_AMMO "? " GS_FUEL "? " GS_ENTR "%i  " GS_NOEXP GS_NOEXP GS_NOEXP GS_NOEXP GS_NOEXP " ", unit->entr );
    else
        if ( unit_check_fuel_usage( unit ) )
            len = sprintf( str, GS_AMMO "%i " GS_FUEL "%i " GS_ENTR "%i  " GS_NOEXP GS_NOEXP GS_NOEXP GS_NOEXP GS_NOEXP " ", unit->cur_ammo, unit->cur_fuel, unit->entr );
        else
            len = sprintf( str, GS_AMMO "%i " GS_FUEL "- " GS_ENTR "%i  " GS_NOEXP GS_NOEXP GS_NOEXP GS_NOEXP GS_NOEXP " ", unit->cur_ammo, unit->entr );
    for ( i = 0; i < unit->exp_level; i++ )
        str[len - 6 + i] = (char)CharExp;
    if ( unit->exp_level < 5 )
        str[len - 6 + i] = (char)(CharExpGrowth + (unit->exp % 100 / 20));
    write_text( gui->font_status, qinfo->contents, 12 + hex_w, 36, str, 255 );
    /* Показать */
    frame_apply( qinfo );
    frame_hide( qinfo, 0 );
}

/*
====================================================================
Нарисуйте на этикетке ожидаемые убытки.
====================================================================
*/
void gui_show_expected_losses( Unit *att, Unit *def, int att_dam, int def_dam )
{
    char str[128];
    SDL_Surface *contents = gui->label_left->frame->contents;
    SDL_FillRect( contents, 0, 0x0 );
    /* флаг атакующего */
    DEST( contents, 10, ( contents->h - nation_flag_height ) >> 1, nation_flag_width, nation_flag_height );
    SOURCE( nation_flags, att->nation->flag_offset, 0 );
    blit_surf();
    /* флаг защитника */
    DEST( contents, contents->w - 10 - nation_flag_width, ( contents->h - nation_flag_height ) >> 1,
          nation_flag_width, nation_flag_height );
    SOURCE( nation_flags, def->nation->flag_offset, 0 );
    blit_surf();
    /* убивает */
    sprintf( str, tr("%i   CASUALTIES   %i"), att_dam, def_dam );
    gui->font_error->align = ALIGN_X_CENTER | ALIGN_Y_CENTER;
    write_text( gui->font_error, contents, contents->w >> 1, contents->h >> 1, str, 255 );
    /* Показать */
    frame_apply( gui->label_left->frame );
}

/*
====================================================================
Нарисуйте фактические потери на этикетке.
====================================================================
*/
void gui_show_actual_losses( Unit *att, Unit *def,
    int att_suppr, int att_dam, int def_suppr, int def_dam )
{
    char str[128];
    SDL_Surface *contents_atk = gui->label_left->frame->contents;
    SDL_FillRect( contents_atk, 0, 0x0 );
    SDL_Surface *contents_def = gui->label_right->frame->contents;
    SDL_FillRect( contents_def, 0, 0x0 );
    /* флаг атакующего */
    DEST( contents_atk, 20, ( contents_atk->h - nation_flag_height ) >> 1, nation_flag_width, nation_flag_height );
    SOURCE( nation_flags, att->nation->flag_offset, 0 );
    blit_surf();
    /* флаг защитника */
    DEST( contents_def, 20, ( contents_def->h - nation_flag_height ) >> 1,
          nation_flag_width, nation_flag_height );
    SOURCE( nation_flags, def->nation->flag_offset, 0 );
    blit_surf();
    /* убивает */
    sprintf( str, tr("%i   KILLED"), att_dam );
    gui->font_std->align = ALIGN_X_CENTER | ALIGN_Y_CENTER;
    write_text( gui->font_std, contents_atk, contents_atk->w >> 1, contents_atk->h >> 1, str, 255 );
    sprintf( str, tr("%i   KILLED"), def_dam );
    write_text( gui->font_std, contents_def, contents_def->w >> 1, contents_def->h >> 1, str, 255 );
    /* Показать */
    frame_apply( gui->label_left->frame );
    frame_apply( gui->label_right->frame );
    /* подавление */
    if (att_suppr>0||def_suppr>0)
    {
        contents_atk = gui->label_left->frame->contents;
        SDL_FillRect( contents_atk, 0, 0x0 );
        contents_def = gui->label_right->frame->contents;
        SDL_FillRect( contents_def, 0, 0x0 );
        /* флаг атакующего */
        DEST( contents_atk, 20, ( contents_atk->h - nation_flag_height ) >> 1, nation_flag_width, nation_flag_height );
        SOURCE( nation_flags, att->nation->flag_offset, 0 );
        blit_surf();
        /* флаг защитника */
        DEST( contents_def, 20, ( contents_def->h - nation_flag_height ) >> 1,
              nation_flag_width, nation_flag_height );
        SOURCE( nation_flags, def->nation->flag_offset, 0 );
        blit_surf();
        /* убивает */
        sprintf( str, tr("%i   SURPRESSED"), att_suppr );
        gui->font_std->align = ALIGN_X_CENTER | ALIGN_Y_CENTER;
        write_text( gui->font_std, contents_atk, contents_atk->w >> 1, contents_atk->h >> 1, str, 255 );
        sprintf( str, tr("%i   SURPRESSED"), def_suppr );
        write_text( gui->font_std, contents_def, contents_def->w >> 1, contents_def->h >> 1, str, 255 );
        /* Показать */
        frame_apply( gui->label_left->frame );
        frame_apply( gui->label_right->frame );
    }
}

/*
====================================================================
Показать полное информационное окно.
====================================================================
*/
void gui_show_full_info( Frame *dest, Unit *unit )
{
    char str[128];
    int border = 10, offset;
    offset = 140;
    int x, y, i;
    SDL_Surface *contents = dest->contents;
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    /* Чисто */
    SDL_FillRect( contents, 0, 0x0 );
    /* Иконка */
    x = border + 20; y = border;
    DEST( contents,
          x + ( ( hex_w - unit->prop.icon_w ) >> 1 ), y + ( ( ( hex_h - unit->prop.icon_h ) >> 1 ) ),
          unit->prop.icon_w, unit->prop.icon_h );
    SOURCE( unit->prop.icon, 0, 0 );
    blit_surf();
    /* национальный флаг */
    DEST( contents, x, y, nation_flag_width, nation_flag_height );
    SOURCE( nation_flags, unit->nation->flag_offset, 0 );
    blit_surf();
    /* имя и тип */
    x = border; y = border + hex_h;
    write_line( contents, gui->font_std, unit->name, x, &y );
    write_line( contents, gui->font_std, unit->prop.name, x, &y );
    write_line( contents, gui->font_std, unit_classes[unit->prop.class].name, x, &y );
    if (!config.use_core_units)
        y += 10;
    else
        write_line( contents, gui->font_std, unit->core?"Core":"Auxiliary", x, &y );
    sprintf( str, tr("%s Movement"), mov_types[unit->prop.mov_type].name );
    write_line( contents, gui->font_std, str, x, &y );
    sprintf( str, tr("%s Target"), trgt_types[unit->prop.trgt_type].name );
    write_line( contents, gui->font_std, str, x, &y );
    /* ammo, fuel, spot, mov, ini, range */
    x = border + hex_w + 130; y = border;
    if ( unit->prop.ammo == 0 )
        sprintf( str, tr("Ammo:       N.A.") );
    else
        if ( cur_player == 0 || player_is_ally( cur_player, unit->player ) )
            sprintf( str, tr("Ammo:     %02i/%02i"), unit->cur_ammo, unit->prop.ammo );
        else
            sprintf( str, tr("Ammo:        %02i"), unit->prop.ammo );
    write_line( contents, gui->font_std, str, x, &y );
    if ( unit->prop.fuel == 0 )
        sprintf( str, tr("Fuel:       N.A.") );
    else
        if ( cur_player == 0 || player_is_ally( cur_player, unit->player ) )
            sprintf( str, tr("Fuel:     %2i/%2i"), unit->cur_fuel, unit->prop.fuel );
        else
            sprintf( str, tr("Fuel:        %2i"), unit->prop.fuel );
    write_line( contents, gui->font_std, str, x, &y );
    y += 10;
    sprintf( str, tr("Spotting:    %2i"), unit->prop.spt );
    write_line( contents, gui->font_std, str, x, &y );
    sprintf( str, tr("Movement:    %2i"), unit->prop.mov );
    write_line( contents, gui->font_std, str, x, &y );
    sprintf( str, tr("Initiative:  %2i"), unit->prop.ini );
    write_line( contents, gui->font_std, str, x, &y );
    sprintf( str, tr("Range:       %2i"), unit->prop.rng );
    write_line( contents, gui->font_std, str, x, &y );
    y += 10;
    sprintf( str, tr("Experience: %3i"), unit->exp );
    write_line( contents, gui->font_std, str, x, &y );
    sprintf( str, tr("Entrenchment: %i"), unit->entr );
    write_line( contents, gui->font_std, str, x, &y );
    /* атака/оборона */
    x = border + hex_w + 130 + 140; y = border;
    for ( i = 0; i < trgt_type_count; i++ ) {
        if ( unit->prop.atks[i] < 0 )
            sprintf( str, tr("%6s Attack:[%2i]"), trgt_types[i].name, -unit->prop.atks[i] );
        else
            sprintf( str, tr("%6s Attack:  %2i"), trgt_types[i].name, unit->prop.atks[i] );
        write_line( contents, gui->font_std, str, x, &y );
    }
    y += 10;
    sprintf( str, tr("Ground Defense: %2i"), unit->prop.def_grnd );
    write_line( contents, gui->font_std, str, x, &y );
    sprintf( str, tr("Air Defense:    %2i"), unit->prop.def_air );
    write_line( contents, gui->font_std, str, x, &y );
    sprintf( str, tr("Close Defense:  %2i"), unit->prop.def_cls );
    write_line( contents, gui->font_std, str, x, &y );
    y += 10;
    sprintf( str, tr("Suppression:    %2i"), unit->turn_suppr );
    write_line( contents, gui->font_std, str, x, &y );
    /* транспортер */
    if ( unit->trsp_prop.id != 0 ) {
        /* икона */
        x = border + 20; y = border + offset;
        DEST( contents,
              x + ( ( hex_w - unit->trsp_prop.icon_w ) >> 1 ), y + ( ( ( hex_h - unit->trsp_prop.icon_h ) >> 1 ) ),
              unit->trsp_prop.icon_w, unit->trsp_prop.icon_h );
        SOURCE( unit->trsp_prop.icon, 0, 0 );
        blit_surf();
        /* имя и тип */
        x = border; y = border + hex_h + offset;
        write_line( contents, gui->font_std, unit->trsp_prop.name, x, &y );
        write_line( contents, gui->font_std, unit_classes[unit->trsp_prop.class].name, x, &y );
        y += 6;
        sprintf( str, tr("%s Movement"), mov_types[unit->trsp_prop.mov_type].name );
        write_line( contents, gui->font_std, str, x, &y );
        sprintf( str, tr("%s Target"), trgt_types[unit->trsp_prop.trgt_type].name );
        write_line( contents, gui->font_std, str, x, &y );
        /* spt, mov, ini, rng */
        x = border + hex_w + 130; y = border + offset;
        if ( cur_player == 0 || player_is_ally( cur_player, unit->player ) )
            sprintf( str, tr("Fuel:     %2i/%2i"), unit->cur_fuel, unit->trsp_prop.fuel );
        else
            sprintf( str, tr("Fuel:        %2i"), unit->prop.fuel );
        write_line( contents, gui->font_std, str, x, &y );
        y += 10;
        sprintf( str, tr("Spotting:    %2i"), unit->trsp_prop.spt );
        write_line( contents, gui->font_std, str, x, &y );
        sprintf( str, tr("Movement:    %2i"), unit->trsp_prop.mov );
        write_line( contents, gui->font_std, str, x, &y );
        sprintf( str, tr("Initiative:  %2i"), unit->trsp_prop.ini );
        write_line( contents, gui->font_std, str, x, &y );
        sprintf( str, tr("Range:       %2i"), unit->trsp_prop.rng );
        write_line( contents, gui->font_std, str, x, &y );
        /* атака и защита */
        x = border + hex_w + 130 + 140; y = border + offset;
        for ( i = 0; i < trgt_type_count; i++ ) {
            sprintf( str, tr("%6s Attack:  %2i"), trgt_types[i].name, unit->trsp_prop.atks[i] );
            write_line( contents, gui->font_std, str, x, &y );
        }
        y += 10;
        sprintf( str, tr("Ground Defense: %2i"), unit->trsp_prop.def_grnd );
        write_line( contents, gui->font_std, str, x, &y );
        sprintf( str, tr("Air Defense:    %2i"), unit->trsp_prop.def_air );
        write_line( contents, gui->font_std, str, x, &y );
        sprintf( str, tr("Close Defense:  %2i"), unit->trsp_prop.def_cls );
        write_line( contents, gui->font_std, str, x, &y );
    }
    /* боевые награды */
    if (config.use_core_units && (camp_loaded != NO_CAMPAIGN) && unit->core)
    {
        x = border; y = border + 255;
        sprintf( str, tr("Battle Honors:") );
        write_line( contents, gui->font_std, str, x, &y );
        x = border + 120; y = border + 255;
        int i = 0, j, stars_number = 0, len;
        char mem_str[128];
        /* рассчитать звезды */
        while (i < 5 && !STRCMP(unit->star[i],""))
        {
            if (i > 0)
                if (STRCMP(unit->star[i],unit->star[i - 1]))
                    stars_number++;
                else
                {
                    x = border + 120;
                    strcpy(mem_str, "%s ");
                    for (j = 1;j < stars_number;j++)
                    {
                        strcat(mem_str,GS_STAR);
                        strcat(mem_str," ");
                    }
                    len = sprintf( str, mem_str, unit->star[i - 1] );
                    for (j = 1;j < stars_number;j++)
                        str[len - (j * 2)] = (char)CharStar;

                    write_line( contents, gui->font_std, str, x, &y );
                    stars_number = 1;
                }
            else
                stars_number = 1;
            i++;
        }
        /* последняя строка, напечатанная в информационном окне */
        if (i > 0)
        {
            x = border + 120;
            strcpy(mem_str, "%s ");
            for (j = 1;j < stars_number;j++)
            {
                strcat(mem_str,GS_STAR);
                strcat(mem_str," ");
            }
            len = sprintf( str, mem_str, unit->star[i - 1] );
            for (j = 1;j < stars_number;j++)
                str[len - (j * 2)] = (char)CharStar;
            write_line( contents, gui->font_std, str, x, &y );
        }
    }
    /* флаги юнитов */
    x = border + 240; y = border + 255;
    sprintf( str, tr("Abilities: ") );
    write_line( contents, gui->font_std, str, x, &y );
    x = border + 240 + 100; y = border + 255;
    i = 0;
    while ( fct_units[i].string[0] != 'X' ) {
        if ( unit_has_flag( unit->sel_prop, fct_units[i].string ) ) {
            strcpy( str, fct_units[i].string );
            x = border + 270 + 70;
            write_line( contents, gui->font_std, str, x, &y );
        }
        i++;
    }
    /* Показать */
    frame_apply( dest );
    frame_hide( dest, 0 );
}

/*
====================================================================
Показать информационное окно сценария.
====================================================================
*/
void gui_show_scen_info()
{
    Text *text;
    char str[128];
    int border = 10, i;
    int x = border, y = border;
    SDL_Surface *contents = gui->sinfo->contents;
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    SDL_FillRect( contents, 0, 0x0 );
    /* папка мод  */
    sprintf( str, tr("MOD: %s"), scen_info->mod_name );
    write_line( contents, gui->font_std, str, x, &y ); y += 10;
    /* заглавие */
    write_line( contents, gui->font_std, scen_info->name, x, &y ); y += 10;
    /* описание */
    text = create_text( gui->font_std, scen_info->desc, contents->w - border*2 );
    for ( i = 0; i < text->count; i++ )
        write_line( contents, gui->font_std, text->lines[i], x, &y );
    delete_text( text );
    /* ход и дата */
    y += 10;
    scen_get_date( str );
    write_line( contents, gui->font_std, str, x, &y );
    if ( turn + 1 < scen_info->turn_limit )
        sprintf( str, tr("Turns Left: %i"), scen_info->turn_limit - turn );
    else
        sprintf( str, tr("Turns Left: Last Turn") );
    write_line( contents, gui->font_std, str, x, &y );
    /* результат сценария в конце */
    if ( turn + 1 > scen_info->turn_limit ) {
        y += 10;
        write_line( contents, gui->font_std, tr("Result: "), x, &y );
        text = create_text( gui->font_std, scen_message, contents->w - border*2 );
        for ( i = 0; i < text->count; i++ )
            write_line( contents, gui->font_std, text->lines[i], x, &y );
        delete_text( text );
    }
    /* игроки */
    if ( cur_player ) {
        y += 10;
        sprintf( str,     tr("Current Player:  %s"), cur_player->name );
        write_line( contents, gui->font_std, str, x, &y );
        if ( players_test_next() ) {
            sprintf( str, tr("Next Player:     %s"), players_test_next()->name );
            write_line( contents, gui->font_std, str, x, &y );
        }
    }
    /* Погода */
    y += 10;
    sprintf( str, tr("Weather:  %s (%s)"),
            (( turn < scen_info->turn_limit ) ?
             weather_types[cur_weather].name : tr("n/a")),
             ( turn < scen_info->turn_limit ) ?
             weather_types[cur_weather].ground_conditions : tr("n/a") );
    write_line( contents, gui->font_std, str, x, &y );
    sprintf( str, tr("Forecast: %s (%s)"),
            ((turn+1 < scen_info->turn_limit ) ?
             weather_types[scen_get_forecast()].name : tr("n/a")),
             ( turn < scen_info->turn_limit ) ?
             weather_types[scen_get_weather()].ground_conditions : tr("n/a") );
    write_line( contents, gui->font_std, str, x, &y );
    /* Показать */
    frame_apply( gui->sinfo );
    frame_hide( gui->sinfo, 0 );
}

/*
====================================================================
Показать явные условия победы и использовать информационное окно сценария для
это.
====================================================================
*/
void gui_render_subcond( VSubCond *cond, char *str )
{
    switch( cond->type ) {
        case VSUBCOND_TURNS_LEFT:
            sprintf( str, tr("%i turns remaining"), cond->count );
            break;
        case VSUBCOND_CTRL_ALL_HEXES:
            sprintf( str, tr("control all victory hexes") );
            break;
        case VSUBCOND_CTRL_HEX:
            sprintf( str, tr("control hex %i,%i"), cond->x, cond->y );
            break;
        case VSUBCOND_CTRL_HEX_NUM:
            sprintf( str, tr("control at least %i vic hexes"), cond->count );
            break;
        case VSUBCOND_UNITS_KILLED:
            sprintf( str, tr("kill units with tag '%s'"), cond->tag );
            break;
        case VSUBCOND_UNITS_SAVED:
            sprintf( str, tr("save units with tag '%s'"), cond->tag );
            break;
    }
}
void gui_show_conds()
{
    char str[128];
    int border = 10, i, j;
    int x = border, y = border;
    SDL_Surface *contents = gui->sinfo->contents;
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    SDL_FillRect( contents, 0, 0x0 );
    /* заглавие */
    sprintf( str, tr("Explicit VicConds (%s)"),
             (vcond_check_type==VCOND_CHECK_EVERY_TURN)?tr("every turn"):tr("last turn") );
    write_line( contents, gui->font_std, str, x, &y ); y += 10;
    for ( i = 1; i < vcond_count; i++ ) {
        sprintf( str, "'%s':", vconds[i].message );
        write_line( contents, gui->font_std, str, x, &y );
        for ( j = 0; j < vconds[i].sub_and_count; j++ ) {
            if ( vconds[i].subconds_and[j].player )
                sprintf( str, tr("AND %.2s "), vconds[i].subconds_and[j].player->name );
            else
                sprintf( str, tr("AND -- ") );
            gui_render_subcond( &vconds[i].subconds_and[j], str + 7 );
            write_line( contents, gui->font_std, str, x, &y );
        }
        for ( j = 0; j < vconds[i].sub_or_count; j++ ) {
            if ( vconds[i].subconds_or[j].player )
                sprintf( str, tr("OR %.2s "), vconds[i].subconds_or[j].player->name );
            else
                sprintf( str, tr("OR -- ") );
            gui_render_subcond( &vconds[i].subconds_or[j], str + 6 );
            write_line( contents, gui->font_std, str, x, &y );
        }
    }
    y += 6;
    /* иначе условие */
    sprintf( str, tr("else: '%s'"), vconds[0].message );
    write_line( contents, gui->font_std, str, x, &y );
    /* Показать */
    frame_apply( gui->sinfo );
    frame_hide( gui->sinfo, 0 );
}
/*
====================================================================
Показать окно подтверждения.
====================================================================
*/
void gui_show_confirm( const char *_text )
{
    Text *text;
    int border = 10, i;
    int x = border, y = border;
    SDL_Surface *contents = gui->confirm->frame->contents;
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    SDL_FillRect( contents, 0, 0x0 );
    text = create_text( gui->font_std, _text, contents->w - border*2 );
    for ( i = 0; i < text->count; i++ )
        write_line( contents, gui->font_std, text->lines[i], x, &y );
    delete_text( text );
    /* Показать */
    frame_apply( gui->confirm->frame );
    group_hide( gui->confirm, 0 );
}

/*
====================================================================
Показывать кнопки устройства на экране x, y (без проверки кнопок)
====================================================================
*/
void gui_show_unit_buttons( int x, int y )
{
    if ( y + gui->unit_buttons->frame->img->img->h >= sdl.screen->h )
        y = sdl.screen->h - gui->unit_buttons->frame->img->img->h;
    group_move( gui->unit_buttons, x, y );
    group_hide( gui->unit_buttons, 0 );
}

/** Показать диалог покупки юнита после его подготовки для текущего игрока. */
void gui_show_purchase_window()
{
	purchase_dlg_reset(gui->purchase_dlg);
	purchase_dlg_hide(gui->purchase_dlg,0);
}

/** Показать диалог покупки юнита после его подготовки для текущего игрока. */
void gui_show_modify_window()
{
	modify_dlg_reset(gui->purchase_dlg);
	purchase_dlg_hide(gui->purchase_dlg,0);
}

/** Показать диалог покупки юнита после его подготовки для текущего игрока. */
void gui_show_headquarters_window()
{
	headquarters_dlg_reset(gui->headquarters_dlg);
	headquarters_dlg_hide(gui->headquarters_dlg,0);
}

/*
====================================================================
Показать окно развертывания и выбрать первый модуль как «deploy_unit».
====================================================================
*/
void gui_show_deploy_window()
{
    Unit *unit;
    SDL_Surface *contents = gui->deploy_window->frame->contents;
    SDL_FillRect( contents, 0, 0x0 );
    deploy_offset = 0;
    deploy_unit = list_get( avail_units, 0 );
    list_reset( avail_units );
    list_clear( left_deploy_units );
    while ( ( unit = list_next( avail_units ) ) ) {
        unit->x = -1;
        list_add( left_deploy_units, unit );
    }
    /* добавить юниты */
    gui_add_deploy_units( contents );
    /* активировать кнопки */
    group_set_active( gui->deploy_window, ID_APPLY_DEPLOY, 1 );
    group_set_active( gui->deploy_window, ID_CANCEL_DEPLOY, !deploy_turn );
    /* Показать */
    frame_apply( gui->deploy_window->frame );
    group_hide( gui->deploy_window, 0 );
}

/*
====================================================================
Окно развертывания ручки.
  gui_handle_deploy_motion: 'unit' - это единица, в которой находится курсор
      в настоящее время выше
  gui_handle_deploy_click: 'new_unit' устанавливается в True, если новый юнит был
      selected (который является 'deploy_unit') иначе False
      вернуть True, если что-то случилось
====================================================================
*/
int gui_handle_deploy_click(int button, int cx, int cy)
{
    int i;
    if ( button == WHEEL_UP ) {
        gui_scroll_deploy_up();
    }
    else
    if ( button == WHEEL_DOWN ) {
        gui_scroll_deploy_down();
    }
    else
    for ( i = 0; i < deploy_show_count; i++ )
        if ( FOCUS( cx, cy,
                    gui->deploy_window->frame->img->bkgnd->surf_rect.x + deploy_border,
                    gui->deploy_window->frame->img->bkgnd->surf_rect.y + deploy_border + i * hex_h,
                    hex_w, hex_h ) ) {
            if ( i + deploy_offset < left_deploy_units->count ) {
                deploy_unit = list_get( left_deploy_units, i + deploy_offset );
                gui_add_deploy_units( gui->deploy_window->frame->contents );
                frame_apply( gui->deploy_window->frame );
                return 1;
            }
        }
    return 0;
}
void gui_handle_deploy_motion( int cx, int cy, Unit **unit )
{
    int i;
    *unit = 0;
    group_handle_motion( gui->deploy_window, cx, cy );
    for ( i = 0; i < deploy_show_count; i++ )
        if ( FOCUS( cx, cy,
                    gui->deploy_window->frame->img->bkgnd->surf_rect.x + deploy_border,
                    gui->deploy_window->frame->img->bkgnd->surf_rect.y + deploy_border + i * hex_h,
                    hex_w, hex_h ) ) {
            *unit = list_get( left_deploy_units, i + deploy_offset );
        }
}

/*
====================================================================
Прокрутите список развертывания вверх / вниз.
====================================================================
*/
void gui_scroll_deploy_up()
{
    deploy_offset -= 2;
    if ( deploy_offset < 0 ) deploy_offset = 0;
    gui_add_deploy_units( gui->deploy_window->frame->contents );
    frame_apply( gui->deploy_window->frame );
}
void gui_scroll_deploy_down()
{
    if ( deploy_show_count >= left_deploy_units->count )
        deploy_offset = 0;
    else {
        deploy_offset += 2;
        if ( deploy_offset + deploy_show_count >= left_deploy_units->count )
            deploy_offset = left_deploy_units->count - deploy_show_count;
    }
    gui_add_deploy_units( gui->deploy_window->frame->contents );
    frame_apply( gui->deploy_window->frame );
}

/*
====================================================================
Обновите список развертывания. Объект либо удален, либо добавлен к
left_deploy_units, и окно развертывания обновится.
====================================================================
*/
void gui_remove_deploy_unit( Unit *unit )
{
    List_Entry *entry;
    Unit *next_unit;
    entry = list_entry( left_deploy_units, unit );
    if ( entry->next->item )
        next_unit = entry->next->item;
    else
        if ( entry->prev->item )
            next_unit = entry->prev->item;
        else
            next_unit = 0;
    list_delete_item( left_deploy_units, unit );
    deploy_unit = next_unit;
    gui_add_deploy_units( gui->deploy_window->frame->contents );
    frame_apply( gui->deploy_window->frame );
}
void gui_add_deploy_unit( Unit *unit )
{
    if ( unit_has_flag( unit->sel_prop, "flying" ) )
        list_insert( left_deploy_units, unit, 0 );
    else
        list_add( left_deploy_units, unit );
    if ( deploy_unit == 0 ) deploy_unit = unit;
    gui_add_deploy_units( gui->deploy_window->frame->contents );
    frame_apply( gui->deploy_window->frame );
}

/*
====================================================================
Показать базовое меню на экране x, y (без проверки кнопок)
====================================================================
*/
void gui_show_menu( int x, int y )
{
    if ( y + gui->base_menu->frame->img->img->h >= sdl.screen->h )
        y = sdl.screen->h - gui->base_menu->frame->img->img->h;
    group_move( gui->base_menu, x, y );
    group_hide( gui->base_menu, 0 );
}

/*
====================================================================
Выведите имя файла на поверхность. (каталоги начинаются со
звездочки)
====================================================================
*/
void gui_render_file_name( void *item, SDL_Surface *buffer )
{
    const char *fname = (const char*)item;
    SDL_FillRect( buffer, 0, 0x0 );
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_CENTER;
    if ( fname[0] != '*' )
        write_text( gui->font_std, buffer, 4, buffer->h >> 1, fname, 255 );
    else {
        DEST( buffer, 2, ( buffer->h - gui->folder_icon->h ) >> 1, gui->folder_icon->w, gui->folder_icon->h );
        SOURCE( gui->folder_icon, 0, 0 );
        blit_surf();
        write_text( gui->font_std, buffer, 4 + gui->folder_icon->w, buffer->h >> 1, fname + 1, 255 );
    }
}

/*
====================================================================
Вывести название сценария / кампании на поверхность. (каталоги начинаются со
звездочки)
====================================================================
*/
void gui_render_scen_file_name( void *item, SDL_Surface *buffer )
{
    Name_Entry_Type *name_entry = (Name_Entry_Type*)item;
    SDL_FillRect( buffer, 0, 0x0 );
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_CENTER;
    if ( name_entry->internal_name[0] != '*' )
        write_text( gui->font_std, buffer, 4, buffer->h >> 1, name_entry->internal_name, 255 );
    else {
        DEST( buffer, 2, ( buffer->h - gui->folder_icon->h ) >> 1, gui->folder_icon->w, gui->folder_icon->h );
        SOURCE( gui->folder_icon, 0, 0 );
        blit_surf();
        write_text( gui->font_std, buffer, 4 + gui->folder_icon->w, buffer->h >> 1, name_entry->internal_name + 1, 255 );
    }
}

/*
====================================================================
Обработка выбора файла сценария (информация о рендере и
загрузить сценарий с помощью пути).
Переменная @camp_entry не используется.
====================================================================
*/
void gui_render_scen_info( const char *path, const char *camp_entry, SDL_Surface *buffer )
{
    Text *text;
    int i, x = 0, y = 0;
    char *info;
    if ( path == 0 ) {
        /* выбор не выполнен */
        group_set_active( gui->scen_dlg->group, ID_SCEN_SETUP, 0 );
        group_set_active( gui->scen_dlg->group, ID_SCEN_OK, 0 );
        SDL_FillRect( buffer, 0, 0x0 );
    }
    else
    if ( ( info = scen_load_info( path ) ) ) {
        group_set_active( gui->scen_dlg->group, ID_SCEN_SETUP, 1 );
        group_set_active( gui->scen_dlg->group, ID_SCEN_OK, 1 );
        /* рендеринг информации */
        SDL_FillRect( buffer, 0, 0x0 );
        gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
        text = create_text( gui->font_std, info, buffer->w );
        for ( i = 0; i < text->count; i++ )
            write_line( buffer, gui->font_std, text->lines[i], x, &y );
        delete_text( text );
    }
}

/*
====================================================================
Обработка выбора файла кампании (отображение информации и
загрузить camp_info с полного пути)
====================================================================
*/
void gui_render_camp_info( const char *path, const char *camp_entry, SDL_Surface *buffer )
{
    Text *text;
    int i, x = 0, y = 0;
    char *info;
    if ( path == 0 ) {
        /* выбор не выполнен */
        group_set_active( gui->camp_dlg->group, ID_CAMP_SETUP, 0 );
        group_set_active( gui->camp_dlg->group, ID_CAMP_OK, 0 );
        SDL_FillRect( buffer, 0, 0x0 );
    }
    else
    if ( ( info = camp_load_info( path, camp_entry ) ) ) {
        group_set_active( gui->camp_dlg->group, ID_CAMP_SETUP, 1 );
        group_set_active( gui->camp_dlg->group, ID_CAMP_OK, 1 );
        /* рендеринг информации */
        SDL_FillRect( buffer, 0, 0x0 );
        gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
        text = create_text( gui->font_std, info, buffer->w );
        for ( i = 0; i < text->count; i++ )
            write_line( buffer, gui->font_std, text->lines[i], x, &y );
        delete_text( text );
        free( info );
    }
}

/*
====================================================================
Обработайте выбор файла для загрузки.
@camp_entry не используется
====================================================================
*/
void gui_render_load_menu( const char *path, const char *camp_entry, SDL_Surface *buffer )
{
    if ( path == 0 )
    {
        /* выбор не выполнен */
        group_set_active( gui->load_menu->group, ID_LOAD_OK, 0 );
        SDL_FillRect( buffer, 0, 0x0 );
    }
    else
    {
        group_set_active( gui->load_menu->group, ID_LOAD_OK, 1 );
        /* рендеринг информации */
        SDL_FillRect( buffer, 0, 0x0 );
    }
}

/*
====================================================================
Обработайте выбор файла для сохранения.
@camp_entry не используется
====================================================================
*/
void gui_render_save_menu( const char *path, const char *camp_entry, SDL_Surface *buffer )
{
    if ( path == 0 )
    {
        /* выбор не выполнен */
        group_set_active( gui->save_menu->group, ID_SAVE_OK, 0 );
        SDL_FillRect( buffer, 0, 0x0 );
    }
    else
    {
        group_set_active( gui->save_menu->group, ID_SAVE_OK, 1 );
        /* рендеринг информации */
        SDL_FillRect( buffer, 0, 0x0 );
    }
}

/*
====================================================================
Обработайте выбор папки мода для загрузки.
@camp_entry не используется
====================================================================
*/
void gui_render_mod_select_info( const char *path, const char *camp_entry, SDL_Surface *buffer )
{
    char pathFinal[MAX_PATH];
    if ( path == 0 )
    {
        /* выбор не выполнен */
        group_set_active( gui->mod_select_dlg->group, ID_MOD_SELECT_OK, 0 );
        SDL_FillRect( buffer, 0, 0x0 );
    }
    else
    {
        group_set_active( gui->mod_select_dlg->group, ID_MOD_SELECT_OK, 1 );
        /* визуализировать изображение */
        if ( search_file_name( pathFinal, 0, "game", path, "", 'i' ) )
        {
            SDL_FillRect( buffer, 0, 0x0 );
            DEST( buffer, 0, 0, buffer->w, buffer->h );
            SOURCE( load_surf( pathFinal, SDL_SWSURFACE, 0, 0, 0, 0 ), 0, 0 );
            blit_surf();
        }
    }
}

/** опция панели сообщений для выбора */
typedef struct {
    /** прямоугольник проверки попадания для этой опции (абс. координаты экрана) */
    int x1, y1, x2, y2;
    /** идентификатор, который будет возвращен при выборе */
    char *id;
    /** текстовое описание */
    char *desc;
} MessagePane_Option;

/**
 * Представляет данные для панели сообщений.
 */
typedef struct MessagePane {
    /** общий текст, который будет отображаться в начале */
    char *text;
    /** идентификатор по умолчанию */
    char *default_id;
    /** содержит идентификатор, который был выбран, или идентификатор по умолчанию, если параметры не существуют */
    char *selected_id;
    /** количество вариантов */
    unsigned options_count;
    /** множество вариантов */
    MessagePane_Option *options;
    /** в настоящее время сфокусированный вариант */
    int focus_idx;
    /** сфокусированная опция при последнем нажатии кнопки мыши */
    int pressed_focus_idx;
    /** true, если в этой области была нажата кнопка */
    int button_pressed;
    /** перекрасить прямоугольник */
    int refresh_x1, refresh_y1, refresh_x2, refresh_y2;
} MessagePane;

/*
====================================================================
Создает новую структуру панели сообщений.
====================================================================
*/
MessagePane *gui_create_message_pane()
{
    MessagePane *pane = calloc(1, sizeof(MessagePane));
    pane->focus_idx = -1;
    pane->refresh_x2 = sdl.screen->w;
    pane->refresh_y2 = sdl.screen->h;
    return pane;
}

/*
====================================================================
Удаляет заданную структуру панели сообщений.
====================================================================
*/
void gui_delete_message_pane(MessagePane *pane)
{
    if (pane) {
        unsigned i;
        for (i = pane->options_count; i; ) {
            i--;
            free(pane->options[i].id);
            free(pane->options[i].desc);
        }
        free(pane->text);
        free(pane->default_id);
    }
    free(pane);
}

/*
====================================================================
Задает текст для панели сообщений.
====================================================================
*/
void gui_set_message_pane_text( struct MessagePane *pane, const char *text )
{
    pane->text = strdup(text);
}

/*
====================================================================
Задает идентификатор по умолчанию для панели сообщений.
====================================================================
*/
void gui_set_message_pane_default( struct MessagePane *pane, const char *default_id )
{
    pane->default_id = strdup(default_id);
}

/*
====================================================================
Возвращает текущий выбранный идентификатор или 0, если ничего не выбрано.
====================================================================
*/
const char *gui_get_message_pane_selection( struct MessagePane *pane )
{
    return pane->selected_id;
}

/*
====================================================================
Заполняет параметры для данной панели сообщений.
идентификаторы константный тип char * список идентификаторов
descs константный тип char * список текстовое описание будучи сопоставлены идентификаторы.
====================================================================
*/
void gui_set_message_pane_options( MessagePane *pane, List *ids, List *descs ) {
    unsigned i;
    pane->options_count = (unsigned)ids->count;
    pane->options = calloc(pane->options_count, sizeof(MessagePane_Option));
    list_reset(ids);
    list_reset(descs);
    for (i = 0; i < pane->options_count; i++) {
        MessagePane_Option *opt = &pane->options[i];
        opt->id = strdup(list_next(ids));
        opt->desc = strdup(list_next(descs));
    }
}

/*
====================================================================
Рисует и заполняет текстом панель сообщений.
====================================================================
*/
void gui_draw_message_pane( MessagePane *pane )
{
    static const char checkbox_indent_str[] = GS_CHECK_BOX_EMPTY " ";
    int i, j, y, x, checkbox_indent;
    Text *text;
    if (!(pane->refresh_x2 - pane->refresh_x1 > 0 && pane->refresh_y2 - pane->refresh_y1 > 0)) return;
    text = create_text( gui->font_brief, pane->text, gui->brief_frame->w - 40 );
    DEST( sdl.screen,/* принимающая поверхность (назначение) */
          ( sdl.screen->w - gui->brief_frame->w ) / 2,
          ( sdl.screen->h - gui->brief_frame->h ) / 2,
          gui->brief_frame->w, gui->brief_frame->h );
    SOURCE( gui->brief_frame, 0, 0 );
    blit_surf();
    gui->font_brief->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    x = ( sdl.screen->w - gui->brief_frame->w ) / 2 + 20;
    y = ( sdl.screen->h - gui->brief_frame->h ) / 2 + 40;
    for ( i = 0; i < text->count; i++ )
        write_line( sdl.screen, gui->font_brief, text->lines[i], x, &y );
    delete_text( text );
    checkbox_indent = text_width(gui->font_brief, checkbox_indent_str);
    /* параметры рисования и переопределение хит-прямоугольников */
    for (j = 0; j < pane->options_count; j++) {
        MessagePane_Option *opt = pane->options + j;
        int old_y = y;
        const int indent_x = x + checkbox_indent;
        char buf[5];
        snprintf(buf, sizeof buf, "%c", CharCheckBoxEmpty + (j == pane->focus_idx));
        write_line(sdl.screen, gui->font_brief, buf, x, &y );
        y = old_y;
        text = create_text(gui->font_brief, opt->desc, gui->brief_frame->w - 40 - checkbox_indent);
        for (i = 0; i < text->count; i++)
            write_line( sdl.screen, gui->font_brief, text->lines[i], indent_x, &y );
        delete_text( text );
        opt->x1 = x; opt->y1 = old_y;
        opt->x2 = x + gui->brief_frame->w - 40;
        opt->y2 = y;
    }
    refresh_screen( pane->refresh_x1, pane->refresh_y1,
    	pane->refresh_x2 - pane->refresh_x1, pane->refresh_y2 - pane->refresh_y1 );
    pane->refresh_x1 = pane->refresh_x2 = pane->refresh_y1 = pane->refresh_y2 = 0;
}

/*
====================================================================
Возвращает индекс сфокусированной опции в разделе (mx, my) или -1
====================================================================
*/
static int gui_map_message_pane_focus_index( MessagePane *pane, int mx, int my )
{
    int i;
    for (i = 0; i < pane->options_count; i++) {
        MessagePane_Option *opt = pane->options + i;
        if (mx >= opt->x1 && mx < opt->x2 && my >= opt->y1 && my < opt->y2)
            return i;
    }
    return -1;
}

/** объединить с существующим прямоугольником перекраски */
static void message_pane_unite_repaint_rect(MessagePane *pane, int x1, int y1, int x2, int y2)
{
    if ((pane->refresh_x2 - pane->refresh_x1) <= 0
        || (pane->refresh_y2 - pane->refresh_y1) <= 0) {
        pane->refresh_x1 = x1; pane->refresh_y1 = y1;
        pane->refresh_x2 = x2; pane->refresh_y2 = y2;
    } else {
        pane->refresh_x1 = MINIMUM(x1, pane->refresh_x1);
        pane->refresh_y1 = MINIMUM(y1, pane->refresh_y1);
        pane->refresh_x2 = MAXIMUM(x2, pane->refresh_x2);
        pane->refresh_y2 = MAXIMUM(y2, pane->refresh_y2);
    }

}

/*
====================================================================
Обрабатывает событие на панели сообщений.
====================================================================
*/
void gui_handle_message_pane_event( MessagePane *pane, int mx, int my, int button, int pressed )
{
    int new_focus_idx = gui_map_message_pane_focus_index( pane, mx, my );
    pane->selected_id = 0;

    /* определите новый прямоугольник перекраски, если фокус изменился */
    if (new_focus_idx != pane->focus_idx) {
        if (pane->focus_idx >= 0 && pane->focus_idx < pane->options_count)
            message_pane_unite_repaint_rect(pane,
                                            pane->options[pane->focus_idx].x1,
                                            pane->options[pane->focus_idx].y1,
                                            pane->options[pane->focus_idx].x2,
                                            pane->options[pane->focus_idx].y2);
        if (new_focus_idx >= 0 && new_focus_idx < pane->options_count)
            message_pane_unite_repaint_rect(pane,
                                            pane->options[new_focus_idx].x1,
                                            pane->options[new_focus_idx].y1,
                                            pane->options[new_focus_idx].x2,
                                            pane->options[new_focus_idx].y2);
    }

    switch (button) {
    case BUTTON_NONE:
        break;
    case BUTTON_LEFT:
        if (pressed) {
            pane->pressed_focus_idx = new_focus_idx;
            pane->button_pressed = 1;
        }
        else if (!pressed && pane->button_pressed) {
            if (new_focus_idx == pane->pressed_focus_idx
                && pane->pressed_focus_idx >= 0 && pane->pressed_focus_idx < pane->options_count) {
                pane->selected_id = pane->options[pane->pressed_focus_idx].id;
            } else if ((new_focus_idx < 0 || new_focus_idx >= pane->options_count)
                       && pane->options_count == 0) {
                pane->selected_id = pane->default_id;
            }
            pane->pressed_focus_idx = -1;
            pane->button_pressed = 0;
        }
        break;
    default:
        break;
    }

    pane->focus_idx = new_focus_idx;
}

/*
====================================================================
Откройте настройку сценария и установите первого игрока в качестве выбранного.
====================================================================
*/
void gui_open_scen_setup()
{
    int i;
    List *list;
    /* отрегулируйте настройки конфигурации, возможно, они изменились
       из-за нагрузки */
    group_lock_button( gui->scen_setup->confirm, ID_SCEN_SETUP_FOG, config.fog_of_war );
    group_lock_button( gui->scen_setup->confirm, ID_SCEN_SETUP_SUPPLY, config.supply );
    group_lock_button( gui->scen_setup->confirm, ID_SCEN_SETUP_WEATHER, config.weather );
    group_lock_button( gui->scen_setup->confirm, ID_SCEN_SETUP_DEPLOYTURN, config.deploy_turn );
    group_lock_button( gui->scen_setup->confirm, ID_SCEN_SETUP_PURCHASE, config.purchase);
    group_lock_button( gui->scen_setup->confirm, ID_SCEN_SETUP_MERGE_REPLACEMENTS, config.merge_replacements );
    /* сделайте список и выберите первую запись */
    list = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
    for ( i = 0; i < setup.player_count; i++ )
        list_add( list, strdup( setup.names[i] ) );
    lbox_set_items( gui->scen_setup->list, list );
    gui->scen_setup->list->cur_item = list_first( list );
    lbox_apply( gui->scen_setup->list );
    if ( gui->scen_setup->list->cur_item )
        gui_handle_player_select( gui->scen_setup->list->cur_item );
    sdlg_hide( gui->scen_setup, 0 );
}

/*
====================================================================
Откройте настройку кампании.
====================================================================
*/
void gui_open_camp_setup()
{
    /* отрегулируйте настройки конфигурации, возможно, они изменились
       из-за нагрузки */
    group_lock_button( gui->camp_setup->confirm, ID_CAMP_SETUP_MERGE_REPLACEMENTS, config.merge_replacements );
    group_lock_button( gui->camp_setup->confirm, ID_CAMP_SETUP_CORE, config.use_core_units );
    sdlg_hide( gui->camp_setup, 0 );
}

/*
====================================================================
Визуализация имени игрока в настройках сценария
====================================================================
*/
void gui_render_player_name( void *item, SDL_Surface *buffer )
{
    SDL_FillRect( buffer, 0, 0x0 );
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_CENTER;
    write_text( gui->font_std, buffer, 4, buffer->h >> 1, (char*)item, 255 );
}

/*
====================================================================
Обработайте выбор игрока в настройках.
====================================================================
*/
void gui_handle_player_select( void *item )
{
    int i;
    const char *name;
    char str[64];
    SDL_Surface *contents;
    /* выбор обновления */
    name = (const char*)item;
    for ( i = 0; i < setup.player_count; i++ )
        if ( STRCMP( name, setup.names[i] ) ) {
            gui->scen_setup->sel_id = i;
            gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_CENTER;
            contents = gui->scen_setup->ctrl->frame->contents;
            SDL_FillRect( contents, 0, 0x0 );
            if ( setup.ctrl[i] == PLAYER_CTRL_HUMAN )
                sprintf( str, tr("Control: Human") );
            else
                sprintf( str, tr("Control: CPU") );
            write_text( gui->font_std, contents, 10, contents->h >> 1, str, 255 );
            frame_apply( gui->scen_setup->ctrl->frame );
            contents = gui->scen_setup->module->frame->contents;
            SDL_FillRect( contents, 0, 0x0 );
            sprintf( str, tr("AI Module: %s"), setup.modules[i] );
            write_text( gui->font_std, contents, 10, contents->h >> 1, str, 255 );
            frame_apply( gui->scen_setup->module->frame );
            break;
        }
}


/*
====================================================================
Загрузите информацию о модуле.
@camp_entry не используется
====================================================================
*/
void gui_render_module_info( const char *path, const char *camp_entry, SDL_Surface *buffer )
{
    if ( path )
        group_set_active( gui->module_dlg->group, ID_MODULE_OK, 1 );
    else
        group_set_active( gui->module_dlg->group, ID_MODULE_OK, 0 );
}

/*
====================================================================
Зеркальное положение асимметричных окон.
====================================================================
*/
static int mirror_hori( int plane_x, int x, int w )
{
    if (x>=plane_x)
        return plane_x-(x-plane_x)-w;
    else
        return plane_x+(plane_x-(x+w));
}
void gui_mirror_asymm_windows()
{
    int plane_x=sdl.screen->w/2;
    int x,y,w,h;
    gui->mirror_asymm = !gui->mirror_asymm;
    /* краткая информация */
    frame_get_geometry(gui->qinfo1,&x,&y,&w,&h);
    frame_move(gui->qinfo1,mirror_hori(plane_x,x,w),y);
    frame_get_geometry(gui->qinfo2,&x,&y,&w,&h);
    frame_move(gui->qinfo2,mirror_hori(plane_x,x,w),y);
    /* окно развертывания */
    group_get_geometry(gui->deploy_window,&x,&y,&w,&h);
    group_move(gui->deploy_window,mirror_hori(plane_x,x,w),y);
}

/** Показать выбор видеорежима */
void gui_vmode_dlg_show()
{
	int i;

	/* заполнение списка при первом запуске */
	if (lbox_is_empty(gui->vmode_dlg->select_lbox)) {
		List *items = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
		char str[64];

		for (i = 0; i < sdl.num_vmodes; i++) {
			VideoModeInfo *vmi = &sdl.vmodes[i];
			snprintf(str,64,"%dx%dx%d %s",
					vmi->width, vmi->height, vmi->depth,
					vmi->fullscreen?
						tr("Fullscreen"):tr("Window"));
			list_add(items, strdup(str));
		}
		select_dlg_set_items( gui->vmode_dlg, items);
	}

	/* выберите текущий режим видео */
	for (i = 0; i < sdl.num_vmodes; i++)
		if (sdl.screen->w == sdl.vmodes[i].width &&
				sdl.screen->h == sdl.vmodes[i].height &&
				(!!(sdl.screen->flags & SDL_FULLSCREEN)) ==
				sdl.vmodes[i].fullscreen) {
			lbox_select_item(gui->vmode_dlg->select_lbox,
					list_get(gui->vmode_dlg->select_lbox->items,i));
			break;
		}

	select_dlg_hide( gui->vmode_dlg, 0 );
}
