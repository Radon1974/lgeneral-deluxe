/***************************************************************************
                          main.c -  description
                             -------------------
    begin                : Tue Mar 12 2002
    copyright            : (C) 2001 by Michael Speck
    email                : kulkanie@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "misc.h"
#include "units.h"
#include "shp.h"
#include "nations.h"
#include "terrain.h"
#include "maps.h"
#include "scenarios.h"

const char *source_path = 0; /* исходный каталог DAT с данными PG */
const char *dest_path = 0; /* корень общей установки */
char target_name[128]; /* название кампании или отдельный сценарий */
char tacicons_name[128]; /* имя файла значков tac для одного сценария */
int single_scen = 0; /* преобразовать отдельный сценарий вместо полной кампании */
int single_scen_id = 0; /* id одного сценария, который конвертируется */
int use_def_pal = 0; /* всегда использовать палитру PG по умолчанию, независимо от того,
                        Значок SHP связан с другим */
int map_or_scen_files_missing = 0; /* некоторые файлы карты / сценария отсутствовали */
int apply_unit_mods = 0; /* применять специальные модификации PG (например, определять
                            нация по имени, зеркальное отображение, правильное написание) */
/* неофициальные варианты */
const char *axis_name = 0, *allies_name =  0;
const char *axis_nations = 0, *allies_nations = 0; /* разделенные запятой */

void print_help()
{
    printf( 
    "Usage:\n    lgc-pg options\n"\
    "Options:\n"\
    "    -s <PGDATADIR>   source directory with Panzer General data\n"\
    "    -d <LGENERALDIR> destination root directory (default: installation path)\n"\
    "    -n <TARGETNAME>  name of campaign or scenario (default: pg)\n"\
    "    -i <SCENID>      id of a single scenario to be converted; if not specified\n"\
    "                     full campaign is converted\n"\
    "    -t <TACICONS>    name of tactical icons destination file (default:\n"\
    "                     TARGETNAME.bmp)\n"\
    "    --defpal         overwrite any individual palette with default PG palette\n"\
    "    --applyunitmods  apply PG specific unit modifications for a single scenario\n"\
    "                     or custom campaign assuming it uses the PG unit database\n"\
    "                     (maybe slightly changed) \n"\
    "    --help           this help\n"\
    "Example:\n   lgc-pg -s /mnt/cdrom/DAT\n"\
    "See README.lgc-pg for more information.\n" );
    exit( 0 );
}

/* разобрать командную строку. если все параметры в порядке, верните True, иначе False */
int parse_args( int argc, char **argv )
{
    int i;
	
    for ( i = 1; i < argc; i++ ) {
        if ( !strcmp( "-s", argv[i] ) )
            source_path = argv[i + 1];
        if ( !strcmp( "-d", argv[i] ) )
            dest_path = argv[i + 1];
        if ( !strcmp( "-n", argv[i] ) )
            snprintf(target_name, 128, "%s", argv[i + 1]);
        if ( !strcmp( "-i", argv[i] ) ) {
            single_scen = 1;
            single_scen_id = atoi( argv[i + 1] );
        }
        if ( !strcmp( "-t", argv[i] ) )
            snprintf(tacicons_name, 128, "%s", argv[i + 1]);
        if ( !strcmp( "--defpal", argv[i] ) )
            use_def_pal = 1;
        if ( !strcmp( "--applyunitmods", argv[i] ) )
            apply_unit_mods = 1;
        if ( !strcmp( "-h", argv[i] ) || !strcmp( "--help", argv[i] ) )
            print_help(); /* выйдет */
        /* неофициальные варианты */
        if ( !strcmp( "--axis_name", argv[i] ) )
            axis_name = argv[i + 1];
        if ( !strcmp( "--allies_name", argv[i] ) )
            allies_name = argv[i + 1];
        if ( !strcmp( "--axis_nations", argv[i] ) )
            axis_nations = argv[i + 1];
        if ( !strcmp( "--allies_nations", argv[i] ) )
            allies_nations = argv[i + 1];
    }
    
    if ( source_path == 0 ) {
        fprintf( stderr, "ERROR: You must specifiy the source directory which "
                            "contains either a custom\nscenario or the original"
                            "data.\n" );
        return 0;
    }
    
    if ( dest_path == 0 ) {
#ifdef INSTALLDIR
        dest_path = get_gamedir(); /* использовать путь установки */
#else
        fprintf( stderr, "ERROR: You must specify the destination path which "
                            "provides the LGeneral\ndirectory struct.\n" );
        return 0;
#endif
    }
    
    if ( single_scen ) {
        if (target_name[0] == 0) {
            fprintf( stderr, "ERROR: You must specify the target name of the "
                                "custom scenario.\n" );
            return 0;
        }
    } else if (target_name[0] == 0)
        strcpy(target_name, "pg"); /* кампания по умолчанию, которая будет преобразована */
    if ( !single_scen && strcmp(target_name,"pg") == 0 )
        apply_unit_mods = 1; /* всегда для кампании PG, конечно */
    if (tacicons_name[0] == 0)
        sprintf( tacicons_name, "%s.bmp", target_name );

    printf( "Settings:\n" );
    printf( "  Source: %s\n", source_path );
    printf( "  Destination: %s\n", dest_path );
    printf( "  Target: %s\n", target_name );
    if (single_scen) {
        printf( "  Single Scenario (from game%03i.scn)\n", single_scen_id );
        if (units_find_tacicons())
            printf( "  Target Tactical Icons: %s\n", tacicons_name );
    } else
        printf("  Full Campaign\n");
    if ( use_def_pal )
        printf( "  Use Default Palette\n" );
    else
        printf( "  Use Individual Palettes\n" );
    if ( apply_unit_mods )
        printf( "  Apply PG unit modifications\n" );
    return 1;
}

int main( int argc, char **argv )
{
    char path[MAXPATHLEN];
    SDL_Surface *title_image = NULL;

    /* информация */
    printf( "LGeneral Converter for Panzer General (DOS version) v%s\n"
            "Copyright 2002-2012 Michael Speck\n"
            "Released under GNU GPL\n---\n", VERSION );
    
    /* параметры синтаксического анализа */
    if ( !parse_args( argc, argv ) ) {
        print_help();
        exit( 1 );
    }
    
    /* SDL требуется для графического преобразования */
    SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER );
    SDL_SetVideoMode( 320, 240, 16, SDL_SWSURFACE );
    atexit( SDL_Quit );
    /* показать красивое изображение */
    snprintf( path, MAXPATHLEN, "%s/convdata/title.bmp", get_gamedir() );
    if ((title_image = SDL_LoadBMP( path ))) {
	    SDL_Surface *screen = SDL_GetVideoSurface();
	    SDL_BlitSurface( title_image, NULL, screen, NULL);
	    SDL_UpdateRect( screen, 0, 0, screen->w, screen->h );
    }
    SDL_WM_SetCaption( "lgc-pg", NULL );
    
    printf( "Converting:\n" );
    if ( single_scen ) {
        if ( !scenarios_convert( single_scen_id ) )
            return 1;
        if ( !maps_convert( single_scen_id ) )
            return 1;
        if ( units_find_panzequp() )  {
            if ( units_find_tacicons() ) {
                if ( !units_convert_graphics( tacicons_name ) )
                    return 1;
                if ( !units_convert_database( tacicons_name ) )
                    return 1;
            } else if ( !units_convert_database( "pg.bmp" ) ) 
                return 1;
        }
        printf( "\nNOTE: You must set various things manually (e.g., victory "
                "conditions). Please\nsee README.lgc-pg for more details.\n" );
    } else {
        /* конвертировать все данные */
        if ( !nations_convert() )
            return 1;
        if ( !units_convert_database( tacicons_name ) )
            return 1;
        if ( !units_convert_graphics( tacicons_name ) )
            return 1;
        if ( !terrain_convert_database() )
            return 1;
        if ( !terrain_convert_graphics() )
            return 1;
        if ( !maps_convert( -1 ) )
            return 1;
        if ( !scenarios_convert( -1 ) )
            return 1;
        /* для неофициальных кампаний нет условий победы и кампании
         * дерево неизвестно; преобразованы только базы данных и сценарии */
        if (strcmp(target_name, "pg"))
            printf( "\nNOTE: You must set various things manually (e.g., victory "
                    "conditions). Please\nsee README.lgc-pg for more details.\n" );
        if (map_or_scen_files_missing)
            printf("\nWARNING: Some scenario or map files were missing! As this is a custom campaign\n"
                    "it may be that not all scenarios are used. In that case ids of missing map and\n"
                    "scenario files should match. If they don't this might be an error.\n");
    }
    printf( "\nDone!\n" );
    if (title_image)
	    SDL_FreeSurface(title_image);
    return 0;
}
