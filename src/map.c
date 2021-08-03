/***************************************************************************
                          map.c  -  description
                             -------------------
    begin                : Mon Jan 22 2001
    copyright            : (C) 2001 by Michael Speck
    email                : kulkanie@gmx.net
 ***************************************************************************/
/***************************************************************************
                          Modifications
    Patch by Galland 2012 http://sourceforge.net/tracker/?group_id=23757&atid=379520
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
#include "parser.h"
#include "file.h"
#include "nation.h"
#include "unit.h"
#include "map.h"
#include "misc.h"
#include "localize.h"
#include "campaign.h"
#include "FPGE/pgf.h"

#include <assert.h>
#include <limits.h>

/*
====================================================================
Внешние
====================================================================
*/
extern Sdl sdl;
extern Terrain_Type *terrain_types;
extern int terrain_type_count;
extern int hex_w, hex_h, hex_x_offset, terrain_columns;
extern List *vis_units;
extern int cur_weather;
extern int nation_flag_width, nation_flag_height;
extern Config config;
extern Terrain_Icons *terrain_icons;
extern Unit_Info_Icons *unit_info_icons;
extern Player *cur_player;
extern List *units;
extern int modify_fog;
extern int camp_loaded;
extern Weather_Type *weather_types;

/*
====================================================================
карта
====================================================================
*/
int map_w = 0, map_h = 0;
Map_Tile **map = 0;
Mask_Tile **mask = 0;

/*
====================================================================
Локальные
====================================================================
*/

int DirectionToNumber[6] = { 0, 1, 3, 4, 5, 7 };

enum { DIST_AIR_MAX = SHRT_MAX };

typedef struct {
    short x, y;
} MapCoord;

static List *deploy_fields;

/*
====================================================================
Проверьте окружающие плитки и возьмите ту, у которой самый высокий
значение in_range.
====================================================================
*/
static void map_get_next_unit_point( Unit *unit, int x, int y, int *next_x, int *next_y )
{
    int high_x, high_y;
    int i;
    high_x = x; high_y = y;
    for ( i = 0; i < 6; i++ ) {
        if ( get_close_hex_pos( x, y, i, next_x, next_y ) ) {
            if ( mask[*next_x][*next_y].in_range > mask[high_x][high_y].in_range ) {
                /* проверить зоны контроля */
                /* ОШИБКА: Временно закоментированы строки 100, 101, 104 - выдают ошибку сегментации */
                //if ( !( config.zones_of_control && ( ( unit_has_flag( unit->sel_prop, "flying" ) && mask[*next_x][*next_y].vis_air_infl > 0 ) ||
                //                                       ( !unit_has_flag( unit->sel_prop, "flying" ) &&  mask[*next_x][*next_y].vis_infl > 0 ) ) ) ) {
                    high_x = *next_x;
                    high_y = *next_y;
                //}
            }
        }
    }
    *next_x = high_x;
    *next_y = high_y;
}

/*
====================================================================
Добавьте влияние юнита к маске (vis_) infl.
====================================================================
*/
static void map_add_vis_unit_infl( Unit *unit )
{
    int i, next_x, next_y;
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        mask[unit->x][unit->y].vis_air_infl++;
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
                mask[next_x][next_y].vis_air_infl++;
    }
    else {
        mask[unit->x][unit->y].vis_infl++;
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
                mask[next_x][next_y].vis_infl++;
    }
}

/*
====================================================================
Загрузить карту в старом формате lgmap.
====================================================================
*/
int map_load_lgmap( char *fname, char *path )
{
    int i, x, y, j, limit;
    PData *pd;
    char *str, *tile;
    char *domain = 0;
    List *tiles, *names;
    if ( ( pd = parser_read_file( fname, path ) ) == 0 ) goto parser_failure;
    domain = determine_domain(pd, fname);
    locale_load_domain(domain, 0/*FIXME*/);
    /* размер карты */
    if ( !parser_get_int( pd, "width", &map_w ) ) goto parser_failure;
    if ( !parser_get_int( pd, "height", &map_h ) ) goto parser_failure;
    /* загружать местности */
    if ( !parser_get_value( pd, "terrain_db", &str, 0 ) ) goto parser_failure;
    if ( !terrain_load( str ) ) goto failure;
    if ( !parser_get_values( pd, "tiles", &tiles ) ) goto parser_failure;
    /* выделить память карты */
    map = calloc( map_w, sizeof( Map_Tile* ) );
    for ( i = 0; i < map_w; i++ )
        map[i] = calloc( map_h, sizeof( Map_Tile ) );
    mask = calloc( map_w, sizeof( Mask_Tile* ) );
    for ( i = 0; i < map_w; i++ )
        mask[i] = calloc( map_h, sizeof( Mask_Tile ) );
    /* сама карта */
    list_reset( tiles );
    for ( y = 0; y < map_h; y++ )
        for ( x = 0; x < map_w; x++ ) {
            tile = list_next( tiles );
            /* по умолчанию нет флага */
            map[x][y].nation = 0;
            map[x][y].player = 0;
            map[x][y].deploy_center = 0;
            /* по умолчанию нет цели в мил */
            map[x][y].obj = 0;
            /* проверить тип плитки; если плитка не найдена, используется первая */
            for ( j = 0; j < terrain_type_count; j++ ) {
                if ( terrain_types[j].id == tile[0] ) {
                    map[x][y].terrain_id[0] = terrain_types[j].id;
                }
            }
            /* проверить id изображения - установить смещение */
            limit = terrain_type_find( map[x][y].terrain_id[0] )->images[0]->w / hex_w - 1;
            if ( tile[1] == '?' )
            {
                /* установить смещение случайным образом */
                map[x][y].image_offset_x = RANDOM( 0, limit ) * hex_w;
                map[x][y].image_offset_y = 0;
            }
            else
            {
                map[x][y].image_offset_x = atoi( tile + 1 ) % terrain_columns * hex_w;
                map[x][y].image_offset_y = atoi( tile + 1 ) / terrain_columns * hex_h;
            }
            /* Имя набора */
            map[x][y].name = strdup( terrain_type_find( map[x][y].terrain_id[0] )->name );
        }
    /* названия карт */
    if ( parser_get_values( pd, "names", &names ) ) {
        list_reset( names );
        for ( i = 0; i < names->count; i++ ) {
            str = list_next( names );
            x = i % map_w;
            y = i / map_w;
            free( map[x][y].name );
            map[x][y].name = strdup( trd(domain, str) );
        }
    }
    parser_free( &pd );
    free(domain);
    return 1;
parser_failure:
    fprintf( stderr, "%s\n", parser_get_error() );
failure:
    map_delete();
    if ( pd ) parser_free( &pd );
    free(domain);
    return 0;
}

/*
====================================================================
Загрузить карту в новом формате lgdmap.
====================================================================
*/
int map_load_lgdmap( char *fname, char *path )
{
    int i;
    char *domain = 0;

    FILE *inf;
    char line[1024], tokens[100][256];
    int j, block=0, last_line_length=-1, cursor=0, token=0;
    int utf16 = 0, lines=0, cur_map_h = 0, x, limit;
    map_w = 0;
    map_h = 0;

    inf=fopen(path,"rb");
    if (!inf)
    {
        fprintf( stderr, "Couldn't open '%s' map file\n", fname);
        return 0;
    }

    //найти ширину и высоту карты
    while (load_line(inf,line,utf16)>=0)
    {
        lines++;
        //удалить комментарии
        if (line[0]==0x23 || line[0]==0x09) { line[0]=0; }
        if (strlen(line)>0 && last_line_length==0)
        {
            block++;
        }
        last_line_length=strlen(line);
        token=0;
        cursor=0;
        for(i=0;i<strlen(line);i++)
            if (line[i]==0x09)
            {
                tokens[token][cursor]=0;
                token++;
                cursor=0;
            }
            else
            {
                tokens[token][cursor]=line[i];
                cursor++;
            }
        tokens[token][cursor]=0;
        token++;
        if (block == 2 && strlen(line)>0)
        {
            if ( map_w == 0)
                map_w = token;
            map_h++;
        }
        if (block == 3)
        {
            fclose(inf);
            break;
        }
    }

    /* выделить память карты */
    map = calloc( map_w, sizeof( Map_Tile* ) );
    for ( i = 0; i < map_w; i++ )
        map[i] = calloc( map_h, sizeof( Map_Tile ) );
    mask = calloc( map_w, sizeof( Mask_Tile* ) );
    for ( i = 0; i < map_w; i++ )
        mask[i] = calloc( map_h, sizeof( Mask_Tile ) );

    block=0;last_line_length=-1;cursor=0;token=0;lines=0;
    inf=fopen(path,"rb");
    while (load_line(inf,line,utf16)>=0)
    {
        //подсчитать строки, чтобы ошибка могла отображаться с номером строки
        lines++;

        //удалить комментарии
        if (line[0]==0x23 || line[0]==0x09) { line[0]=0; }
        if (strlen(line)>0 && last_line_length==0)
        {
            block++;
        }
        last_line_length=strlen(line);
        token=0;
        cursor=0;
        for(i=0;i<strlen(line);i++)
            if (line[i]==0x09)
            {
                tokens[token][cursor]=0;
                token++;
                cursor=0;
            }
            else
            {
                tokens[token][cursor]=line[i];
                cursor++;
            }
        tokens[token][cursor]=0;
        token++;

        //Блок # 1 файл ландшафта
        if (block == 1 && strlen(line)>0)
        {
            /* load terrains */
            if ( !terrain_load( tokens[1] ) ) goto failure;
        }

        //Блок # 2 слой карты 0 - местность
        if (block == 2 && strlen(line)>0)
        {
            /* сама карта */
            for ( x = 0; x < token; x++ )
            {
                /* по умолчанию нет флага */
                map[x][cur_map_h].nation = 0;
                map[x][cur_map_h].player = 0;
                map[x][cur_map_h].deploy_center = 0;
                /* по умолчанию нет цели в мил */
                map[x][cur_map_h].obj = 0;
                /* проверить тип плитки; если плитка не найдена, используется первая */
                map[x][cur_map_h].terrain_id[0] = 0;
                for ( j = 0; j < terrain_type_count; j++ ) {
                    if ( terrain_types[j].id == tokens[x][0] ) {
                        map[x][cur_map_h].terrain_id[0] = terrain_types[j].id;
                    }
                }
                /* проверить id изображения - установить смещение */
                limit = terrain_type_find( map[x][cur_map_h].terrain_id[0] )->images[0]->w / hex_w - 1;
                if ( tokens[x][1] == '?' )
                {
                    /* установить смещение случайным образом */
                    map[x][cur_map_h].image_offset_x = RANDOM( 0, limit ) * hex_w;
                    map[x][cur_map_h].image_offset_y = 0;
                }
                else
                {
                    map[x][cur_map_h].image_offset_x = atoi( tokens[x] + 1 ) % terrain_columns * hex_w;
                    map[x][cur_map_h].image_offset_y = atoi( tokens[x] + 1 ) / terrain_columns * hex_h;
                }
                /* Имя набора */
                map[x][cur_map_h].name = strdup( terrain_type_find( map[x][cur_map_h].terrain_id[0] )->name );
            }
            cur_map_h++;
            if ( cur_map_h == map_h )
                cur_map_h = 0;
        }

        //Имена тайлов карты блока # 3
        if (block == 3 && strlen(line)>0)
        {
            /* названия карт */
            for ( x = 0; x < token; x++ )
            {
                if ( tokens[x][0] != '0' )
                {
                    free( map[x][cur_map_h].name );
                    map[x][cur_map_h].name = strdup( tokens[x] );
                }
            }
            cur_map_h++;
            if ( cur_map_h == map_h )
                cur_map_h = -1;
        }

        //Блок № 4, 5 и т. Д. Последующие слои карты
        if (block >= 4 && strlen(line)>0)
        {
            if ( cur_map_h != -1 )
            {
                /* новые соединения слоя карты */
                for ( x = 0; x < token; x++ )
                {
                    map[x][cur_map_h].layers[block - 3] = atoi( tokens[x] );
                }
            }
            else
            {
                /* новый слой карты terrain_id */
                int y;
                for (y = 0; y < map_h; ++y)
                    for (x = 0; x < map_w; ++x)
                        map[x][y].terrain_id[block - 3] = tokens[1][0];
            }
            cur_map_h++;
            if ( cur_map_h == map_h )
                cur_map_h = -1;
        }
    }
    fclose(inf);
    free(domain);
    return 1;
failure:
    map_delete();
    free(domain);
    return 0;
}

/*
====================================================================
Публичные
====================================================================
*/

/*
====================================================================
Загрузить карту.
====================================================================
*/
int map_load( char *fname )
{
    char *path, *extension;
    path = calloc( 256, sizeof( char ) );
    extension = calloc( 10, sizeof( char ) );
    search_file_name_exact( path, fname, config.mod_name, "Scenario" );
    extension = strrchr(fname,'.');
    extension++;
    if ( strcmp( extension, "lgmap" ) == 0 )
        return map_load_lgmap( fname, path );
    else if ( strcmp( extension, "lgdmap" ) == 0 )
        return map_load_lgdmap( fname, path );
    return 0;
}

/*
====================================================================
Удалить карту.
====================================================================
*/
void map_delete( )
{
    int i, j;
    if ( deploy_fields ) list_delete( deploy_fields );
    deploy_fields = 0;
    terrain_delete();
    if ( map ) {
        for ( i = 0; i < map_w; i++ )
            if ( map[i] ) {
                for ( j = 0; j < map_h; j++ )
                    if ( map[i][j].name )
                        free ( map[i][j].name );
                free( map[i] );
            }
        free( map );
    }
    if ( mask ) {
        for ( i = 0; i < map_w; i++ )
            if ( mask[i] )
                free( mask[i] );
        free( mask );
    }
    map = 0; mask = 0;
    map_w = map_h = 0;
}

/*
====================================================================
Получить плитку в x, y
====================================================================
*/
Map_Tile* map_tile( int x, int y )
{
    if ( x < 0 || y < 0 || x >= map_w || y >= map_h ) {
#if 0
        fprintf( stderr, "map_tile: map tile at %i,%i doesn't exist\n", x, y);
#endif
        return 0;
    }
    return &map[x][y];
}
Mask_Tile* map_mask_tile( int x, int y )
{
    if ( x < 0 || y < 0 || x >= map_w || y >= map_h ) {
#if 0
        fprintf( stderr, "map_tile: mask tile at %i,%i doesn't exist\n", x, y);
#endif
        return 0;
    }
    return &mask[x][y];
}

/*
====================================================================
Очистите переданные флаги маски карты.
====================================================================
*/
void map_clear_mask( int flags )
{
    int i, j;
    for ( i = 0; i < map_w; i++ )
        for ( j = 0; j < map_h; j++ ) {
            if ( flags & F_FOG ) mask[i][j].fog = 1;
            if ( flags & F_INVERSE_FOG ) mask[i][j].fog = 0;
            if ( flags & F_SPOT ) mask[i][j].spot = 0;
            if ( flags & F_IN_RANGE ) mask[i][j].in_range = 0;
            if ( flags & F_MOUNT ) mask[i][j].mount = 0;
            if ( flags & F_SEA_EMBARK ) mask[i][j].sea_embark = 0;
            if ( flags & F_AUX ) mask[i][j].aux = 0;
            if ( flags & F_INFL ) mask[i][j].infl = 0;
            if ( flags & F_INFL_AIR ) mask[i][j].air_infl = 0;
            if ( flags & F_VIS_INFL ) mask[i][j].vis_infl = 0;
            if ( flags & F_VIS_INFL_AIR ) mask[i][j].vis_air_infl = 0;
            if ( flags & F_BLOCKED ) mask[i][j].blocked = 0;
            if ( flags & F_BACKUP ) mask[i][j].backup = 0;
            if ( flags & F_MERGE_UNIT ) mask[i][j].merge_unit = 0;
            if ( flags & F_DEPLOY ) mask[i][j].deploy = 0;
            if ( flags & F_CTRL_GRND ) mask[i][j].ctrl_grnd = 0;
            if ( flags & F_CTRL_AIR ) mask[i][j].ctrl_air = 0;
            if ( flags & F_CTRL_SEA ) mask[i][j].ctrl_sea = 0;
            if ( flags & F_MOVE_COST ) mask[i][j].moveCost = 0;
            if ( flags & F_DISTANCE ) mask[i][j].distance = -1;
            if ( flags & F_DANGER ) mask[i][j].danger = 0;
            if ( flags & F_SPLIT_UNIT )
            {
                mask[i][j].split_unit = 0;
                mask[i][j].split_okay = 0;
            }
        }
}

/*
====================================================================
Поменять местами единицы. Возвращает предыдущую единицу или 0, если нет.
====================================================================
*/
Unit *map_swap_unit( Unit *unit )
{
    Unit *old;
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        old = map_tile( unit->x, unit->y )->a_unit;
        map_tile( unit->x, unit->y )->a_unit = unit;
    }
    else {
        old = map_tile( unit->x, unit->y )->g_unit;
        map_tile( unit->x, unit->y )->g_unit = unit;
    }
    unit->terrain = terrain_type_find( map[unit->x][unit->y].terrain_id[0] );
    return old;
}

/*
====================================================================
Вставить, удалить указатель объекта с карты.
====================================================================
*/
void map_insert_unit( Unit *unit )
{
    Unit *old = map_swap_unit( unit );
    if ( old ) {
        fprintf( stderr, "insert_unit_to_map: warning: "
                         "unit %s hasn't been removed properly from %i,%i:"
                         "overwrite it\n",
                     old->name, unit->x, unit->y );
    }
}
void map_remove_unit( Unit *unit )
{
    if ( unit_has_flag( unit->sel_prop, "flying" ) )
        map_tile( unit->x, unit->y )->a_unit = 0;
    else
        map_tile( unit->x, unit->y )->g_unit = 0;
}

/*
====================================================================
Получите соседние плитки по часовой стрелке с идентификатором от 0 до 5.
====================================================================
*/
Map_Tile* map_get_close_hex( int x, int y, int id )
{
    int next_x, next_y;
    if ( get_close_hex_pos( x, y, id, &next_x, &next_y ) )
        return &map[next_x][next_y];
    return 0;
}

/*
====================================================================
Добавить / установить засвет юнита во вспомогательную маску
====================================================================
*/
void map_add_unit_spot_mask_rec( Unit *unit, int x, int y, int points )
{
    int i, next_x, next_y;
    /* сломать, если эта плитка уже замечена */
    if ( mask[x][y].aux >= points ) return;
    /* spot tile */
    mask[x][y].aux = points;
    /* вычесть точки */
    points -= terrain_type_find( map[x][y].terrain_id[0] )->spt[cur_weather];
    /* если есть еще баллы, продолжить наблюдение */
    if ( points > 0 )
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( x, y, i, &next_x, &next_y ) )
                if ( !( terrain_type_find( map[next_x][next_y].terrain_id[0] )->flags[cur_weather] & NO_SPOTTING ) )
                    map_add_unit_spot_mask_rec( unit, next_x, next_y, points );
}
void map_add_unit_spot_mask( Unit *unit )
{
    int i, next_x, next_y;
    if ( unit->x < 0 || unit->y < 0 || unit->x >= map_w || unit->y >= map_h ) return;
    mask[unit->x][unit->y].aux = unit->sel_prop->spt;
    for ( i = 0; i < 6; i++ )
        if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
            map_add_unit_spot_mask_rec( unit, next_x, next_y, unit->sel_prop->spt );
}
void map_get_unit_spot_mask( Unit *unit )
{
    map_clear_mask( F_AUX );
    map_add_unit_spot_mask( unit );
}

/*
====================================================================
Установите диапазон движения юнита на in_range / sea_embark / mount.
====================================================================
*/
#ifdef OLD
void map_add_unit_move_mask_rec( Unit *unit, int x, int y, int distance, int points, int stage )
{
    int next_x, next_y, i, moves;
// if (strstr(unit->name,"leF")) printf("%s: points=%d stage=%d\n", unit->name, points, stage);
    /* сломать, если эта плитка уже отмечена */
    if ( mask[x][y].in_range >= points ) return;
    /* нельзя вводить внешние фрагменты карты */
    if ( x <= 0 || y <= 0 || x >= map_w - 1 || y >= map_h - 1 ) return;
    /* должен подняться, чтобы прийти сюда?
     * Устанавливайте флаг монтирования только на этапе 1 и только при условии, если
     * поле не могло быть достигнуто без объекта, имеющего
     * транспорт.
     */
    if ( stage == 1 && !mask[x][y].in_range
         && unit->embark == EMBARK_NONE && unit->trsp_prop.id ) {
        if ( unit->cur_mov - points < unit->prop.mov )
            mask[x][y].mount = 0;
        else
            mask[x][y].mount = 1;
    }
    /* отметить как достижимый */
    mask[x][y].in_range = points;
    /* запомнить расстояние */
    if (mask[x][y].distance==-1||distance<mask[x][y].distance)
        mask[x][y].distance = distance;
    /* вычесть точки */
    moves = unit_check_move( unit, x, y, stage );
    if ( moves == -1 ) {
        /* -1 означает, что он берет все очки и
           юнит должен находиться рядом с плиткой, чтобы войти в него
           поэтому он не должен был двигаться раньше */
        if ( points < ( stage == 0 && unit->cur_mov > unit->sel_prop->mov
			? unit->sel_prop->mov : unit->cur_mov ) )
            mask[x][y].in_range = 0;
        points = 0;
    }
    else
        points -= moves;
// if (strstr(unit->name,"leF")) printf("moves=%d mask[%d][%d].in_range=%d\n", moves, x, y, mask[x][y].in_range);
    /* go on if points left */
    if ( points > 0 )
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( x, y, i, &next_x, &next_y ) )
                if ( unit_check_move( unit, next_x, next_y, stage ) != 0 )
                    map_add_unit_move_mask_rec( unit, next_x, next_y, distance+1, points, stage );
}
void map_get_unit_move_mask( Unit *unit )
{
    int i, next_x, next_y, stage, distance = 0;

    map_clear_unit_move_mask();
    if ( unit->x < 0 || unit->y < 0 || unit->x >= map_w || unit->y >= map_h ) return;
    if ( unit->cur_mov == 0 ) return;
    mask[unit->x][unit->y].in_range = unit->cur_mov + 1;
    mask[unit->x][unit->y].distance = distance;
    for ( stage = 0; stage < 2; stage++) {
    for ( i = 0; i < 6; i++ )
        if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) ) {
            if ( map_check_unit_embark( unit, next_x, next_y, EMBARK_SEA, 0 ) ) {
                /* юнит может погрузиться на морской транспортер */
                mask[next_x][next_y].sea_embark = 1;
                continue;
            }
            if ( map_check_unit_debark( unit, next_x, next_y, EMBARK_SEA, 0 ) ) {
                /* юнит может высадиться с морского транспортера */
                mask[next_x][next_y].sea_embark = 1;
                continue;
            }
            if ( unit_check_move( unit, next_x, next_y, stage ) )
                map_add_unit_move_mask_rec( unit, next_x, next_y, distance+1,
			stage == 0 && unit->cur_mov > unit->sel_prop->mov
			? unit->sel_prop->mov : unit->cur_mov, stage );
        }
    }
    mask[unit->x][unit->y].blocked = 1;
}
#endif
/*
====================================================================
Проверить, может ли юнит войти в (x, y), если у него есть движение «точек»
осталось очков. «установленный» означает использовать базовую стоимость для
транспортер. Верните стоимость топлива (<= баллов). При входе
невозможно, «стоимость» не определена. Направление используется слоями карты направлений.
====================================================================
*/
int unit_can_enter_hex( Unit *unit, int x, int y, int is_close, int points, int mounted, int *cost, int *FinishMove, int direction )
{
    int i, temp, prev_x, prev_y;//, cur_terrain = 0;
    int base = terrain_get_mov( terrain_type_find( map[x][y].terrain_id[0] ), unit->sel_prop->mov_type, cur_weather );
    /* проверка дополнительных слоев */
    if ( direction >= 0 )
        for ( i = 1; i < MAX_LAYERS; i++)
        {
            if ( map[x][y].terrain_id[i] != 0 && get_close_hex_pos( x, y, (direction + 3) % 6, &prev_x, &prev_y ) )
            {
// DEBUG fprintf(stderr,"%d,%d:%d -> %d,%d:%d  %d (%d -> %d) \n", prev_x,prev_y, map[prev_x][prev_y].layers[i],x,y,map[x][y].layers[i], direction, map[prev_x][prev_y].layers[i] & (1L << DirectionToNumber[direction]),map[x][y].layers[i] & (1L << DirectionToNumber[(direction + 3) % 6]) );
                if ( ( map[prev_x][prev_y].layers[i] & (1L << DirectionToNumber[direction]) ) > 0 &&
                     ( map[x][y].layers[i] & (1L << DirectionToNumber[(direction + 3) % 6]) ) > 0 )
                {
                    temp = terrain_get_mov( terrain_type_find( map[x][y].terrain_id[i] ), unit->sel_prop->mov_type, cur_weather );
                    if ( temp < base )
                    {
                        base = temp;
                    }
                }
            }
            else
                break;
        }
    /* если мы проверим навесной корпус, придется использовать стоимость наземного транспортера */
    if ( mounted && unit->trsp_prop.id )
    {
        base = terrain_get_mov( terrain_type_find( map[x][y].terrain_id[0] ), unit->trsp_prop.mov_type, cur_weather );
        /* проверка дополнительных слоев */
        if ( direction >= 0 )
            for ( i = 1; i < MAX_LAYERS; i++)
            {
                if ( map[x][y].terrain_id[i] != 0 && get_close_hex_pos( x, y, (direction + 3) % 6, &prev_x, &prev_y ) )
                {
                    if ( ( map[prev_x][prev_y].layers[i] & (1L << DirectionToNumber[direction]) ) > 0 &&
                         ( map[x][y].layers[i] & (1L << DirectionToNumber[(direction + 3) % 6]) ) > 0 )
                    {
                        temp = terrain_get_mov( terrain_type_find( map[x][y].terrain_id[i] ), unit->trsp_prop.mov_type, cur_weather );
                        if ( temp < base )
                        {
                            base = temp;
                        }
                    }
                }
                else
                    break;
            }
    }
    /* союзные мостовые инженеры на реке? */
    if ( terrain_type_find( map[x][y].terrain_id[0] )->flags[cur_weather] & RIVER )
        if ( map[x][y].g_unit && unit_has_flag( map[x][y].g_unit->sel_prop, "bridge_eng" ) )
            if ( player_is_ally( unit->player, map[x][y].g_unit->player ) )
                base = 1; /* проверить погодные условия */
    /* непроходимый? */
    if (base==0) return 0;
    /* стоит все, но не близко? */
    if (base==-1&&!is_close) return 0;
    /* осталось мало очков? */
    if (base>0&&points<base) return 0;
    /* вы можете перемещаться по союзным юнитам, но тогда должна быть установлена ​​маска :: заблокирована
     * потому что вы не должны останавливаться на этой плитке */
    if ( ( x != unit->x  || y != unit->y ) && mask[x][y].spot ) {
        if ( map[x][y].a_unit && unit_has_flag( unit->sel_prop, "flying" ) ) {
            if ( !player_is_ally( unit->player, map[x][y].a_unit->player ) )
                return 0;
            else
                map_mask_tile( x, y )->blocked = 1;
        }
        if ( map[x][y].g_unit && !unit_has_flag( unit->sel_prop, "flying" ) ) {
            if ( !player_is_ally( unit->player, map[x][y].g_unit->player ) )
                return 0;
            else
                map_mask_tile( x, y )->blocked = 1;
        }
    }
    /* если уже все потратить; мы сделали */
    if (base==-1) { *cost = points; return 1; }
    *cost = base;
    /* вход завершает ваш ход */
    if ( config.zones_of_control )
    {
        if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
            if ( mask[x][y].vis_air_infl > 0 ) {
                (*FinishMove)++;
            }
        }
        else {
            if ( mask[x][y].vis_infl > 0 ) {
                (*FinishMove)++;
            }
        }
    }
    return 1;
}
/*
====================================================================
Проверить, достижим ли шестнадцатеричный (x, y) юнит. "расстояние" - это
расстояние до гекса, на котором стоит юнит. 'points' - это число
очков, оставшихся у объекта до попытки входа (x, y).
«установленный» означает повторную проверку с подвижной маской транспортера.
И установить маску [x] [y] .mount, если в досягаемости попала плитка, которая была
раньше не было. Направление используется слоями карты направлений.
====================================================================
*/
void map_add_unit_move_mask_rec( Unit *unit, int x, int y, int distance, int points, int mounted, int FinishMove, int direction )
{
    int i, next_x, next_y, cost = 0;
    /* сломать, если эта плитка уже отмечена */
    if ( mask[x][y].in_range >= points ) return;
    if ( mask[x][y].sea_embark ) return;
    /* нельзя вводить внешние фрагменты карты */
    if ( x <= 0 || y <= 0 || x >= map_w - 1 || y >= map_h - 1 ) return;
    /* мы можем войти? если да, сколько это стоит? */
    if (distance==0||unit_can_enter_hex(unit,x,y,(distance==1),points,mounted,&cost,&FinishMove, direction))
    {
        /* запомнить расстояние */
        if (mask[x][y].distance==-1||distance<mask[x][y].distance)
            mask[x][y].distance = distance;
        distance = mask[x][y].distance;
        /* перепроверьте: после вычета затрат должно быть больше или равно баллов
           в предыдущее состояние (расход топлива в зависимости от транспорта) */
        if ( mask[x][y].in_range > points-cost ) return;
        /* введите плитку новую или с большим количеством точек */
        points -= cost;
        if (mounted)
        {
            if (mask[x][y].in_range==-1)
            {
                mask[x][y].mount = 1;
            }
            /* получить общую стоимость переезда (смонтировано) */
            mask[x][y].moveCost = unit->trsp_prop.mov-points;
        }
        else
        {
            /* получить общие затраты на переезд (базовая несмонтированная) */
            mask[x][y].moveCost = unit->sel_prop->mov-points;
        }
        mask[x][y].in_range = points;
    }
    else
        points = 0;
    /* все очки израсходованы? если так, мы не можем идти дальше */
    if ( points==0 || FinishMove ) return;
    /* проверьте, близки ли гексы в диапазоне */
    for ( i = 0; i < 6; i++ )
        if ( get_close_hex_pos( x, y, i, &next_x, &next_y ) )
        {
            if ( distance==0 && map_check_unit_embark( unit, next_x, next_y, EMBARK_SEA, 0 ) ) {
                /* установка может погрузиться на морской транспортер */
                mask[next_x][next_y].sea_embark = 1;
                continue;
            }
            if ( distance==0 && map_check_unit_debark( unit, next_x, next_y, EMBARK_SEA, 0 ) ) {
                /* установка может высадиться с морского транспортера */
                mask[next_x][next_y].sea_embark = 1;
                continue;
            }
            map_add_unit_move_mask_rec( unit, next_x, next_y, distance+1, points, mounted, FinishMove, i );
        }
}
void map_get_unit_move_mask( Unit *unit )
{
    int x,y, FinishMove;
    map_clear_unit_move_mask();
    /* мы сохраняем семантическое изменение in_range локально, выполняя ручную настройку */
    for (x=0;x<map_w;x++) for(y=0;y<map_h;y++)
        mask[x][y].in_range=-1;
    if ( unit->embark == EMBARK_NONE && unit->trsp_prop.id )
    {
        /* это пойдет не так, если передвижная единица может двигаться с интервалами, которые
           логически правильная (в этом случае автоматическое монтирование не
           возможно, пользователю придется явно монтировать / размонтировать перед запуском
           двигаться); поэтому все подразделения должны двигаться за один раз */

        //начало патча: нам нужно подумать, если наш диапазон ограничен нехваткой топлива -trip
        //map_add_unit_move_mask_rec(unit,unit->x,unit->y,0,unit->prop.mov,0);
        //map_add_unit_move_mask_rec(unit,unit->x,unit->y,0,unit->trsp_prop.mov,1);
        int maxpoints = unit->prop.mov;
        if ((unit->prop.fuel || unit->trsp_prop.fuel) &&
                                                unit->cur_fuel < maxpoints) {
            maxpoints = unit->cur_fuel;
            //printf("limiting movement because fuel = %d\n", unit->cur_fuel);
        }
        FinishMove = 0;
        map_add_unit_move_mask_rec(unit,unit->x,unit->y,0,maxpoints,0, FinishMove, -1);
        /* fix  при нехватке топлива для использования всего диапазона наземного транспорта -trip */
        maxpoints = unit->cur_mov;
        if ((unit->prop.fuel || unit->trsp_prop.fuel) &&
                                                unit->cur_fuel < maxpoints) {
            maxpoints = unit->cur_fuel;
            //printf("limiting expansion of movement via transport because fuel = %d\n", unit->cur_fuel);
        }
        FinishMove = 0;
        map_add_unit_move_mask_rec(unit,unit->x,unit->y,0,maxpoints,1, FinishMove, -1);
        //конец патча -trip

    }
    else
    {
        FinishMove = 0;
        map_add_unit_move_mask_rec(unit,unit->x,unit->y,0,unit->cur_mov,0, FinishMove, -1);
    }
    for (x=0;x<map_w;x++) for(y=0;y<map_h;y++)
        mask[x][y].in_range++;
}
void map_clear_unit_move_mask()
{
    map_clear_mask( F_IN_RANGE | F_MOUNT | F_SEA_EMBARK | F_BLOCKED | F_AUX | F_MOVE_COST | F_DISTANCE );
}

/*
====================================================================
Записывает в заданный массив координаты всех дружественных
airport, возвращая количество записанных координат.
Массив должен быть достаточно большим, чтобы вместить все координаты.
Гарантируется, что функция никогда не напишет больше, чем (map_w * map_h)
записи.
====================================================================
*/
static int map_write_friendly_depot_list( Unit *unit, MapCoord *coords )
{
    int x, y;
    MapCoord *p = coords;
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            if ( map_is_allied_depot( &map[x][y], unit ) ) {
                p->x = x; p->y = y;
                ++p;
            }
    return p - coords;
}
/*
====================================================================
Устанавливает маску расстояния, начиная с аэродрома в точке (ax, ay).
====================================================================
*/
static void map_get_dist_air_mask( int ax, int ay, short *dist_air_mask )
{
    int x, y;
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ ) {
            int d = get_dist( ax, ay, x, y ) - 1;
            if (d < dist_air_mask[y*map_w+x])
                dist_air_mask[y*map_w+x] = d;
        }
    dist_air_mask[ay*map_w+ax] = 0;
}

/*
====================================================================
Воссоздает маску опасности для «юнита».
Для этого туман должен быть установлен на диапазон движения «юнит».
функция для правильной работы.
Стоимость движения маски должна быть установлена ​​как «юнит».
Возвращает 1, если установлена ​​хотя бы одна маска опасности, иначе 0.
====================================================================
*/
int map_get_danger_mask( Unit *unit )
{
    int i, x, y, retval = 0;
    short *dist_air_mask = alloca(map_w*map_h*sizeof dist_air_mask[0]);
    MapCoord *airfields = alloca(map_w*map_h*sizeof airfields[0]);
    int airfield_count = map_write_friendly_depot_list( unit, airfields );

    /* инициализировать маски */
    for (i = 0; i < map_w*map_h; i++) dist_air_mask[i] = DIST_AIR_MAX;

    /* собрать маску дистанции с учетом всех дружественных аэродромов */
    for ( i = 0; i < airfield_count; i++ )
        map_get_dist_air_mask( airfields[i].x, airfields[i].y, dist_air_mask );

    /* теперь пометьте как опасную зону любую клетку, чей следующий дружественный аэродром
       дальше, чем оставшееся количество топлива. */
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            if (!mask[x][y].fog) {
                int left = unit->cur_fuel - unit_calc_fuel_usage(unit, mask[x][y].moveCost);
                retval |=
                mask[x][y].danger =
                    /* Сначала сравните расстояние с предполагаемым количеством топлива. Если это
                     * слишком далеко, это опасно.
                     */
                    dist_air_mask[y*map_w+x] > left
                    /* В частности, разрешите поставляемую плитку.
                     */
                    && !map_get_unit_supply_level(x, y, unit);
            }
    return retval;
}
/*
====================================================================
Получите список путевых точек, по которым юнит движется к месту назначения.
Это включает проверку невидимого влияния вражеских юнитов (например,
Сюрприз-контакт).
====================================================================
*/
Way_Point* map_get_unit_way_points( Unit *unit, int x, int y, int *count, Unit **ambush_unit )
{
    //Way_Point way[50] = {0};
    //Way_Point reverse[50] ={0};

    Way_Point *way = NULL;
    Way_Point *reverse = NULL;
    int i;
    int next_x, next_y;
    /* такая же плитка? */
    if ( unit->x == x && unit->y == y ) return 0;
    /* выделить память */
    /* patch by Galland 2012 http://sourceforge.net/tracker/?group_id=23757&atid=379520 */
    int maxpoints = unit->cur_mov;
    if (mask[x][y].mount == 1)
        maxpoints = (unit->trsp_prop.mov>unit->prop.mov?unit->cur_mov:unit->prop.mov);

    way = calloc( maxpoints + 1, sizeof( Way_Point ) );
    reverse = calloc( maxpoints + 1, sizeof( Way_Point ) );
    /* конец патч */
    /* получить позиции проще всего в обратном порядке */
    next_x = x;
    next_y = y;
    *count = NULL;
    while ( next_x != unit->x || next_y != unit->y ) {
        reverse[*count].x = next_x; reverse[*count].y = next_y;
        map_get_next_unit_point( unit, next_x, next_y, &next_x, &next_y );
        (*count)++;
        }
    reverse[*count].x = unit->x; reverse[*count].y = unit->y; (*count)++;
    for ( i = 0; i < *count; i++ ) {
        way[i].x = reverse[(*count) - 1 - i].x;
        way[i].y = reverse[(*count) - 1 - i].y;
        way[i].cost = mask[way[i].x][way[i].y].moveCost;
    }

    free( reverse );
    // точки пути отладки
    //printf( "'%s': %i,%i", unit->name, way[0].x, way[0].y );
    //for ( i = 1; i < *count; i++ )
    //    printf( " -> %i,%i", way[i].x, way[i].y );
    //printf( "\n" );
    /* проверить на засаду и влияние
     * если на пути стоит юнит, он должен быть врагом (друзья, пятнистые враги не допускаются)
     * так что срезайте путь до этого way_point и установите ambush_unit
     * если у незапятнанной плитки влияние> 0, значит, поблизости находится враг, и наш отряд должен остановиться
     */
    for ( i = 1; i < *count; i++ ) {
        /* проверьте, ожидает ли на этой плитке юнит */
        /* if mask::blocked установлен, это собственный отряд, поэтому не проверяйте засаду */
        if ( !map_mask_tile( way[i].x, way[i].y )->blocked ) {
            if ( map_tile( way[i].x, way[i].y )->g_unit )
                if ( !unit_has_flag( unit->sel_prop, "flying" ) ) {
                    *ambush_unit = map_tile( way[i].x, way[i].y )->g_unit;
                    break;
                }
            if ( map_tile( way[i].x, way[i].y )->a_unit )
                if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
                    *ambush_unit = map_tile( way[i].x, way[i].y )->a_unit;
                    break;
                }
        }
        /* если мы доберемся сюда, юниты не ждут, но, возможно, тоже приблизимся к плитке */
        /* поэтому проверьте плитку движущегося юнита, если на него влияет ранее незапятнанный юнит */
        if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
            if ( map_mask_tile( way[i - 1].x, way[i - 1].y )->air_infl && !map_mask_tile( way[i - 1].x, way[i - 1].y )->vis_air_infl )
                break;
        }
        else {
            if ( map_mask_tile( way[i - 1].x, way[i - 1].y )->infl && !map_mask_tile( way[i - 1].x, way[i - 1].y )->vis_infl )
                break;
        }
    }
    if ( i < *count ) *count = i; /* враг на пути; сократить */
    return way;
}

/*
====================================================================
Резервное копирование / восстановление точечной маски в / из резервной маски.
====================================================================
*/
void map_backup_spot_mask()
{
    int x, y;
    map_clear_mask( F_BACKUP );
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            map_mask_tile( x, y )->backup = map_mask_tile( x, y )->spot;
}
void map_restore_spot_mask()
{
    int x, y;
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            map_mask_tile( x, y )->spot = map_mask_tile( x, y )->backup;
    map_clear_mask( F_BACKUP );
}

/*
====================================================================
Получите юнитов по слиянию и установите
маска «слияние». Максимум единиц MAP_MERGE_UNIT_LIMIT.
Все неиспользуемые записи в партнерах устанавливаются на 0.
====================================================================
*/
void map_get_merge_units( Unit *unit, Unit **partners, int *count )
{
    Unit *partner;
    int i, next_x, next_y;
    *count = 0;
    map_clear_mask( F_MERGE_UNIT );
    memset( partners, 0, sizeof( Unit* ) * MAP_MERGE_UNIT_LIMIT );
    /* проверьте окружающие плитки */
    for ( i = 0; i < 6; i++ )
        if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) ) {
            partner = 0;
            if ( map[next_x][next_y].g_unit && unit_check_merge( unit, map[next_x][next_y].g_unit ) )
                partner = map[next_x][next_y].g_unit;
            else
                if ( map[next_x][next_y].a_unit && unit_check_merge( unit, map[next_x][next_y].a_unit ) )
                    partner = map[next_x][next_y].a_unit;
            if ( partner ) {
                partners[(*count)++] = partner;
                mask[next_x][next_y].merge_unit = partner;
            }
        }
}

/*
====================================================================
Проверьте, может ли юнит передать силу отряду (если не NULL) или создать
автономный модуль (если unit NULL) по координатам.
====================================================================
*/
int map_check_unit_split( Unit *unit, int str, int x, int y, Unit *dest )
{
    if (unit->str-str<4) return 0;
    if (dest)
    {
        int old_str, ret;
        old_str = unit->str; unit->str = str;
        ret = unit_check_merge(unit,dest); /* is equal for now */
        unit->str = old_str;
        return ret;
    }
    else
    {
        if (str<4) return 0;
        if (!is_close(unit->x,unit->y,x,y)) return 0;
        if ( unit_has_flag( unit->sel_prop, "flying" ) && map[x][y].a_unit) return 0;
        if (!unit_has_flag( unit->sel_prop, "flying" ) && map[x][y].g_unit) return 0;
        if (!terrain_get_mov(terrain_type_find( map[x][y].terrain_id[0] ),unit->sel_prop->mov_type,cur_weather)) return 0;
    }
    return 1;
}

/*
====================================================================
Получите разделенных юнитов, предполагая, что отряд хочет дать силу 'str'
точки и установите маску «разделение». Максимум единиц MAP_SPLIT_UNIT_LIMIT.
Все неиспользованные записи в партнерах устанавливаются на 0. 'str' должно быть допустимым количеством,
здесь это не проверяется.
====================================================================
*/
void map_get_split_units_and_hexes( Unit *unit, int str, Unit **partners, int *count )
{
    Unit *partner;
    int i, next_x, next_y;
    *count = 0;
    map_clear_mask( F_SPLIT_UNIT );
    memset( partners, 0, sizeof( Unit* ) * MAP_SPLIT_UNIT_LIMIT );
    /* проверьте окружающие плитки */
    for ( i = 0; i < 6; i++ )
        if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) ) {
            partner = 0;
            if ( map[next_x][next_y].g_unit && map_check_unit_split( unit, str, next_x, next_y, map[next_x][next_y].g_unit ) )
                partner = map[next_x][next_y].g_unit;
            else
                if ( map[next_x][next_y].a_unit && map_check_unit_split( unit, str, next_x, next_y, map[next_x][next_y].a_unit ) )
                    partner = map[next_x][next_y].a_unit;
            else
                if ( map_check_unit_split( unit, str, next_x, next_y, 0 ) )
                    mask[next_x][next_y].split_okay = 1;
            if ( partner ) {
                partners[(*count)++] = partner;
                mask[next_x][next_y].split_unit = partner;
            }
        }
}


/*
====================================================================
Получите список (vis_units) всех видимых единиц, проверив маску пятна.
====================================================================
*/
void map_get_vis_units( void )
{
    int x, y;
    if ( vis_units )
    {
        list_clear( vis_units );
        for ( x = 0; x < map_w; x++ )
            for ( y = 0; y < map_h; y++ )
                if ( mask[x][y].spot || ( cur_player && cur_player->ctrl == PLAYER_CTRL_CPU ) ) {
                    if ( map[x][y].g_unit ) list_add( vis_units, map[x][y].g_unit );
                    if ( map[x][y].a_unit ) list_add( vis_units, map[x][y].a_unit );
                }
    }
}

/*
====================================================================
Нарисуйте на поверхности фрагмент карты местности. (затуманивается, если установлена ​​маска :: туман)
====================================================================
*/
void map_draw_terrain( SDL_Surface *surf, int map_x, int map_y, int x, int y )
{
    Map_Tile *tile;
    if ( map_x < 0 || map_y < 0 || map_x >= map_w || map_y >= map_h ) return;
    tile = &map[map_x][map_y];
    /* земля */
    DEST( surf, x, y, hex_w, hex_h );
    if ( mask[map_x][map_y].fog )
        SOURCE( terrain_type_find( tile->terrain_id[0] )->images_fogged[ground_conditions_get_index( weather_types[cur_weather].ground_conditions )],
                tile->image_offset_x, tile->image_offset_y )
    else
        SOURCE( terrain_type_find( tile->terrain_id[0] )->images[ground_conditions_get_index( weather_types[cur_weather].ground_conditions )],
                tile->image_offset_x, tile->image_offset_y )
    blit_surf();
    /* национальный флаг */
    if ( tile->nation != 0 ) {
        nation_draw_flag( tile->nation, surf,
                          x + ( ( hex_w - nation_flag_width ) >> 1 ),
                          y + hex_h - nation_flag_height - 2,
                          tile->obj );
    }
    /* сетка */
    if ( config.grid ) {
        DEST( surf, x, y, hex_w, hex_h );
        SOURCE( terrain_icons->grid, 0, 0 );
        blit_surf();
    }
    /* финальный розыгрыш */
    DEST( sdl.screen, 0, 30, surf->w, surf->h );
    FULL_SOURCE( surf )
    blit_surf();
}
/*
====================================================================
Нарисуйте блоки плитки. Если установлена ​​маска :: fog, юниты не отрисовываются.
Если 'ground' имеет значение True, наземный блок отображается как основной.
и воздушный блок нарисован маленьким (и наоборот).
Если установлено 'select', добавляется рамка выбора.
====================================================================
*/
void map_draw_units( SDL_Surface *surf, int map_x, int map_y, int x, int y, int ground, int select )
{
    Unit *unit = 0;
    Map_Tile *tile;
    if ( map_x < 0 || map_y < 0 || map_x >= map_w || map_y >= map_h ) return;
    tile = &map[map_x][map_y];
    /* юниты */
    if ( MAP_CHECK_VIS( map_x, map_y ) ) {
        if ( tile->g_unit ) {
            if ( ground || tile->a_unit == 0 ) {
                /* большой наземный юнит */
                DEST( surf,
                      x + ( (hex_w - tile->g_unit->sel_prop->icon_w) >> 1 ),
                      y + ( ( hex_h - tile->g_unit->sel_prop->icon_h ) >> 1 ),
                      tile->g_unit->sel_prop->icon_w, tile->g_unit->sel_prop->icon_h );
                SOURCE( tile->g_unit->sel_prop->icon, tile->g_unit->icon_offset, 0 );
                blit_surf();
                unit = tile->g_unit;
            }
            else {
                /* небольшой наземный юнит */
                DEST( surf,
                      x + ( (hex_w - tile->g_unit->sel_prop->icon_tiny_w) >> 1 ),
                      y + ( ( hex_h - tile->g_unit->sel_prop->icon_tiny_h ) >> 1 ) + 4,
                      tile->g_unit->sel_prop->icon_tiny_w, tile->g_unit->sel_prop->icon_tiny_h );
                SOURCE( tile->g_unit->sel_prop->icon_tiny, tile->g_unit->icon_tiny_offset, 0 );
                blit_surf();
                unit = tile->a_unit;
            }
        }
        if ( tile->a_unit ) {
            if ( !ground || tile->g_unit == 0 ) {
               /* большой воздушный юнит */
                if ( tile->a_unit->sel_prop->icon_type == UNIT_ICON_FIXED )
                    DEST( surf,
                         x,
                          y - 10,
                          tile->a_unit->sel_prop->icon_w, tile->a_unit->sel_prop->icon_h )
                else
                    DEST( surf,
                          x + ( (hex_w - tile->a_unit->sel_prop->icon_w) >> 1 ),
                          y + 6,
                          tile->a_unit->sel_prop->icon_w, tile->a_unit->sel_prop->icon_h )
                SOURCE( tile->a_unit->sel_prop->icon, tile->a_unit->icon_offset, 0 );
                blit_surf();
                unit = tile->a_unit;
            }
            else {
                /* небольшой воздушный юнит */
                if ( tile->a_unit->sel_prop->icon_type == UNIT_ICON_FIXED )
                    DEST( surf,
                          x + ( (hex_w - tile->a_unit->sel_prop->icon_tiny_w) >> 1 ),
                          y - 6,
                          tile->a_unit->sel_prop->icon_tiny_w, tile->a_unit->sel_prop->icon_tiny_h )
                else
                    DEST( surf,
                          x + ( (hex_w - tile->a_unit->sel_prop->icon_tiny_w) >> 1 ),
                          y + 6,
                          tile->a_unit->sel_prop->icon_tiny_w, tile->a_unit->sel_prop->icon_tiny_h )
                SOURCE( tile->a_unit->sel_prop->icon_tiny, tile->a_unit->icon_tiny_offset, 0 );
                blit_surf();
                unit = tile->g_unit;
            }
        }

        /* значки информации об объекте */
        if ( unit && config.show_bar ) {
            /* сила */
            DEST( surf,
                  x + ( ( hex_w - unit_info_icons->str_w ) >> 1 ),
                  y + hex_h - unit_info_icons->str_h,
                  unit_info_icons->str_w, unit_info_icons->str_h );
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
            /* только для текущего игрока */
            if ( unit->player == cur_player ) {
                /* атака */
                if ( unit->cur_atk_count > 0 ) {
                    DEST( surf, x + ( hex_w - hex_x_offset ), y + hex_h - unit_info_icons->atk->h,
                          unit_info_icons->atk->w, unit_info_icons->atk->h );
                    SOURCE( unit_info_icons->atk, 0, 0 );
                    blit_surf();
                }
                /* переехать */
                if ( unit->cur_mov > 0 ) {
                    DEST( surf, x + hex_x_offset - unit_info_icons->mov->w, y + hex_h - unit_info_icons->mov->h,
                          unit_info_icons->mov->w, unit_info_icons->mov->h );
                    SOURCE( unit_info_icons->mov, 0, 0 );
                    blit_surf();
                }
                /* охрана */
                if ( unit->is_guarding ) {
                    DEST( surf, x + ((hex_w-unit_info_icons->guard->w)>>1),
                          y + hex_h - unit_info_icons->guard->h,
                          unit_info_icons->guard->w, unit_info_icons->guard->h );
                    SOURCE( unit_info_icons->guard, 0, 0 );
                    blit_surf();
                }
            }
        }
    }
    /* рамка выбора */
    if ( select ) {
        DEST( surf, x, y, hex_w, hex_h );
        SOURCE( terrain_icons->select, 0, 0 );
        blit_surf();
    }
    /* финальный розыгрыш */
    DEST( sdl.screen, 0, 30, surf->w, surf->h );
    FULL_SOURCE( surf )
    blit_surf();
}
/*
====================================================================
Нарисуйте плитку опасности. Ожидает, что "surf" будет содержать полностью нарисованную плитку в
данная позиция, которая будет окрашена наложением опасности
поверхность местности.
====================================================================
*/
void map_apply_danger_to_tile( SDL_Surface *surf, int map_x, int map_y, int x, int y )
{
    DEST( surf, x, y, hex_w, hex_h );
    SOURCE( terrain_icons->danger, 0, 0 );
    alpha_blit_surf( DANGER_ALPHA );
    /* финальный розыгрыш */
    DEST( sdl.screen, 0, 30, surf->w, surf->h );
    FULL_SOURCE( surf )
    blit_surf();
}
/*
====================================================================
Нарисуйте местность и юниты.
====================================================================
*/
void map_draw_tile( SDL_Surface *surf, int map_x, int map_y, int x, int y, int ground, int select )
{
    map_draw_terrain( surf, map_x, map_y, x, y - 30 );
    map_draw_units( surf, map_x, map_y, x, y - 30, ground, select );
}

/*
====================================================================
Установить / обновить маску пятна текущим игроком или юнитом движка.
Обновление добавляет плитки, видимые юнитом.
====================================================================
*/
void map_set_spot_mask()
{
    int i;
    int x, y, next_x, next_y;
    Unit *unit;
    map_clear_mask( F_SPOT );
    map_clear_mask( F_AUX ); /* сначала буфер здесь */
    /* получить spot_mask для каждого юнита и добавить в туман */
    /* использовать map :: mask :: aux как буфер */
    if ( units )
    {
        list_reset( units );
        for ( i = 0; i < units->count; i++ ) {
            unit = list_next( units );
            if ( unit->killed ) continue;
            if ( player_is_ally( cur_player, unit->player ) ) /* это ваше подразделение или, по крайней мере, оно союзное ... */
                map_add_unit_spot_mask( unit );
        }
    }
    /* проверьте все флаги; если флаг принадлежит вам или кому-либо из ваших партнеров, вы тоже видите окружающее */
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            if ( map[x][y].player != 0 )
                if ( player_is_ally( cur_player, map[x][y].player ) ) {
                    mask[x][y].aux = 1;
                    for ( i = 0; i < 6; i++ )
                        if ( get_close_hex_pos( x, y, i, &next_x, &next_y ) )
                            mask[next_x][next_y].aux = 1;
                }
    /* превратить в туман */
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            if ( mask[x][y].aux || !config.fog_of_war )
                 mask[x][y].spot = 1;
    /* обновить список видимых юнитов */
    map_get_vis_units();
}
void map_update_spot_mask( Unit *unit, int *enemy_spotted )
{
    int x, y;
    *enemy_spotted = 0;
    if ( player_is_ally( cur_player, unit->player ) ) {
        /* это ваше подразделение или, по крайней мере, оно союзное ... */
        map_get_unit_spot_mask( unit );
        for ( x = 0; x < map_w; x++ )
            for ( y = 0; y < map_h; y++ )
                if ( mask[x][y].aux ) {
                    /* если в этой вспомогательной маске есть враг, которого раньше не заметили */
                    /* установить enemy_spotted true */
                    if ( !mask[x][y].spot ) {
                        if ( map[x][y].g_unit && !player_is_ally( unit->player, map[x][y].g_unit->player ) )
                            *enemy_spotted = 1;
                        if ( map[x][y].a_unit && !player_is_ally( unit->player, map[x][y].a_unit->player ) )
                            *enemy_spotted = 1;
                    }
                    mask[x][y].spot = 1;
                }
    }
}

/*
====================================================================
Установите mask :: fog (который является фактическим туманом двигателя) либо
точечная маска, маска in_range (закрывает sea_embark), маска слияния,
развернуть маску.
====================================================================
*/
void map_set_fog( int type )
{
    int x, y;
    for ( y = 0; y < map_h; y++ )
        for ( x = 0; x < map_w; x++ )
            switch ( type ) {
                case F_SPOT: mask[x][y].fog = !mask[x][y].spot; break;
                case F_IN_RANGE: mask[x][y].fog = ( (!mask[x][y].in_range && !mask[x][y].sea_embark) || mask[x][y].blocked ); break;
                case F_MERGE_UNIT: mask[x][y].fog = !mask[x][y].merge_unit; break;
                case F_SPLIT_UNIT: mask[x][y].fog = !mask[x][y].split_unit&&!mask[x][y].split_okay; break;
                case F_DEPLOY: mask[x][y].fog = !mask[x][y].deploy; break;
                default: mask[x][y].fog = 0; break;
            }
}

/*
====================================================================
Установите туман на маску пятна игрока, используя mask :: aux (не mask :: spot)
====================================================================
*/
void map_set_fog_by_player( Player *player )
{
    int i;
    int x, y, next_x, next_y;
    Unit *unit;
    map_clear_mask( F_AUX ); /* сначала буфер здесь */
    /* юниты */
    list_reset( units );
    for ( i = 0; i < units->count; i++ ) {
        unit = list_next( units );
        if ( unit->killed ) continue;
        if ( player_is_ally( player, unit->player ) ) /* это ваше подразделение или, по крайней мере, оно союзное ... */
            map_add_unit_spot_mask( unit );
    }
    /* проверьте все флаги; если флаг принадлежит вам или кому-либо из ваших партнеров, вы тоже видите окружающее */
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            if ( map[x][y].player != 0 )
                if ( player_is_ally( player, map[x][y].player ) ) {
                    mask[x][y].aux = 1;
                    for ( i = 0; i < 6; i++ )
                        if ( get_close_hex_pos( x, y, i, &next_x, &next_y ) )
                            mask[next_x][next_y].aux = 1;
                }
    /* превратить в туман */
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            if ( mask[x][y].aux || !config.fog_of_war )
                 mask[x][y].fog = 0;
            else
                 mask[x][y].fog = 1;
}

/*
====================================================================
Измените различные маски влияния.
====================================================================
*/
void map_add_unit_infl( Unit *unit )
{
    int i, next_x, next_y;
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        mask[unit->x][unit->y].air_infl++;
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
                mask[next_x][next_y].air_infl++;
    }
    else {
        mask[unit->x][unit->y].infl++;
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
                mask[next_x][next_y].infl++;
    }
}
void map_remove_unit_infl( Unit *unit )
{
    int i, next_x, next_y;
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        mask[unit->x][unit->y].air_infl--;
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
                mask[next_x][next_y].air_infl--;
    }
    else {
        mask[unit->x][unit->y].infl--;
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
                mask[next_x][next_y].infl--;
    }
}
void map_remove_vis_unit_infl( Unit *unit )
{
    int i, next_x, next_y;
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        mask[unit->x][unit->y].vis_air_infl--;
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
                mask[next_x][next_y].vis_air_infl--;
    }
    else {
        mask[unit->x][unit->y].vis_infl--;
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &next_x, &next_y ) )
                mask[next_x][next_y].vis_infl--;
    }
}
void map_set_infl_mask()
{
    Unit *unit = 0;
    map_clear_mask( F_INFL | F_INFL_AIR );
    /* добавить влияние всех враждебных юнитов */
    if ( units )
    {
        list_reset( units );
        while ( ( unit = list_next( units ) ) )
            if ( !unit->killed && !player_is_ally( cur_player, unit->player ) )
                map_add_unit_infl( unit );
        /* видимое влияние также должно быть обновлено */
        map_set_vis_infl_mask();
    }
}
void map_set_vis_infl_mask()
{
    Unit *unit = 0;
    map_clear_mask( F_VIS_INFL | F_VIS_INFL_AIR );
    /* добавить влияние всех враждебных юнитов */
    list_reset( units );
    while ( ( unit = list_next( units ) ) )
        if ( !unit->killed && !player_is_ally( cur_player, unit->player ) )
            if ( map_mask_tile( unit->x, unit->y )->spot )
                map_add_vis_unit_infl( unit );
}

/*
====================================================================
Проверить, может ли установка производить посадку в воздух / море / высадку в точке x, y.
Если 'init'! = 0, используются упрощенные правила для развертывания
====================================================================
*/
int map_check_unit_embark( Unit *unit, int x, int y, int type, int init )
{
    int i, nx, ny;
    if ( x < 0 || y < 0 || x >= map_w || y >= map_h ) return 0;
    if ( type == EMBARK_AIR ) {
        if ( unit_has_flag( unit->sel_prop, "flying" ) ) return 0;
        if ( unit_has_flag( unit->sel_prop, "swimming" ) ) return 0;
        if ( cur_player->air_trsp == 0 ) return 0;
        if ( unit->embark != EMBARK_NONE ) return 0;
        if ( !init && map[x][y].a_unit ) return 0;
        if ( unit->player->air_trsp_used >= unit->player->air_trsp_count ) return 0;
        if ( !init && !unit->unused ) return 0;
        if ( !init && !( terrain_type_find( map[x][y].terrain_id[0] )->flags[cur_weather] & SUPPLY_AIR ) ) return 0;
        if ( init && !unit_has_flag( unit->sel_prop, "parachute" ) && !( terrain_type_find( map[x][y].terrain_id[0] )->flags[cur_weather] & SUPPLY_AIR ) ) return 0;
        if ( !unit_has_flag( unit->sel_prop, "air_trsp_ok" ) ) return 0;
        if ( init && unit_has_flag( &unit->trsp_prop, "transporter" ) ) return 0;
        return 1;
    }
    if ( type == EMBARK_SEA ) {
        if ( unit_has_flag( unit->sel_prop, "flying" ) ) return 0;
        if ( unit_has_flag( unit->sel_prop, "swimming" ) ) return 0;
        if ( cur_player->sea_trsp == 0 ) return 0;
        if ( unit->embark != EMBARK_NONE || ( !init && unit->sel_prop->mov == 0 ) ) return 0;
        if ( !init && map[x][y].g_unit ) return 0;
        if ( unit->player->sea_trsp_used >= unit->player->sea_trsp_count ) return 0;
        if ( !init && !unit->unused ) return 0;
        if ( terrain_get_mov( terrain_type_find( map[x][y].terrain_id[0] ), unit->player->sea_trsp->mov_type, cur_weather ) == 0 ) return 0;
        /* в основном мы должны быть близко к гавани, но город просто
           у воды тоже нормально, иначе было бы тоже
           ограничительный. */
        if ( !init ) {
            if ( terrain_type_find( map[x][y].terrain_id[0] )->flags[cur_weather] & SUPPLY_GROUND )
                return 1;
            for ( i = 0; i < 6; i++ )
                if ( get_close_hex_pos( x, y, i, &nx, &ny ) )
                    if ( terrain_type_find( map[nx][ny].terrain_id[0] )->flags[cur_weather] & SUPPLY_GROUND )
                        return 1;
        }
        return init;
    }
    return 0;
}
int map_check_unit_debark( Unit *unit, int x, int y, int type, int init )
{
    if ( x < 0 || y < 0 || x >= map_w || y >= map_h ) return 0;
    if ( type == EMBARK_SEA ) {
        if ( unit->embark != EMBARK_SEA ) return 0;
        if ( !init && map[x][y].g_unit ) return 0;
        if ( !init && !unit->unused ) return 0;
        if ( !init && terrain_get_mov( terrain_type_find( map[x][y].terrain_id[0] ), unit->prop.mov_type, cur_weather ) == 0 ) return 0;
        return 1;
    }
    if ( type == EMBARK_AIR ) {
        if ( unit->embark != EMBARK_AIR ) return 0;
        if ( !init && map[x][y].g_unit ) return 0;
        if ( !init && !unit->unused ) return 0;
        if ( !init && terrain_get_mov( terrain_type_find( map[x][y].terrain_id[0] ), unit->prop.mov_type, cur_weather ) == 0 ) return 0;
        if ( !init && !( terrain_type_find( map[x][y].terrain_id[0] )->flags[cur_weather] & SUPPLY_AIR ) && !unit_has_flag( &unit->prop, "parachute" ) )
            return 0;
        return 1;
    }
    return 0;
}

/*
====================================================================
Посадить / высадить отряд и вернуться, если был обнаружен враг.
Если «вражеское пятно» равно 0, не пересчитывать маску пятна.
Если координаты объекта или x и y выходят за границы, соответствующие
плиткой не манипулируют.
Для высадки в воздухе (x, y) = (- 1, -1) означает развертывание на аэродроме, в противном случае
это парашютист, что означает: он может уплыть, он может потерять
сила, он может не двигаться.
====================================================================
*/
void map_embark_unit( Unit *unit, int x, int y, int type, int *enemy_spotted )
{
    Map_Tile *tile;
    if ( type == EMBARK_AIR ) {
        /* не помечен как использованный, поэтому может снова высадиться непосредственно */

        /* покинуть наземный транспортер */
        memset( &unit->trsp_prop, 0, sizeof( Unit_Lib_Entry ) );
        /* установите и измените значение на air_tran_prop */
        memcpy( &unit->trsp_prop, cur_player->air_trsp, sizeof( Unit_Lib_Entry ) );
        unit->sel_prop = &unit->trsp_prop;
        unit->embark = EMBARK_AIR;
        /* юнит сейчас летает */
        tile = map_tile(x, y);
        if (tile) tile->a_unit = unit;
        tile = map_tile(unit->x, unit->y);
        if (tile) tile->g_unit = 0;
        /* установить полный диапазон движения */
        unit->cur_mov = unit->trsp_prop.mov;
        /* отменить атаки */
        unit->cur_atk_count = 0;
        /* нет окопов */
        unit->entr = 0;
        /* отрегулировать смещение изображения */
        unit_adjust_icon( unit );
        /* другой air_tran используется */
        cur_player->air_trsp_used++;

        if (enemy_spotted)
            /* в любом случае ваше пятно должно быть обновлено */
            map_update_spot_mask( unit, enemy_spotted );
        return;
    }
    if ( type == EMBARK_SEA ) {
        /* действие предпринято */
        unit->unused = 0;
        /* складской наземный транспортер */
        memcpy( &unit->land_trsp_prop, &unit->trsp_prop, sizeof( Unit_Lib_Entry ) );
        /* установить и изменить на sea_tran_prop */
        memcpy( &unit->trsp_prop, cur_player->sea_trsp, sizeof( Unit_Lib_Entry ) );
        unit->sel_prop = &unit->trsp_prop;
        unit->embark = EMBARK_SEA;
        /* обновить позицию */
        tile = map_tile(unit->x, unit->y);
        if (tile) tile->g_unit = 0;
        unit->x = x; unit->y = y;
        tile = map_tile(x, y);
        if (tile) tile->g_unit = unit;
        /* установить полный диапазон движения */
        unit->cur_mov = unit->trsp_prop.mov;
        /* отменить атаки */
        unit->cur_atk_count = 0;
        /* нет окопов */
        unit->entr = 0;
        /* отрегулировать смещение изображения */
        unit_adjust_icon( unit );
        /* другой air_tran используется */
        cur_player->sea_trsp_used++;

        if (enemy_spotted)
            /* в любом случае ваше пятно должно быть обновлено */
            map_update_spot_mask( unit, enemy_spotted );
        return;
    }
}
void map_debark_unit( Unit *unit, int x, int y, int type, int *enemy_spotted )
{
    Map_Tile *tile;
    if ( type == EMBARK_SEA ) {
        /* действие предпринято */
        unit->unused = 0;
        /* вернуться к unit_prop */
        memcpy( &unit->trsp_prop, &unit->land_trsp_prop, sizeof( Unit_Lib_Entry ) );
        memset( &unit->land_trsp_prop, 0, sizeof( Unit_Lib_Entry ) );
        unit->sel_prop = &unit->prop;
        unit->embark = EMBARK_NONE;
        /* установить позицию */
        tile = map_tile(unit->x, unit->y);
        if (tile) tile->g_unit = 0;
        unit->x = x; unit->y = y;
        tile = map_tile(x, y);
        if (tile) tile->g_unit = unit;
        if ( tile && tile->nation ) {
            tile->nation = unit->nation;
            tile->player = unit->player;
        }
        /* движение запрещено */
        unit->cur_mov = 0;
        /* отменить атаки */
        unit->cur_atk_count = 0;
        /* нет окопов */
        unit->entr = 0;
        /* отрегулировать смещение изображения */
        unit_adjust_icon( unit );
        /* свободный морской транспортник */
        cur_player->sea_trsp_used--;

        if (enemy_spotted)
            /* в любом случае ваше пятно должно быть обновлено */
            map_update_spot_mask( unit, enemy_spotted );
        return;
    }
    if ( type == EMBARK_AIR || type == DROP_BY_PARACHUTE ) {
        /* юнит может двигаться прямо */

        /* вернуться к unit_prop */
        memset( &unit->trsp_prop, 0, sizeof( Unit_Lib_Entry ) );
        unit->sel_prop = &unit->prop;
        unit->embark = EMBARK_NONE;
        /* спуститься на землю */
        tile = map_tile(unit->x,unit->y);
        if ( tile ) tile->a_unit = 0;
        if (type == EMBARK_AIR)
        {
            if (tile) tile->g_unit = unit;
        }
        else
        {
            /* 70% шанс дрейфа */
            if (DICE(10)<=7)
            {
                int i,nx,ny,c=0,j;
                for (i=0;i<6;i++)
                    if (get_close_hex_pos(x,y,i,&nx,&ny))
                        if (map[nx][ny].g_unit==0)
                            if (terrain_get_mov(terrain_type_find( map[nx][ny].terrain_id[0] ),unit->prop.mov_type,cur_weather)!=0)
                                c++;
                j = DICE(c)-1;
                for (i=0,c=0;i<6;i++)
                    if (get_close_hex_pos(x,y,i,&nx,&ny))
                        if (map[nx][ny].g_unit==0)
                            if (terrain_get_mov(terrain_type_find( map[nx][ny].terrain_id[0] ),unit->prop.mov_type,cur_weather)!=0)
                            {
                                if (j==c)
                                {
                                    unit->x = nx; unit->y = ny;
                                    break;
                                }
                                else
                                    c++;
                            }
            }
            else
            {
                unit->x = x; unit->y = y;
            }
            tile = map_tile(unit->x,unit->y);
            if (tile) tile->g_unit = unit;
        }
        /* шанс умереть за каждого парашютиста: 6% -exp% */
        if (type == DROP_BY_PARACHUTE)
        {
            int i,c=0,l=6-unit->exp_level;
            for (i=0;i<unit->str;i++)
                if (DICE(100)<=l)
                    c++;
            unit->str -= c;
            if (unit->str<=0) unit->str=1; /* помилуй */
        }
        /* установить движение + атаки */
        if (type == EMBARK_AIR)
        {
            unit->cur_mov = unit->sel_prop->mov;
            unit->cur_atk_count = unit->sel_prop->atk_count;
        }
        else
        {
            unit->cur_mov = 0; unit->cur_atk_count = 0; unit->unused = 0;
        }
        /* нет окопов */
        unit->entr = 0;
        /* отрегулировать смещение изображения */
        unit_adjust_icon( unit );
        /* свободный морской транспортник */
        cur_player->air_trsp_used--;

        if (enemy_spotted)
            /* в любом случае ваше пятно должно быть обновлено */
            map_update_spot_mask( unit, enemy_spotted );
        return;
    }
}

/*
====================================================================
Отметьте, что это поле является полем развертывания для данного игрока.
====================================================================
*/
void map_set_deploy_field( int mx, int my, int player ) {
    if (!deploy_fields)
        deploy_fields = list_create( LIST_AUTO_DELETE, (void (*)(void*))list_delete );
    else
        list_reset( deploy_fields );
    do {
        MapCoord *pt = 0;
        List *field_list;
        if ( player == 0 ) {
            pt = malloc( sizeof(MapCoord) );
            pt->x = (short)mx; pt->y = (short)my;
        }

        field_list = list_next( deploy_fields );
        if ( !field_list ) {
            field_list = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
            list_add( deploy_fields, field_list );
        }
        if ( pt ) list_add( field_list, pt );
    } while ( player-- );
}

/*
====================================================================
Проверить, может ли (mx, my) служить центром развертывания для данного
юнита (при условии, что он близок к нему, что не проверено). Если нет юнита
дан, проверьте, есть ли
====================================================================
*/
static int map_check_deploy_center( Player *player, Unit *unit, int mx, int my )
{
    if (map[mx][my].deploy_center!=1||!map[mx][my].player) return 0;
    if (!player_is_ally(map[mx][my].player,player)) return 0;
    if (unit)
    {
        if (terrain_get_mov(terrain_type_find( map[mx][my].terrain_id[0] ),unit->sel_prop->mov_type,cur_weather)==0)
            return 0;
        if ( unit_has_flag( unit->sel_prop, "flying" ) )
            if (!(terrain_type_find( map[mx][my].terrain_id[0] )->flags[cur_weather]&SUPPLY_AIR))
                return 0;
        if (map[mx][my].nation!=unit->nation)
            return 0;
    }
    return 1;
}

/*
====================================================================
Добавьте любой центр развертывания и его окружение к маске развертывания, если он
может поставить юнит. Если юнит не установлен, добавьте любой центр развертывания.
====================================================================
*/
static void map_add_deploy_centers_to_deploy_mask( Player *player, Unit *unit )
{
    int x, y, i, next_x, next_y;
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            if (map_check_deploy_center(player,unit,x,y))
            {
                mask[x][y].deploy = 1;
                for (i=0;i<6;i++)
                    if (get_close_hex_pos( x, y, i, &next_x, &next_y ))
                        mask[next_x][next_y].deploy = 1;
            }
}

/*
====================================================================
Добавьте начальную маску развертывания по умолчанию: все центры снабжения и их
окружение (кроме аэродромов для самолетов), все собственные части и
любой близкий гексагон, если такой гексагон не заблокирован или близко к врагу
единица, река или рядом с незапятнанной плиткой. Корабли не могут быть
развернут, поэтому вода всегда отключена. Закручивает маску развертывания!
====================================================================
*/
static MapCoord *map_create_new_coord(int x, int y)
{
    MapCoord *pt = calloc(1,sizeof(MapCoord));
    pt->x = x; pt->y = y;
    return pt;
}
static void map_add_default_deploy_fields( Player *player, List *fields )
{
    int i,j,x,y,next_x,next_y,okay;
    Unit *unit;
    list_reset(units);
    while ((unit=list_next(units)))
    {
        if (unit->player == player && unit_supports_deploy(unit))
        {
            for (i=0;i<6;i++)
                if (get_close_hex_pos(unit->x,unit->y,i,&next_x,&next_y))
                {
                    okay = 1;
                    for (j=0;j<6;j++)
                        if (get_close_hex_pos(next_x,next_y,j,&x,&y))
                            if (!mask[x][y].spot||
                                (map[x][y].a_unit&&!player_is_ally(player,map[x][y].a_unit->player))||
                                (map[x][y].g_unit&&!player_is_ally(player,map[x][y].g_unit->player)))
                            {
                                okay = 0; break;
                            }
                    if (terrain_type_find( map[next_x][next_y].terrain_id[0] )->flags[cur_weather]&RIVER) okay = 0;
                    mask[next_x][next_y].deploy = okay;
                }
        }
    }
    list_reset(units);
    while ((unit=list_next(units))) /* убедитесь, что все подразделения могут быть повторно развернуты */
        if (unit->player == player && unit_supports_deploy(unit))
            mask[unit->x][unit->y].deploy = 1;
    map_add_deploy_centers_to_deploy_mask(player,0);
    for ( x = 0; x < map_w; x++ )
        for ( y = 0; y < map_h; y++ )
            if (mask[x][y].deploy)
                list_add(fields,map_create_new_coord(x,y));
}

/*
====================================================================
Установите маску развертывания по списку полей игрока. Если первая запись (-1, -1),
создать маску по умолчанию, используя исходную схему пятен и
юнита.
====================================================================
*/
static void map_set_initial_deploy_mask( Player *player )
{
    int i = player_get_index(player);
    List *field_list;
    MapCoord *pt;
    if ( !deploy_fields ) return;
    list_reset( deploy_fields );
    while ( ( field_list = list_next( deploy_fields ) ) && i-- );
    if ( !field_list ) return;

    list_reset( field_list );
    while ( ( pt = list_next( field_list ) ) ) {
        Mask_Tile *tile = map_mask_tile(pt->x, pt->y);
        if (!tile)
        {
            list_delete_current(field_list);
            map_add_default_deploy_fields(player,field_list);
        }
        else {
            tile->deploy = 1;
            tile->spot = 1;
        }
    }
}

/*
====================================================================
Установите маску развертывания для этого устройства. Если 'init', используйте начальное развертывание
маска (или по умолчанию). Если нет, установите действующие центры развертывания. В
Во втором прогоне удалите любую плитку, заблокированную собственным юнитом, если установлено значение «unit».
====================================================================
*/
void map_get_deploy_mask( Player *player, Unit *unit, int init )
{
    int x, y;
    assert( unit || init );
    map_clear_mask( F_DEPLOY );
    if (init)
        map_set_initial_deploy_mask(player);
    else
        map_add_deploy_centers_to_deploy_mask(player, unit);
    if (unit)
    {
        for ( x = 0; x < map_w; x++ )
            for ( y = 0; y < map_h; y++ )
                if ( unit_has_flag( unit->sel_prop, "flying" ) )
                {
                    if (map[x][y].a_unit) mask[x][y].deploy = 0;
                } else {
                    if (map[x][y].g_unit&&(!init||!map_check_unit_embark(unit,x,y,EMBARK_AIR,1)))
                        mask[x][y].deploy = 0;
                }
    }
}

/*
====================================================================
Проверьте, можно ли отключить отряд. Если 'air_region' отметьте
только самолеты, иначе только наземные подразделения.
====================================================================
*/
Unit* map_get_undeploy_unit( int x, int y, int air_region )
{
    if ( air_region ) {
        /* check air */
        if ( map[x][y].a_unit &&  map[x][y].a_unit->fresh_deploy )
            return  map[x][y].a_unit;
        else
            /*if ( map[x][y].g_unit && map[x][y].g_unit->fresh_deploy )
                return  map[x][y].g_unit;
            else*/
                return 0;
    }
    else {
        /* check ground */
        if ( map[x][y].g_unit &&  map[x][y].g_unit->fresh_deploy )
            return  map[x][y].g_unit;
        else
            /*if ( map[x][y].a_unit &&  map[x][y].a_unit->fresh_deploy )
                return  map[x][y].a_unit;
            else*/
                return 0;
    }
}

/*
====================================================================
Проверьте уровень поставки плитки (mx, my) в контексте «юнит».
(шестиугольные плитки с SUPPLY_GROUND имеют 100% запас)
====================================================================
*/
int map_get_unit_supply_level( int mx, int my, Unit *unit )
{
    int x, y, w, h, i, j;
    int flag_supply_level, supply_level;
    /* летающие и плавательные юниты получают 100% запасы, если находятся рядом с аэродромом или гаванью */
    if ( unit_has_flag( unit->sel_prop, "swimming" )
         || unit_has_flag( unit->sel_prop, "flying" ) ) {
        supply_level = map_supplied_by_depot( mx, my, unit )*100;
    }
    else {
        /* наземные юниты получают 100% близость к флагу и теряют около 10% за каждое ускользающее звание */
        /* проверить все флаги в пределах области x-10, y-10,20,20 на их расстоянии */
        /* сначала получить регион */
        x = mx - 10; y = my - 10; w = 20; h = 20;
        if ( x < 0 ) { w += x; x = 0; }
        if ( y < 0 ) { h += y; y = 0; }
        if ( x + w > map_w ) w = map_w - x;
        if ( y + h > map_h ) h = map_h - y;
        /* now check flags */
        supply_level = 0;
        for ( i = x; i < x + w; i++ )
            for ( j = y; j < y + h; j++ )
                if ( map[i][j].player && player_is_ally( unit->player, map[i][j].player ) ) {
                    flag_supply_level = get_dist( mx, my, i, j );
                    if ( flag_supply_level < 2 ) flag_supply_level = 100;
                    else {
                        flag_supply_level = 100 - ( flag_supply_level - 1 ) * 10;
                        if ( flag_supply_level < 0 ) flag_supply_level = 0;
                    }
                    if ( flag_supply_level > supply_level )
                        supply_level = flag_supply_level;
                }
    }
    /* воздух: если враждебное влияние равно 1, то подача составляет 50%, если влияние> 1 подача невозможна */
    /* земля: если враждебное влияние равно 1, то запас - 75%, если влияние> 1, то запас - 50% */
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        if ( mask[ mx][ my ].air_infl > 1 || mask[ mx][ my ].infl > 1 )
            supply_level = 0;
        else
            if ( mask[ mx][my ].air_infl == 1 || mask[ mx][ my ].infl == 1 )
                supply_level = supply_level / 2;
    }
    else {
        if ( mask[ mx][ my ].infl == 1 )
            supply_level = 3 * supply_level / 4;
        else
            if ( mask[ mx][ my ].infl > 1 )
                supply_level = supply_level / 2;
    }
    return supply_level;
}

/*
====================================================================
Проверьте, является ли этот фрагмент карты точкой снабжения данного отряда.
====================================================================
*/
int map_is_allied_depot( Map_Tile *tile, Unit *unit )
{
    if ( tile == 0 ) return 0;
    /* может это авианосец */
    if ( tile->g_unit )
        if ( unit_has_flag( tile->g_unit->sel_prop, "carrier" ) )
            if ( player_is_ally( tile->g_unit->player, unit->player ) )
                if ( unit_has_flag( unit->sel_prop, "carrier_ok" ) )
                    return 1;
    /* проверить депо */
    if ( tile->player == 0 ) return 0;
    if ( !player_is_ally( unit->player, tile->player ) ) return 0;
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        if ( !(terrain_type_find( tile->terrain_id[0] )->flags[cur_weather] & SUPPLY_AIR) )
            return 0;
    }
    else
        if ( unit_has_flag( unit->sel_prop, "swimming" ) ) {
            if ( !(terrain_type_find( tile->terrain_id[0] )->flags[cur_weather] & SUPPLY_SHIPS) )
                return 0;
        }
    return 1;
}
/*
====================================================================
Проверяет, поставляется ли этот гексагон (mx, my) складом в
контекст «юнита».
====================================================================
*/
int map_supplied_by_depot( int mx, int my, Unit *unit )
{
    int i;
    if ( map_is_allied_depot( &map[mx][my], unit ) )
        return 1;
    for ( i = 0; i < 6; i++ )
        if ( map_is_allied_depot( map_get_close_hex( mx, my, i ), unit ) )
            return 1;
    return 0;
}

/*
====================================================================
Получить зону высадки для юнита (все близкие гексы, которые свободны).
====================================================================
*/
void map_get_dropzone_mask( Unit *unit )
{
    int i,x,y;
    map_clear_mask(F_DEPLOY);
    for (i=0;i<6;i++)
        if (get_close_hex_pos(unit->x,unit->y,i,&x,&y))
            if (map[x][y].g_unit==0)
                if (terrain_get_mov(terrain_type_find( map[x][y].terrain_id[0] ),unit->prop.mov_type,cur_weather)!=0)
                    mask[x][y].deploy = 1;
    if (map[unit->x][unit->y].g_unit==0)
        if (terrain_get_mov(terrain_type_find( map[unit->x][unit->y].terrain_id[0] ),unit->prop.mov_type,cur_weather)!=0)
            mask[unit->x][unit->y].deploy = 1;
}
