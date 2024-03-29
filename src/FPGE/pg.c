/*
 * pg.c
 *
 *  Created on: 2011-09-04
 *      Author: wino
 */
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

#include <stdio.h>
#include "fpge.h"
#include "pg.h"
#include "pgf.h"
#include "terrain.h"
#include "map.h"
#include "localize.h"
#include "sdl.h"
#include "misc.h"
#include "file.h"
#include "lgeneral.h"

extern Sdl sdl;
extern Font *log_font;
extern Config config;
extern int map_w, map_h;
extern Map_Tile **map;
extern Mask_Tile **mask;
extern int log_x, log_y;       /* положение, в котором следует отображать информацию журнала */
extern Terrain_Type *terrain_types;
extern int terrain_type_count;
extern int hex_w, hex_h, terrain_columns;

char stm_fname[16];

char tile_type[] = {
    'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 
    'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'c',
    'c', 'o', 'o', 'o', 'h', 'h', 'h', 'h', 'c', 'o', 'c',
    'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'o', 'c',
    'c', 'c', 'c', 'c', 'c', 'c', 'o', 'o', 'R', 'R', 'R',
    'R', 'c', 'c', 'c', 'c', 'c', 'R', 'R', 'R', 'R', 'R',
    'c', 'c', 'c', 'c', 'c', 'R', 'R', 'o', 'c', 'm', 'm',
    'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 
    'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 
    'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 
    'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 
    'm', 'm', 'm', 's', 't', 't', 't', 'a', 'c', 'c', '~',
    '~', 's', 's', 'f', 'f', 'f', 'f', 'c', '~', '~', '~',
    '~', 's', 'f', 'f', 'f', 'c', 'c', 'c', 'F', 'F', 'F',
    'c', 'c', 'c', '@', 'F', 'F', 'F', 'F', 'R', 'F', 'c',
    'R', 'c', '@', 'F', 'F', 'F', 'F', 'F', 'F', 'c', 'c',
    'c', '@', 'F', 'F', 'F', 'm', 'm', 'd', 'm', 'm', 'd',
    'm', 'm', 'm', 'd', 'm', 'm', 'm', 'd', 'd', 'm', 'm',
    'm', 'm', 'D', 'D', 'D', 'D', 'D', 't', 'F', 
    /* ??? -- действительно горы ??? */
    'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm',
    'm', 'm', 'm', 'm', 'm', 'm',
    /* ??? */
    'c', 'c', 'c', 'c', 'R', 'R', 'R', 'c', 'h', 'h', 'd',
    'd', 'd', 'd'
};

/*
====================================================================
Прочитать имя фрагмента карты с этим идентификатором в buf.
====================================================================
*/
static void tile_get_name( FILE *file, int id, char *buf )
{
    fseek( file, 2 + id * 20, SEEK_SET );
    if ( feof( file ) ) 
        sprintf( buf, "none" );
    else
{
        fread( buf, 20, 1, file );
//fprintf(stderr, "%s\n", buf);
}
}

int parse_set_file(FILE *inf, int offset)
{
    FILE *name_file;
    char path[MAX_PATH];
    int x,y,c=0,i;
    /* информация журнала */
    char log_str[128], name_buf[24];
 
    //получить размер карты
    fseek(inf, offset+MAP_X_ADDR, SEEK_SET);
    fread(&map_w, 2, 1, inf);
    fseek(inf, offset+MAP_Y_ADDR, SEEK_SET);
    fread(&map_h, 2, 1, inf);
    ++map_w;
    ++map_h; //PG устанавливает это значение на единицу меньше размера, т.е. последнего индекса.
    if ( !terrain_load( "terrain.lgdtdb" ) )
    {
        map_delete();
        return 0;
    }
    search_file_name_exact( path, "mapnames.str", config.mod_name, "Scenario" );
    if ( (name_file = fopen( path, "r" )) == NULL )
    {
        return 0;
    }
    /* выделить память карты */
    map = calloc( map_w, sizeof( Map_Tile* ) );
    for ( i = 0; i < map_w; i++ )
        map[i] = calloc( map_h, sizeof( Map_Tile ) );
    mask = calloc( map_w, sizeof( Mask_Tile* ) );
    for ( i = 0; i < map_w; i++ )
        mask[i] = calloc( map_h, sizeof( Mask_Tile ) );
    //get STM
    fseek(inf, offset+MAP_STM, SEEK_SET);
    fread(stm_fname, 14, 1, inf);
    stm_fname[14]=0;
    //stm_number=atoi(stm);
    //printf("nn=%d\n",nn);
    //get the tiles numbers
    sprintf( log_str, tr("Loading Flags") );
    write_line( sdl.screen, log_font, log_str, log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
    fseek(inf, offset+MAP_LAYERS_START + 5 * map_w * map_h, SEEK_SET);
    for (y = 0; y < map_h; ++y)
        for (x = 0; x < map_w; ++x)
        {
            /* по умолчанию нет флага */
            map[x][y].nation = 0;
            map[x][y].player = 0;
            map[x][y].deploy_center = 0;
            /* по умолчанию не задана цель в милах */
            map[x][y].obj = 0;
            c+=fread(&(map[x][y].terrain_id[0]), 2, 1, inf);
            /* проверить идентификатор изображения — установить смещение */
            map[x][y].image_offset_x = ( map[x][y].terrain_id[0] ) % terrain_columns * hex_w;
            map[x][y].image_offset_y = ( map[x][y].terrain_id[0] ) / terrain_columns * hex_h;
            map[x][y].terrain_id[0] = tile_type[map[x][y].terrain_id[0]];
            /* очистить юниты на этой плитке */
            map[x][y].g_unit = 0;
            map[x][y].a_unit = 0;
        }
    if (c!=map_w*map_h)
        fprintf(stderr, "WARNING: SET file too short !\n");
    //получить информацию о стране (флаге)
    fseek(inf, offset+MAP_LAYERS_START + 3 * map_w * map_h, SEEK_SET);
    for (y = 0; y < map_h; ++y)
    {
        for (x = 0; x < map_w; ++x){
            c = 0;
            fread(&c, 1, 1, inf);
            map[x][y].obj = 0;
            map[x][y].nation = nation_find_by_id( c - 1 );
            if (map[x][y].nation != 0)
            {
//                fprintf(stderr, "%s ", map[x][y].nation->name);
                map[x][y].player = player_get_by_nation( map[x][y].nation );
//                fprintf(stderr, "%s ", map[x][y].player->name);
                if ( map[x][y].nation )
                    map[x][y].deploy_center = 1;
            }
        }
//        fprintf(stderr, "\n");
    }
    //получить имена
    fseek(inf, offset+MAP_LAYERS_START + 0 * map_w * map_h, SEEK_SET);

    for (y = 0; y < map_h; ++y)
        for (x = 0; x < map_w; ++x)
        {
            i = 0;
            fread( &i, 2, 1, inf );
            i = SDL_SwapLE16( i );
            tile_get_name( name_file, i, name_buf );
            map[x][y].name = strdup( name_buf );
        }

    //получить дорожное определение
    fseek(inf,offset+MAP_LAYERS_START + 2 * map_w * map_h, SEEK_SET);
    for (y = 0; y < map_h; ++y)
        for (x = 0; x < map_w; ++x){
            map[x][y].terrain_id[1] = 'r';
            fread(&(map[x][y].layers[1]), 1, 1, inf);
            if ( map[x][y].terrain_id[0] == 'a' || map[x][y].terrain_id[0] == 't' || map[x][y].terrain_id[0] == 'h' )
                map[x][y].layers[1] = 255;
            if (map[x][y].layers[0]&(~0xBB)) printf("WARNING: Wrong road at %d,%d !\n",x,y);

        }
    return 1;
}

// читать файлы SET и STM
int load_map_pg(char *fullName) {
    FILE *inf;
    int error=0;

    inf = fopen(fullName, "rb");
    if (!inf)
        return 0;

    error=parse_set_file(inf,0);
    fclose(inf);

    if (error)
        return error;

    return 1;
}
