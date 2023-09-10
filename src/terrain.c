/***************************************************************************
                          terrain.c  -  description
                             -------------------
    begin                : Wed Mar 17 2002
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
#include "parser.h"
#include "unit_lib.h"
#include "terrain.h"
#include "localize.h"
#include "file.h"
#include "FPGE/pgf.h"

/*
====================================================================
Внешние
====================================================================
*/
extern Sdl sdl;
extern Mov_Type *mov_types;
extern int mov_type_count;
extern int log_x, log_y;
extern Font *log_font;
extern Config config;

/*
====================================================================
Таблица преобразования флагов для местности
====================================================================
*/
StrToFlag fct_terrain[] = {
    { "supply_air", SUPPLY_AIR },               
    { "supply_ground", SUPPLY_GROUND },         
    { "supply_ships", SUPPLY_SHIPS },           
    { "river", RIVER },
    { "no_spotting", NO_SPOTTING },             
    { "inf_close_def", INF_CLOSE_DEF },         
    { "swamp", SWAMP },
    { "desert", DESERT },
    { "X", 0}    
};

/*
====================================================================
Таблица преобразования флагов для погоды
====================================================================
*/
StrToFlag fct_weather[] = {
    { "no_air_attack", NO_AIR_ATTACK },         
    { "double_fuel_cost", DOUBLE_FUEL_COST },   
    { "cut_strength", CUT_STRENGTH },
    { "bad_sight", BAD_SIGHT },
    { "X", 0}    
};

/*
====================================================================
Геометрия шестигранной плитки
====================================================================
*/
int hex_w, hex_h;
int hex_x_offset, hex_y_offset;
int terrain_columns;         /* количество столбцов в файле изображения местности */
int terrain_rows;            /* количество строк в файле изображения местности */

/*
====================================================================
Типы местности и компания
====================================================================
*/
Terrain_Type *terrain_types = 0;
int terrain_type_count = 0;
Weather_Type *weather_types = 0;
int weather_type_count = 0;
Terrain_Icons *terrain_icons = 0;
Terrain_Images *terrain_images = 0;
int ground_conditions_count = 0;

/*
====================================================================
Получить идентификатор (номер) типа местности
====================================================================
*/
int terrain_type_get_index( char terrain_type )
{
    int i;
    for ( i = 0; i < terrain_type_count; i++ )
        if ( terrain_types[i].id == terrain_type )
            return i;
    return -1;
}

/*
====================================================================
Получить идентификатор (номер) типа погоды
====================================================================
*/
int weather_type_get_index( char *weather_type )
{
    int i;
    for ( i = 0; i < weather_type_count; i++ )
        if ( strcmp( weather_types[i].id, weather_type ) == 0 )
            return i;
    return -1;
}

/*
====================================================================
Загружайте типы местности, информацию о погоде и значки шестнадцатеричных плиток в формате tdb.
====================================================================
*/
int terrain_load_tdb( char *fname, char *path )
{
    int i, j, k;
    PData *pd, *sub, *subsub, *subsubsub;
    List *entries, *flags;
    char *flag, *str;
    char *domain = 0;
    int count;
    /* информация журнала */
    int  log_dot_limit = 40; /* максимум точек */
    char log_str[128];
    if ( ( pd = parser_read_file( fname, path ) ) == 0 ) goto parser_failure;
//    domain = determine_domain(pd, fname);
//    locale_load_domain(domain, 0/*FIXME*/);
    /* получить погоду */
    if ( !parser_get_entries( pd, "weather", &entries ) ) goto parser_failure;
    weather_type_count = entries->count;
    weather_types = calloc( weather_type_count, sizeof( Weather_Type ) );
    list_reset( entries ); i = 0;
    while ( ( sub = list_next( entries ) ) ) {
        weather_types[i].id = strdup( sub->name );
//        if ( !parser_get_localized_string( sub, "name", domain, &weather_types[i].name ) ) goto parser_failure;
        if ( !parser_get_value( sub, "name", &str, 0 ) ) goto parser_failure;
        weather_types[i].name = strdup( str );
        if ( !parser_get_value( sub, "ground_cond", &str, 0 ) ) goto parser_failure;
        weather_types[i].ground_conditions = strdup( str );
        if ( !parser_get_values( sub, "flags", &flags ) ) goto parser_failure;
        list_reset( flags );
        while ( ( flag = list_next( flags ) ) ) {
            int NumberInArray;
            weather_types[i].flags |= check_flag( flag, fct_weather, &NumberInArray );
        }
        i++;
    }
    /* шестигранная геометрия плитки */
    if ( !parser_get_int( pd, "hex_width", &hex_w ) ) goto parser_failure;
    if ( !parser_get_int( pd, "hex_height", &hex_h ) ) goto parser_failure;
    if ( !parser_get_int( pd, "hex_x_offset", &hex_x_offset ) ) goto parser_failure;
    if ( !parser_get_int( pd, "hex_y_offset", &hex_y_offset ) ) goto parser_failure;
    /* значки ландшафта */
    terrain_icons = calloc( 1, sizeof( Terrain_Icons ) );
    if ( !parser_get_value( pd, "fog", &str, 0 ) ) goto parser_failure;
    search_file_name_exact( path, str, config.mod_name, "Graphics" );
    if ( ( terrain_icons->fog = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
    if ( !parser_get_value( pd, "danger", &str, 0 ) ) goto parser_failure;
    search_file_name_exact( path, str, config.mod_name, "Graphics" );
    if ( ( terrain_icons->danger = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
    if ( !parser_get_value( pd, "grid", &str, 0 ) ) goto parser_failure;
    search_file_name_exact( path, str, config.mod_name, "Graphics" );
    if ( ( terrain_icons->grid = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
    if ( !parser_get_value( pd, "frame", &str, 0 ) ) goto parser_failure;
    search_file_name_exact( path, str, config.mod_name, "Graphics" );
    if ( ( terrain_icons->select = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
    if ( !parser_get_value( pd, "crosshair", &str, 0 ) ) goto parser_failure;
    search_file_name_exact( path, str, config.mod_name, "Graphics" );
    if ( ( terrain_icons->cross = anim_create( load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 1000/config.anim_speed, hex_w, hex_h, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    anim_hide( terrain_icons->cross, 1 );
    if ( !parser_get_value( pd, "explosion", &str, 0 ) ) goto parser_failure;
    search_file_name_exact( path, str, config.mod_name, "Graphics" );
    if ( ( terrain_icons->expl1 = anim_create( load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 50/config.anim_speed, hex_w, hex_h, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    anim_hide( terrain_icons->expl1, 1 );
    if ( ( terrain_icons->expl2 = anim_create( load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 50/config.anim_speed, hex_w, hex_h, sdl.screen, 0, 0 ) ) == 0 )
        goto failure;
    anim_hide( terrain_icons->expl2, 1 );
    /* звуки местности */
#ifdef WITH_SOUND    
    if ( parser_get_value( pd, "explosion_sound", &str, 0 ) )
    {
        search_file_name_exact( path, str, config.mod_name, "Sound" );
        terrain_icons->wav_expl = wav_load( path, 2 );
    }
    if ( parser_get_value( pd, "select_sound", &str, 0 ) )
    {
        search_file_name_exact( path, str, config.mod_name, "Sound" );
        terrain_icons->wav_select = wav_load( path, 1 );
    }
#endif
    /* столбцы изображения данных ландшафта */
    if ( !parser_get_int( pd, "terrain_columns", &terrain_columns ) ) goto parser_failure;
    /* строки изображения данных ландшафта */
    if ( !parser_get_int( pd, "terrain_rows", &terrain_rows ) ) goto parser_failure;
    /* у каждого типа условий грунта есть свое собственное изображение - если он назван "по умолчанию", мы
       укажите на изображение weather_type 0 */
    terrain_images = calloc( 1, sizeof( Terrain_Images ) );
    terrain_images->ground_conditions = calloc( weather_type_count / 4, sizeof( char* ) );
    terrain_images->images = calloc( weather_type_count / 4, sizeof( SDL_Surface* ) );
    for ( j = 0; j < weather_type_count / 4; j++ ) {
        terrain_images->ground_conditions[j] = strdup( weather_types[4*j].ground_conditions );
        sprintf( path, "image/%s", weather_types[4*j].ground_conditions );
        if ( !parser_get_value( pd, path, &str, 0 ) ) goto parser_failure;
        if ( STRCMP( "default", str ) && j > 0 ) {
            /* просто указатель */
            terrain_images->images[j] = terrain_images->images[0];
        }
        else {
            search_file_name_exact( path, str, config.mod_name, "Graphics" );
            if ( ( terrain_images->images[j] = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto parser_failure;
            SDL_SetColorKey( terrain_images->images[j], SDL_SRCCOLORKEY, 
                             get_pixel( terrain_images->images[j], 0, 0 ) );
        }
    }
    /* изображение тумана */
    terrain_images->images_fogged = calloc( weather_type_count / 4, sizeof( SDL_Surface* ) );
    for ( j = 0; j < weather_type_count / 4; j++ ) {
        if ( terrain_images->images[j] == terrain_images->images[0] && j > 0 ) {
            /* просто указатель */
            terrain_images->images_fogged[j] = terrain_images->images_fogged[0];
        }
        else {
            terrain_images->images_fogged[j] = create_surf( terrain_images->images[j]->w, terrain_images->images[j]->h, SDL_SWSURFACE );
            FULL_DEST( terrain_images->images_fogged[j]  );
            FULL_SOURCE( terrain_images->images[j] );
            blit_surf();
            count =  terrain_images->images[j]->w /  hex_w;
            for ( i = 0; i < terrain_rows; i++ )
                for ( k = 0; k < terrain_columns; k++ ) {
                    DEST( terrain_images->images_fogged[j], k * hex_w, i * hex_h,  hex_w, hex_h );
                    SOURCE( terrain_icons->fog, 0, 0 );
                    alpha_blit_surf( FOG_ALPHA );
                }
            SDL_SetColorKey( terrain_images->images_fogged[j], SDL_SRCCOLORKEY, get_pixel( terrain_images->images_fogged[j], 0, 0 ) );
        }
    }
    /* типы местности */
    if ( !parser_get_entries( pd, "terrain", &entries ) ) goto parser_failure;
    terrain_type_count = entries->count;
    terrain_types = calloc( terrain_type_count, sizeof( Terrain_Type ) );
    list_reset( entries ); i = 0;
    while ( ( sub = list_next( entries ) ) ) {
        /* id */
        terrain_types[i].id = sub->name[0];
        /* имя */
        if ( !parser_get_localized_string( sub, "name", domain, &terrain_types[i].name ) ) goto parser_failure;
        /* стоимость места */
        terrain_types[i].spt = calloc( weather_type_count, sizeof( int ) );
        if ( !parser_get_pdata( sub, "spot_cost",  &subsub ) ) goto parser_failure;
        for ( j = 0; j < weather_type_count; j++ )
            if ( !parser_get_int( subsub, weather_types[j].id, &terrain_types[i].spt[j] ) ) goto parser_failure;
        /* изображение */
        terrain_types[i].images = terrain_images->images;
        /* изображение тумана */
        terrain_types[i].images_fogged = terrain_images->images_fogged;
        /* стоимость переезда */
        terrain_types[i].mov = calloc( mov_type_count * weather_type_count, sizeof( int ) );
        if ( !parser_get_pdata( sub, "move_cost",  &subsub ) ) goto parser_failure;
        for ( k = 0; k < mov_type_count; k++ ) {
            if ( !parser_get_pdata( subsub, mov_types[k].id,  &subsubsub ) ) goto parser_failure;
            for ( j = 0; j < weather_type_count; j++ ) {
                if ( !parser_get_value( subsubsub, weather_types[j].id, &str, 0 ) ) goto parser_failure;
                if ( str[0] == 'X' )
                    terrain_types[i].mov[j + k * weather_type_count] = 0; /* impassable */
                else
                    if ( str[0] == 'A' )
                        terrain_types[i].mov[j + k * weather_type_count] = -1; /* costs all */
                    else
                        terrain_types[i].mov[j + k * weather_type_count] = atoi( str ); /* normal cost */
            }
        }
        /* окоп */
        if ( !parser_get_int( sub, "min_entr",  &terrain_types[i].min_entr ) ) goto parser_failure;
        if ( !parser_get_int( sub, "max_entr",  &terrain_types[i].max_entr ) ) goto parser_failure;
        /* изменение инициативы */
        if ( !parser_get_int( sub, "max_init",  &terrain_types[i].max_ini ) ) goto parser_failure;
        /* флаги */
        terrain_types[i].flags = calloc( weather_type_count, sizeof( int ) );
        if ( !parser_get_pdata( sub, "flags",  &subsub ) ) goto parser_failure;
        for ( j = 0; j < weather_type_count; j++ ) {
            if ( !parser_get_values( subsub, weather_types[j].id, &flags ) ) goto parser_failure;
            list_reset( flags );
            while ( ( flag = list_next( flags ) ) ) {
                int NumberInArray;
                terrain_types[i].flags[j] |= check_flag( flag, fct_terrain, &NumberInArray );
            }
        }
        /* следующая местность */
        i++;
        /* ЖУРНАЛ */
        strcpy( log_str, "  [                                        ]" );
        for ( k = 0; k < i * log_dot_limit / entries->count; k++ )
            log_str[3 + k] = '*';
        write_text( log_font, sdl.screen, log_x, log_y, log_str, 255 );
        SDL_UpdateRect( sdl.screen, log_font->last_x, log_font->last_y, log_font->last_width, log_font->last_height );
    }
    parser_free( &pd );
    /* ЖУРНАЛ */
    write_line( sdl.screen, log_font, log_str, log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
    free(domain);
    return 1;
parser_failure:        
    fprintf( stderr, "%s\n", parser_get_error() );
failure:
    terrain_delete();
    if ( pd ) parser_free( &pd );
    free(domain);
    return 0;
}

/*
====================================================================
Загрузите типы местности, информацию о погоде и значки шестигранных плиток в формате lgdtdb.
====================================================================
*/
int terrain_load_lgdtdb( char *fname, char *path )
{
    int i;
    char *domain = 0;

    FILE *inf;
    char line[1024], tokens[100][256], log_str[128];
    int j, k, block=0, last_line_length=-1, cursor=0, token=0;
    int utf16 = 0, lines=0, cur_ground_condition = 0, cur_weather_type = 0, cur_terrain_type = 0, count;
    weather_type_count = 0;
    ground_conditions_count = 0;

    inf=fopen(path,"rb");
    if (!inf)
    {
        fprintf( stderr, "Couldn't open terrain database file\n");
        return 0;
    }

    //найти количество типов погоды, типов движения и классов юнитов
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
            ground_conditions_count++;
        if (block == 3 && strlen(line)>0)
            weather_type_count++;
        if (block == 4 && strlen(line)>0)
            terrain_type_count++;
        if (block == 5)
        {
            fclose(inf);
            break;
        }
    }

    weather_types = calloc( weather_type_count, sizeof( Weather_Type ) );
    terrain_icons = calloc( 1, sizeof( Terrain_Icons ) );
    terrain_images = calloc( 1, sizeof( Terrain_Images ) );
    terrain_images->images = calloc( ground_conditions_count, sizeof( SDL_Surface* ) );
    terrain_images->images_fogged = calloc( ground_conditions_count, sizeof( SDL_Surface* ) );
    terrain_types = calloc( terrain_type_count, sizeof( Terrain_Type ) );
    terrain_images->ground_conditions = calloc( ground_conditions_count, sizeof( char* ) );

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

        //Block#1 основные данные о местности
        if (block == 1 && strlen(line)>0)
        {
/*            if ( strcmp( tokens[0], "domain" ) == 0 )
            {
                domain = determine_domain(tokens[1], fname);
            }*/

            /* шестигранная геометрия плитки */
            if ( strcmp( tokens[0], "hex_width" ) == 0 )
            {
                hex_w = atoi( tokens[1] );
            }
            if ( strcmp( tokens[0], "hex_height" ) == 0 )
            {
                hex_h = atoi( tokens[1] );
            }
            if ( strcmp( tokens[0], "hex_x_offset" ) == 0 )
            {
                hex_x_offset = atoi( tokens[1] );
            }
            if ( strcmp( tokens[0], "hex_y_offset" ) == 0 )
            {
                hex_y_offset = atoi( tokens[1] );
            }

            /* значки ландшафта */
            if ( strcmp( tokens[0], "fog" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                if ( ( terrain_icons->fog = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
            }
            if ( strcmp( tokens[0], "danger" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                if ( ( terrain_icons->danger = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
            }
            if ( strcmp( tokens[0], "grid" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                if ( ( terrain_icons->grid = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
            }
            if ( strcmp( tokens[0], "frame" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                if ( ( terrain_icons->select = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
            }
            if ( strcmp( tokens[0], "crosshair" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                if ( ( terrain_icons->cross = anim_create( load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 1000/config.anim_speed, hex_w, hex_h, sdl.screen, 0, 0 ) ) == 0 )
                    goto failure;
                anim_hide( terrain_icons->cross, 1 );
            }
            if ( strcmp( tokens[0], "explosion" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                if ( ( terrain_icons->expl1 = anim_create( load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 50/config.anim_speed, hex_w, hex_h, sdl.screen, 0, 0 ) ) == 0 )
                    goto failure;
                anim_hide( terrain_icons->expl1, 1 );
                if ( ( terrain_icons->expl2 = anim_create( load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 50/config.anim_speed, hex_w, hex_h, sdl.screen, 0, 0 ) ) == 0 )
                    goto failure;
                anim_hide( terrain_icons->expl2, 1 );
            }

            /* звуки местности */
#ifdef WITH_SOUND    
            if ( strcmp( tokens[0], "explosion_sound" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Sound" );
                terrain_icons->wav_expl = wav_load( path, 2 );
            }
            if ( strcmp( tokens[0], "select_sound" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Sound" );
                terrain_icons->wav_select = wav_load( path, 1 );
            }
#endif

        }

        //Block#2 данные о грунтовых условиях
        if (block == 2 && strlen(line)>0)
        {
            terrain_images->ground_conditions[cur_ground_condition] = strdup( tokens[0] );
            sprintf( path, "image/%s", tokens[0] );
            if ( STRCMP( "default", tokens[1] ) && j > 0 ) {
                /* просто указатель */
                terrain_images->images[cur_ground_condition] = terrain_images->images[0];
            }
            else {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                if ( ( terrain_images->images[cur_ground_condition] = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
                SDL_SetColorKey( terrain_images->images[cur_ground_condition], SDL_SRCCOLORKEY, 
                                 get_pixel( terrain_images->images[cur_ground_condition], 0, 0 ) );
            }

            /* каждому типу грунтовых условий присвоено собственное изображение */
            if ( terrain_columns == 0 )
            {
                terrain_columns = terrain_images->images[0]->w / hex_w;
            }
            if ( terrain_rows == 0 )
            {
                terrain_rows = terrain_images->images[0]->h / hex_h;
            }

            /* изображение тумана */
            if ( terrain_images->images[cur_ground_condition] == terrain_images->images[0] && cur_ground_condition > 0 ) {
                /* просто указатель */
                terrain_images->images_fogged[cur_ground_condition] = terrain_images->images_fogged[0];
            }
            else {
                terrain_images->images_fogged[cur_ground_condition] = create_surf( terrain_images->images[cur_ground_condition]->w, terrain_images->images[cur_ground_condition]->h, SDL_SWSURFACE );
                FULL_DEST( terrain_images->images_fogged[cur_ground_condition]  );
                FULL_SOURCE( terrain_images->images[cur_ground_condition] );
                blit_surf();
                count = terrain_images->images[cur_ground_condition]->w / hex_w;
                for ( j = 0; j < terrain_rows; j++ )
                    for ( k = 0; k < terrain_columns; k++ ) {
                        DEST( terrain_images->images_fogged[cur_ground_condition], k * hex_w, j * hex_h, hex_w, hex_h );
                        SOURCE( terrain_icons->fog, 0, 0 );
                        alpha_blit_surf( FOG_ALPHA );
                    }
                SDL_SetColorKey( terrain_images->images_fogged[cur_ground_condition], SDL_SRCCOLORKEY, get_pixel( terrain_images->images_fogged[cur_ground_condition], 0, 0 ) );
            }
            cur_ground_condition++;
        }

        //Block#3 типы погоды
        if (block == 3 && strlen(line)>0)
        {
            weather_types[cur_weather_type].id = strdup( tokens[0] );
            weather_types[cur_weather_type].name = strdup( tokens[1] );
            weather_types[cur_weather_type].ground_conditions = strdup( tokens[2] );
            for ( j = 3; j < token; j++ )
            {
                if ( atoi( tokens[j] ) == 1 )
                {
                    int NumberInArray, Flag;
                    Flag = check_flag( fct_weather[j - 3].string, fct_weather, &NumberInArray );
                    weather_types[cur_weather_type].flags |= Flag;
                }
            }
            cur_weather_type++;
        }

        //Block#4 типы местности
        if (block == 4 && strlen(line)>0)
        {
            /* id */
            terrain_types[cur_terrain_type].id = tokens[0][0];
            /* имя */
            terrain_types[cur_terrain_type].name = strdup( tokens[1] );
            /* image */
            terrain_types[cur_terrain_type].images = terrain_images->images;
            /* изображение тумана */
            terrain_types[cur_terrain_type].images_fogged = terrain_images->images_fogged;
            cur_terrain_type++;
        }

        //Block#5 spot costs
        if (block == 5 && strlen(line)>0)
        {
            /* spot cost */
            terrain_types[terrain_type_get_index( tokens[0][0] )].spt = calloc( weather_type_count, sizeof( int ) );
            for ( j = 1; j < token; j++ )
                terrain_types[terrain_type_get_index( tokens[0][0] )].spt[j - 1] = atoi( tokens[j] );
        }

        //Block#6 флаги местности
        if (block == 6 && strlen(line)>0)
        {
            /* флаги */
            if (terrain_types[terrain_type_get_index( tokens[0][0] )].flags == 0)
                terrain_types[terrain_type_get_index( tokens[0][0] )].flags = calloc( weather_type_count, sizeof( int ) );
            for ( j = 2; j < token; j++ ) {
                if ( atoi( tokens[j] ) == 1 )
                {
                    int NumberInArray, Flag;
                    Flag = check_flag( fct_terrain[j - 2].string, fct_terrain, &NumberInArray );
                    terrain_types[terrain_type_get_index( tokens[0][0] )].flags[weather_type_get_index( tokens[1] )] |= Flag;
                }
            }
        }

        //Block#7 затраты на перемещение
        if (block == 7 && strlen(line)>0)
        {
            if (terrain_types[terrain_type_get_index( tokens[0][0] )].mov == 0)
                terrain_types[terrain_type_get_index( tokens[0][0] )].mov = calloc( mov_type_count * weather_type_count, sizeof( int ) );
            for ( j = 2; j < token; j++ ) {
                if ( tokens[j][0] == 'X' )
                    terrain_types[terrain_type_get_index( tokens[0][0] )].mov[j - 2 + movement_type_get_index( tokens[1] ) * weather_type_count] = 0; /* непроходимый */
                else
                    if ( tokens[j][0] == 'A' )
                        terrain_types[terrain_type_get_index( tokens[0][0] )].mov[j - 2 + movement_type_get_index( tokens[1] ) * weather_type_count] = -1; /* стоит все */
                    else
                        terrain_types[terrain_type_get_index( tokens[0][0] )].mov[j - 2 + movement_type_get_index( tokens[1] ) * weather_type_count] = atoi( tokens[j] ); /* нормальная стоимость */
            }
        }
    }    
    fclose(inf);
    /* ЖУРНАЛ */
    write_line( sdl.screen, log_font, log_str, log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
    free(domain);
    return 1;
failure:
    terrain_delete();
    free(domain);
    return 0;
}

/*
====================================================================
Загрузите типы местности, информацию о погоде и значки шестигранных плиток.
====================================================================
*/
int terrain_load( char *fname )
{
    char *path, *extension;
    path = calloc( 256, sizeof( char ) );
    extension = calloc( 10, sizeof( char ) );
    search_file_name_exact( path, fname, config.mod_name, "Scenario" );
    extension = strrchr(fname,'.');
    extension++;
    if ( strcmp( extension, "tdb" ) == 0 )
        return terrain_load_tdb( fname, path );
    else if ( strcmp( extension, "lgdtdb" ) == 0 )
        return terrain_load_lgdtdb( fname, path );
    return 0;
}

/*
====================================================================
Удалить типы местности и другие
====================================================================
*/
void terrain_delete( void )
{
    int i, j;
    /* типы местности */
    if ( terrain_images ) {
        for ( j = ( weather_type_count / 4 ) - 1; j >= 0; j-- ) 
            if ( terrain_images->images[j] ) {
                if ( terrain_images->images[j] == terrain_images->images[0] )
                    if ( j > 0 )
                        continue; /* только указатель */
                SDL_FreeSurface( terrain_images->images[j] );
            }
        free( terrain_images ); terrain_images = 0;
    }
    if ( terrain_types ) {
        for ( i = 0; i < terrain_type_count; i++ ) {
            if ( terrain_types[i].flags ) free( terrain_types[i].flags );
            if ( terrain_types[i].mov ) free( terrain_types[i].mov );
            if ( terrain_types[i].spt ) free( terrain_types[i].spt );
            if ( terrain_types[i].name ) free( terrain_types[i].name );
        }
        free( terrain_types ); terrain_types = 0;
        terrain_type_count = 0;
    }
    /* Погода */
    if ( weather_types ) {
        for ( i = 0; i < weather_type_count; i++ ) {
            if ( weather_types[i].id ) free( weather_types[i].id );
            if ( weather_types[i].name ) free( weather_types[i].name );
        }
        free( weather_types ); weather_types = 0;
        weather_type_count = 0;
    }
    /* значки ландшафта */
    if ( terrain_icons ) {
        if ( terrain_icons->fog ) SDL_FreeSurface( terrain_icons->fog );
        if ( terrain_icons->grid ) SDL_FreeSurface( terrain_icons->grid );
        if ( terrain_icons->danger ) SDL_FreeSurface( terrain_icons->danger );
        if ( terrain_icons->select ) SDL_FreeSurface( terrain_icons->select );
        anim_delete( &terrain_icons->cross );
        anim_delete( &terrain_icons->expl1 );
        anim_delete( &terrain_icons->expl2 );
#ifdef WITH_SOUND
        if ( terrain_icons->wav_expl ) wav_free( terrain_icons->wav_expl );
        if ( terrain_icons->wav_select ) wav_free( terrain_icons->wav_select );
#endif
        free( terrain_icons ); terrain_icons = 0;
    }
}

/*
====================================================================
Получите стоимость движения для типа местности, передав движение
введите идентификатор и идентификатор погоды в дополнение.
Верните -1, если все очки движения израсходованы
Вернуть 0, если движение невозможно
Стоимость возврата остальное.
====================================================================
*/
int terrain_get_mov( Terrain_Type *type, int mov_type, int weather )
{
    return type->mov[mov_type * weather_type_count + weather];
}

/*
====================================================================
Получить идентификатор (номер) погодных условий почвы
====================================================================
*/
int ground_conditions_get_index( char *ground_condition )
{
    int i;
    for ( i = 0; i < weather_type_count / 4; i++ )
        if ( strcmp( ground_condition, weather_types[i * 4].ground_conditions ) == 0 )
            return i;
    return -1;
}

/*
====================================================================
Найти тип местности по id
====================================================================
*/
Terrain_Type *terrain_type_find( char terrain_type )
{
    int i;
    for ( i = 0; i < terrain_type_count; i++ )
        if ( terrain_types[i].id == terrain_type )
            return &terrain_types[i];
    return 0;
}

