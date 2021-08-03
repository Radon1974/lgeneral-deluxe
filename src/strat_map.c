/***************************************************************************
                          strat_map.c  -  description
                             -------------------
    begin                : Fri Apr 5 2002
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
 
#include <dirent.h>
#include "lgeneral.h"
#include "windows.h"
#include "unit.h"
#include "purchase_dlg.h"
#include "gui.h"
#include "nation.h"
#include "player.h"
#include "date.h"
#include "map.h"
#include "scenario.h"
#include "engine.h"
#include "strat_map.h"

/*
====================================================================
Внешние
====================================================================
*/
extern Sdl sdl;
extern Nation *nations;
extern int nation_count;
extern int nation_flag_width, nation_flag_height;
extern int map_w, map_h;
extern int hex_w, hex_h;
extern int hex_x_offset, hex_y_offset, terrain_columns, terrain_rows;
extern int terrain_type_count;
extern Terrain_Type *terrain_types;
extern Mask_Tile **mask;
extern Map_Tile **map;
extern int air_mode;
extern Player *cur_player;
extern int cur_weather;
extern Weather_Type *weather_types;
extern Terrain_Images *terrain_images;

/*
====================================================================
Данные стратегической карты.
====================================================================
*/
static int screen_x, screen_y; /* положение на экране */
static int width, height; /* размер изображения карты */
static int sm_x_offset, sm_y_offset; /* смещение для первого тайла карты страт 0,0 в strat_map */
static int strat_tile_width;
static int strat_tile_height; /* размер уменьшенного фрагмента карты */
static int strat_tile_x_offset;
static int strat_tile_y_offset; /* смещения, которые будут добавлены к позиции для следующей плитки */
static SDL_Surface *strat_map = 0; /* картина собрана create_strat_map() */
static SDL_Surface *fog_layer = 0; /* используется для буферизации тумана перед добавлением к страт-карте */
static SDL_Surface *unit_layer = 0; /* слой с флагами юнитов, указывающими его положение */
static SDL_Surface *flag_layer = 0; /* слой со всеми флагами; белая рамка означает нормальный; золотая рамка означает цель */
static int tile_count; /* equals map::def::tile_count */
static SDL_Surface **strat_tile_pic = 0; /* уменьшенные фрагменты карты нормалей; создан в strat_map_create() */
static SDL_Surface *strat_flags = 0; /* изображение флага с измененным размером, содержащее флаги нации */
static int strat_flag_width, strat_flag_height; /* размер страт-флага с измененным размером */
static SDL_Surface *blink_dot = 0; /* мигающая белая и черная точка,
                                      блок еще не был перемещен */
static List *dots = 0; /* список точек, которые нужно мигать */
static int blink_on = 0; /* переключатель используется для мигания */

/*
====================================================================
ЛОКАЛЬНЫЕ
====================================================================
*/

/*
====================================================================
Получите размер стратегической карты в зависимости от масштаба, где
масштаб == 1: нормальный размер
масштаб == 2: размер / 2
масштаб == 3: размер / 3
...
====================================================================
*/
static int scaled_strat_map_width( int scale )
{
    return ( ( map_w - 1 ) * hex_x_offset / scale );
}
static int scaled_strat_map_height( int scale )
{
    return ( ( map_h - 1 ) * hex_h / scale );
}

/*
====================================================================
Обновляет смещение изображения для стратегической карты во всех фрагментах карты.
====================================================================
*/
static void update_strat_image_offset()
{
    int x, y;
    for ( x = 0; x < map_w; x++ )
        for ( y = 0;  y < map_h; y++ )
            map_tile( x, y )->strat_image_offset = strat_tile_width
                                                 * map_tile( x, y )->image_offset_x / hex_w +
                                                 strat_tile_width * terrain_columns
                                                 * map_tile( x, y )->image_offset_y / hex_h;
}

/*
====================================================================
ПУБЛИЧНЫЕ
====================================================================
*/

/*
====================================================================
Вызывается после загрузки сценария в init_engine () и создает
изображения тайлов стратегической карты, флаги и сама strat_map +
unit_layer
====================================================================
*/
void strat_map_create()
{
    Uint32 ckey;
    int i, j, j2, x, y;
    int strat_tile_count;
    Uint32 pixel;
    int hori_scale, vert_scale;
    int scale;
    /* масштабировать нормальную геометрию так, чтобы она соответствовала экрану * /
    / * пробуем горизонтально */
    hori_scale = 1;
    while( scaled_strat_map_width( hori_scale ) > sdl.screen->w ) hori_scale++;
    vert_scale = 1;
    while( scaled_strat_map_height( vert_scale ) > sdl.screen->h )
        vert_scale++;
    /* использовать больший масштаб */
    if ( hori_scale > vert_scale )
        scale = hori_scale;
    else
        scale = vert_scale;
    /* масштаб должен быть не менее 2 */
    if ( scale < 2 )
        scale = 2;
    /* получить размер тайла страт-карты */
    strat_tile_width = hex_w / scale;
    strat_tile_height = hex_h / scale;
    strat_tile_x_offset = hex_x_offset / scale;
    strat_tile_y_offset = hex_y_offset / scale;
    /* создать массив страт тайлов */
    tile_count = terrain_type_count / 4;
    strat_tile_pic = calloc( tile_count, sizeof( SDL_Surface* ) );
    /* создавать страт-тайлы */
    for ( i = 0; i < tile_count; i++ ) {
        /* сколько плитки уложено? */
        strat_tile_count = terrain_columns * terrain_rows;
        /* создать strat_pic */
        strat_tile_pic[i] = create_surf( strat_tile_count * strat_tile_width,
                                         strat_tile_height,
                                         SDL_SWSURFACE );
        /* прозрачный цвет */
        ckey = get_pixel( terrain_images->images[0], 0, 0 );
        FULL_DEST( strat_tile_pic[i] );
        fill_surf( ckey );
        SDL_SetColorKey( strat_tile_pic[i], SDL_SRCCOLORKEY, ckey );
        /* копировать пиксели из pic в strat_pic, если strat_fog не прозрачный */
        for ( j = 0; j < terrain_columns; j++ )
            for ( j2 = 0; j2 < terrain_rows; j2++ )
                for ( x = 0; x < strat_tile_width; x++ )
                    for ( y = 0; y < strat_tile_height; y++ ) {
                        /* у нас есть пиксель в strat_pic от x + j * strat_fog_pic->w,y */
                        /* нет, нам нужен эквивалентный пиксель в tiles[i]->pic to copy it */
                        pixel = get_pixel( terrain_images->images[i],
                                           j * hex_w +
                                           scale * x,
                                           j2 * hex_h + 
                                           scale * y);
                        set_pixel( strat_tile_pic[i], j * strat_tile_width + strat_tile_width * terrain_columns * j2 + x,
                                   y, pixel );
                    }
    }
    /* обновить смещение страт-изображения во всех фрагментах карты */
    update_strat_image_offset();
    /* измененные размеры национальных флагов */
    strat_flag_width = strat_tile_width - 2;
    strat_flag_height = strat_tile_height - 2;
    strat_flags = create_surf( strat_flag_width * nation_count, strat_flag_height, SDL_SWSURFACE );
    /* уменьшите масштаб */
    for ( i = 0; i < nation_count; i++ )
        for ( x = 0; x < strat_flag_width; x++ )
            for ( y = 0; y < strat_flag_height; y++ ) {
                pixel = nation_get_flag_pixel( &nations[i], 
                                               nation_flag_width * x / strat_flag_width,
                                               nation_flag_height * y / strat_flag_height );
                set_pixel( strat_flags, i * strat_flag_width + x, y, pixel );
            }
    /* измерения */
    width = ( map_w - 1 ) * strat_tile_x_offset;
    sm_x_offset = -strat_tile_width + strat_tile_x_offset;
    height = ( map_h - 1 ) * strat_tile_height;
    sm_y_offset = -strat_tile_height / 2;
    screen_x = ( sdl.screen->w - width ) / 2;
    screen_y = ( sdl.screen->h - height ) / 2;
    /* построить белый прямоугольник */
    blink_dot = create_surf( 2, 2, SDL_SWSURFACE );
    set_pixel( blink_dot, 0, 0, SDL_MapRGB( blink_dot->format, 255, 255, 255 ) );
    set_pixel( blink_dot, 1, 0, SDL_MapRGB( blink_dot->format, 16, 16, 16 ) );
    set_pixel( blink_dot, 1, 1, SDL_MapRGB( blink_dot->format, 255, 255, 255 ) );
    set_pixel( blink_dot, 0, 1, SDL_MapRGB( blink_dot->format, 16, 16, 16 ) );
    /* список должностей */
    dots = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
}

/*
====================================================================
Очистить материал, выделенный strat_map_create()
====================================================================
*/
void strat_map_delete()
{
    int i;
    if ( strat_tile_pic ) {
        for ( i = 0; i < tile_count; i++ )
            free_surf( &strat_tile_pic[i] );
        free( strat_tile_pic );
        strat_tile_pic = 0;
    }
    free_surf( &strat_flags );
    free_surf( &strat_map );
    free_surf( &fog_layer );
    free_surf( &unit_layer );
    free_surf( &flag_layer );
    free_surf( &blink_dot );
    if ( dots ) {
        list_delete( dots );
        dots = 0;
    }
}

/*
====================================================================
Обновите растровое изображение, содержащее стратегическую карту.
====================================================================
*/
void strat_map_update_terrain_layer()
{
    int i, j;
    /* перераспределить */
    free_surf( &strat_map );
    free_surf( &fog_layer );
    strat_map = create_surf( width, height, SDL_SWSURFACE );
    SDL_FillRect( strat_map, 0, 0x0 );
    fog_layer = create_surf( width, height, SDL_SWSURFACE );
    SDL_FillRect( fog_layer, 0, 0x0 );
    /* сначала соберите все затуманенные плитки без альфы */
    for ( j = 0; j < map_h; j++ )
        for ( i = 0; i < map_w; i++ )
            if ( mask[i][j].fog ) {
                DEST( fog_layer,
                      sm_x_offset + i * strat_tile_x_offset,
                      sm_y_offset + j * strat_tile_height + ( i & 1 ) * strat_tile_y_offset,
                      strat_tile_width, strat_tile_height );
                SOURCE( strat_tile_pic
                        [ground_conditions_get_index( weather_types[cur_weather].ground_conditions )],
                        map_tile( i, j )->strat_image_offset, 0 );
                blit_surf();
            }
    /* теперь добавьте эту карту тумана с прозрачностью к strat_map */
    FULL_DEST( strat_map ); FULL_SOURCE( fog_layer );
    alpha_blit_surf( 255 - FOG_ALPHA );
    /* теперь добавьте незакрытые фрагменты карты */
    for ( j = 0; j < map_h; j++ )
        for ( i = 0; i < map_w; i++ )
            if ( !mask[i][j].fog ) {
                DEST( strat_map,
                      sm_x_offset + i * strat_tile_x_offset,
                      sm_y_offset + j * strat_tile_height + ( i & 1 ) * strat_tile_y_offset,
                      strat_tile_width, strat_tile_height );
                SOURCE( strat_tile_pic[ground_conditions_get_index( weather_types[cur_weather].ground_conditions )],
                        map_tile( i, j )->strat_image_offset, 0 );
                blit_surf();
            }
}
typedef struct {
    int x, y;
    Nation *nation;
} DotPos;
void strat_map_update_unit_layer()
{
    int i, j;
    Unit *unit;
    DotPos *dotpos = 0;
    list_clear( dots );
    free_surf( &unit_layer );
    free_surf( &flag_layer );
    unit_layer = create_surf( width, height, SDL_SWSURFACE );
    SDL_FillRect( unit_layer, 0, 0x0 );
    flag_layer = create_surf( width, height, SDL_SWSURFACE );
    SDL_FillRect( flag_layer, 0, 0x0 );
    /* рисовать плитку; затуманенные плитки копируются прозрачными с инверсией FOG_ALPHA */
    for ( j = 0; j < map_h; j++ )
        for ( i = 0; i < map_w; i++ ) {
            if ( map_tile( i, j )->nation != 0 ) {
                /* рисовать рамку и отмечать только если цель */
                if ( map_tile( i, j )->obj ) {
                    DEST( flag_layer,
                          sm_x_offset + i * strat_tile_x_offset,
                          sm_y_offset + j * strat_tile_height + ( i & 1 ) * strat_tile_y_offset,
                          strat_tile_width, strat_tile_height );
                    fill_surf( 0xffff00 );
                    /* добавить флаг */
                    DEST( flag_layer,
                          sm_x_offset + i * strat_tile_x_offset + 1,
                          sm_y_offset + j * strat_tile_height + ( i & 1 ) * strat_tile_y_offset + 1,
                          strat_flag_width, strat_flag_height );
                    SOURCE( strat_flags, nation_get_index( map_tile( i, j )->nation ) * strat_flag_width, 0  );
                    blit_surf();
                }
            }
            unit = 0;
            if ( air_mode && map[i][j].a_unit )
                unit = map_tile( i, j )->a_unit;
            if ( !air_mode && map[i][j].g_unit )
                unit = map_tile( i, j )->g_unit;
            if ( map_mask_tile( i, j )->fog ) unit = 0;
            if ( unit ) {
                /* флаг нации имеет небольшую рамку в один пиксель */
                DEST( unit_layer,
                      sm_x_offset + i * strat_tile_x_offset + 1,
                      sm_y_offset + j * strat_tile_height + ( i & 1 ) * strat_tile_y_offset + 1,
                      strat_flag_width, strat_flag_height );
                SOURCE( strat_flags, nation_get_index( unit->nation ) * strat_flag_width, 0 );
                blit_surf();
                if ( cur_player )
                if ( cur_player == unit->player )
                if ( unit->cur_mov > 0 ) {
                    /* устройству нужно мигающее пятно */
                    dotpos = calloc( 1, sizeof( DotPos ) );
                    dotpos->x = sm_x_offset + i * strat_tile_x_offset + 1/* +
                                ( strat_flag_width - blink_dot->w ) / 2*/;
                    dotpos->y = sm_y_offset + j * strat_tile_height + ( i & 1 ) * strat_tile_y_offset + 1/* +
                                ( strat_flag_height - blink_dot->h ) / 2*/;
                    dotpos->nation = unit->nation;
                    list_add( dots, dotpos );
                }
            }
        }
}
void strat_map_draw()
{
    SDL_FillRect( sdl.screen, 0, 0x0 );
    DEST( sdl.screen, screen_x, screen_y, width, height );
    SOURCE( strat_map, 0, 0 );
    blit_surf();
    DEST( sdl.screen, screen_x, screen_y, width, height );
    SOURCE( flag_layer, 0, 0 );
    blit_surf();
    DEST( sdl.screen, screen_x, screen_y, width, height );
    SOURCE( unit_layer, 0, 0 );
    blit_surf();
}

/*
====================================================================
Обработка ввода движения. Если указатель мыши находится на тайле карты x, y и true
возвращается.
====================================================================
*/
int strat_map_get_pos( int cx, int cy, int *map_x, int *map_y )
{
    int x = 0, y = 0;
    int tile_x_offset, tile_y_offset;
    int tile_x, tile_y;
    Uint32 ckey;
    /* проверьте, если за пределами карты */
    if ( cx < screen_x || cy < screen_y )
        return 0;
    if ( cx >= screen_x + width || cy >= screen_y + height )
        return 0;
    /* закрепить позицию мыши на маске */
    cx -= screen_x;
    cy -= screen_y;
    /* получить смещение карты на экране из положения мыши */
    x = ( cx - sm_x_offset ) / strat_tile_x_offset;
    if ( x & 1 )
        y = ( cy - ( strat_tile_height >> 1 ) - sm_y_offset ) / strat_tile_height;
    else
        y = ( cy - sm_y_offset ) / strat_tile_height;
    /* получить начальную позицию этого тайла карты страт в x, y на поверхности strat_map */
    tile_x_offset = sm_x_offset;
    tile_y_offset = sm_y_offset;
    tile_x_offset += x * strat_tile_x_offset;
    tile_y_offset += y * strat_tile_height;
    if ( x & 1 )
        tile_y_offset += strat_tile_height >> 1;
    /* теперь проверьте, где именно указатель мыши находится на этой плитке, используя
       strat_tile_pic [0] */
    tile_x = cx - tile_x_offset;
    tile_y = cy - tile_y_offset;
    ckey = get_pixel( strat_tile_pic[0], 0, 0 );
    if ( get_pixel( strat_tile_pic[0], tile_x, tile_y ) == ckey ) {
        if ( tile_y < strat_tile_y_offset && !( x & 1 ) ) y--;
        if ( tile_y >= strat_tile_y_offset && (x & 1 ) ) y++;
        x--;
    }
    /* назначать */
    *map_x = x;
    *map_y = y;
    return 1;
}

/*
====================================================================
Мигайте точками, которые показывают неподвижные юниты.
====================================================================
*/
void strat_map_blink()
{
    DotPos *pos;
    blink_on = !blink_on;
    list_reset( dots );
    while ( ( pos = list_next( dots ) ) ) {
        if ( blink_on ) {
            DEST( sdl.screen, screen_x + pos->x, screen_y + pos->y, strat_flag_width, strat_flag_height );
            SOURCE( strat_flags, nation_get_index( pos->nation ) * strat_flag_width, 0 );
            blit_surf();
        }
        else {
            DEST( sdl.screen, screen_x + pos->x, screen_y + pos->y, strat_flag_width, strat_flag_height );
            SOURCE( strat_map, pos->x, pos->y );
            blit_surf();
            SOURCE( unit_layer, pos->x, pos->y );
            alpha_blit_surf( 96 );
        }
/*        if ( blink_on ) {
            DEST( sdl.screen, screen_x + pos->x, screen_y + pos->y, blink_dot->w, blink_dot->h );
            SOURCE( blink_dot, 0, 0 );
            blit_surf();
        }
        else {
            DEST( sdl.screen, screen_x + pos->x, screen_y + pos->y, blink_dot->w, blink_dot->h );
            SOURCE( unit_layer, pos->x, pos->y );
            blit_surf();
        }*/
    }
    add_refresh_region( screen_x, screen_y, width, height );
}
