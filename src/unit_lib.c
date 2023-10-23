/***************************************************************************
                          unit_lib.c  -  description
                             -------------------
    begin                : Sat Mar 16 2002
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
#include "localize.h"
#include "nation.h"
#include "file.h"
#include "config.h"
#include "player.h"
#include "unit.h"
#include "FPGE/pgf.h"

/*
====================================================================
Внешние
====================================================================
*/
extern Sdl sdl;
extern int log_x, log_y;
extern Font *log_font;
extern Config config;
extern List *units; //список активных юнитов
/*
====================================================================
Типы целей, типы движения, классы юнитов
Они могут быть загружены, только если unit_lib_main_loaded имеет значение False.
====================================================================
*/
int unit_lib_main_loaded = 0;
Trgt_Type *trgt_types = 0;
int trgt_type_count = 0;
Mov_Type *mov_types = 0;
int mov_type_count = 0;
Unit_Class *unit_classes= 0;
int unit_class_count = 0;
int icon_type;
SDL_Surface *icons = NULL;
int icon_width = 0, icon_height = 0, icon_columns;
int outside_icon_width = 0, outside_icon_height = 0;

/*
====================================================================
Значки плиток карты юнитов (сила, движение, атака ...)
====================================================================
*/
Unit_Info_Icons *unit_info_icons = 0;

/*
====================================================================
Список библиотеки юнитов, созданный вызовом UNIT_LIB_MAIN
из unit_lib_load ().
====================================================================
*/
List *unit_lib = 0;

/*
====================================================================
Таблица преобразования для string -> flag.
====================================================================
*/
StrToFlag fct_units[] = {
    { "swimming", SWIMMING },
    { "flying", FLYING },
    { "diving", DIVING },
    { "parachute", PARACHUTE },
    { "transporter", TRANSPORTER },
    { "recon", RECON },
    { "artillery", ARTILLERY },
    { "interceptor", INTERCEPTOR },
    { "air_defense", AIR_DEFENSE },
    { "bridge_eng", BRIDGE_ENG },
    { "infantry", INFANTRY },
    { "air_trsp_ok", AIR_TRSP_OK },
    { "destroyer", DESTROYER },
    { "ignore_entr", IGNORE_ENTR },
    { "carrier", CARRIER },
    { "carrier_ok", CARRIER_OK },
    { "bomber", BOMBER },
    { "attack_first", ATTACK_FIRST },
    { "low_entr_rate", LOW_ENTR_RATE },
    { "tank", TANK },
    { "anti_tank", ANTI_TANK },
    { "suppr_fire", SUPPR_FIRE },
    { "turn_suppr", TURN_SUPPR },
    { "jet", JET },
    { "ground_trsp_ok", GROUND_TRSP_OK },
    { "bunker_killer", BUNKER_KILLER },
    { "torpedo_bomber", TORPEDO_BOMBER },
    { "radar", RADAR },
    { "night_optics", NIGHT_OPTICS },
    { "banzai", BANZAI },
    { "guide", GUIDE },
    { "ranger", RANGER },
    { "fearless", FEARLESS },
    { "sonar", SONAR },
    { "kamikazi", KAMIKAZI },
    { "guard", GUARD },
    { "X", 0}
};

/*
====================================================================
Локальные
====================================================================
*/

/*
====================================================================
Получите геометрию значка в «значках» поверхности, используя три
Измерьте точки в левом верхнем, правом верхнем и левом нижнем углах.
====================================================================
*/
static void unit_get_icon_geometry( int icon_id, int *width, int *height, int *offset, Uint32 *key )
{
    Uint32 mark;
    int y;
    int count = icon_id * 2; /* есть два пикселя на одну иконку */

    /* нада белая точка! возьмите первый пиксель в верхнем левом углу как отметку */
    mark = get_pixel( icons, 0, 0 );
    /* вычислить смещение */
    for ( y = 0; y < icons->h; y++ )
        if ( get_pixel( icons, 0, y ) == mark ) {
            if ( count == 0 ) break;
            count--;
        }
    *offset = y;
    /* вычислить высоту */
    y++;
    while ( y < icons->h && get_pixel( icons, 0, y ) != mark )
        y++;
   (*height) = y - (*offset) + 1;
    /* вычислить ширину */
    y = *offset;
    *width = 1;
    while ( get_pixel( icons, (*width ), y ) != mark )
        (*width)++;
    (*width)++;
    /* пиксель рядом с левой верхней клавишей измерения - это цветовая клавиша */
    *key = get_pixel( icons, 1, *offset );
}

/*
====================================================================
Удалить запись библиотеки модулей.
====================================================================
*/
void unit_lib_delete_entry( void *ptr )
{
    Unit_Lib_Entry *entry = (Unit_Lib_Entry*)ptr;
    if ( entry == 0 ) return;
    if ( entry->id ) free( entry->id );
    if ( entry->name ) free( entry->name );
    if ( entry->icon ) SDL_FreeSurface( entry->icon );
    if ( entry->icon_tiny ) SDL_FreeSurface( entry->icon_tiny );
#ifdef WITH_SOUND
    if ( entry->wav_alloc && entry->wav_move )
        wav_free( entry->wav_move );
#endif
    free( entry );
}

/*
====================================================================
Оцените юнит. Эта оценка станет относительной, тогда как
лучший рейтинг - 1000. Каждый рабочий регион
земля / море / воздух будет иметь свою собственную ссылку.
Эта оценка специфична для PG. Стоимость не считается.
====================================================================
*/
void unit_lib_eval_unit( Unit_Lib_Entry *unit )
{
    int attack = 0, defense = 0, misc = 0;
    /* Оценка рассчитывается путем деления свойств юнита.
       по категориям. Оценки добавляются разными
       веса. */
    /* Первая категория охватывает навыки атаки. */
    if ( unit_has_flag( unit, "flying" ) ) {    //самолеты
        attack = unit->atks[0] + unit->atks[1] + /* земля */
                 2 * MAXIMUM( unit->atks[2], abs( unit->atks[2] ) / 2 ) + /* воздух */
                 unit->atks[3]; /* море */
    }
    else {
        if ( unit_has_flag( unit, "swimming" ) ) {  //корабли
            attack = unit->atks[0] + unit->atks[1] + /* земля */
                     unit->atks[2] + /* воздух */
                     2 * unit->atks[3]; /* море */
        }
        else {
            attack = 2 * MAXIMUM( unit->atks[0], abs( unit->atks[0] ) / 2 ) + /* soft */
                     2 * MAXIMUM( unit->atks[1], abs( unit->atks[1] ) / 2 ) + /* hard */
                     MAXIMUM( unit->atks[2], abs( unit->atks[2] ) / 2 ) + /* воздух */
                     unit->atks[3]; /* море */
        }
    }
    attack += unit->ini;
    attack += 2 * unit->rng;
    /* Вторая категория включает защитные навыки. */
    if ( unit_has_flag( unit, "flying" ) )
        defense = unit->def_grnd + 2 * unit->def_air;
    else {
        defense = 2 * unit->def_grnd + unit->def_air;
        if ( unit_has_flag( unit, "infantry" ) )
            /* немного раздуть пехоту, поскольку она не нуждается в
               значение близкой защиты */
            defense += 5;
        else
            defense += unit->def_cls;
    }
    /* Третья категория охватывает разные навыки.. */
    if ( unit_has_flag( unit, "flying" ) )
        misc = MINIMUM( 12, unit->ammo ) + MINIMUM( unit->fuel, 80 ) / 5 + unit->mov / 2;
    else
        misc = MINIMUM( 12, unit->ammo ) + MINIMUM( unit->fuel, 60 ) / 4 + unit->mov;
    /* Подводить итоги */
    unit->eval_score = ( 2 * attack + 2 * defense + misc ) / 5;
}

/*
====================================================================
Загрузите библиотеку модулей в формате udb. Если UNIT_LIB_MAIN передается target_types,
mov_types и юнит-классы будут загружены (может произойти только один раз)
====================================================================
*/
int unit_lib_load_udb( char *fname, char *path, int main )
{
    int i, icon_id;
    Unit_Lib_Entry *unit;
    List *entries, *flags;
    PData *pd, *sub, *subsub;
    char *str, *flag;
    char *domain = 0;
    /* информация журнала */
    int  log_dot_limit = 40; /* максимум точек */
    int  log_dot_count = 0; /* фактическое количество отображаемых точек */
    int  log_units_per_dot = 0; /* количество юнитов, которые представляет точка */
    int  log_unit_count = 0; /* if > units_per_dot добавлена ​​новая точка */
    char log_str[128];
    /* может быть только одна основная библиотека */
    if ( main == UNIT_LIB_MAIN && unit_lib_main_loaded ) {
        fprintf( stderr, tr("%s: can't load as main unit library (which is already loaded): loading as 'additional' instead\n"),
                 fname );
        main = UNIT_LIB_ADD;
    }
    /* анализировать файл */

    sprintf( log_str, tr("  Parsing '%s'"), fname );
    write_line( sdl.screen, log_font, log_str, log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
    if ( ( pd = parser_read_file( fname, path ) ) == 0 ) goto parser_failure;
    domain = determine_domain(pd, fname);
    locale_load_domain(domain, 0/*FIXME*/);
    /* если требуются основные или базовые данные, прочтите типы целей и т. д. */
    if ( main != UNIT_LIB_ADD ) {
        write_line( sdl.screen, log_font, tr("  Loading Main Definitions"), log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
        unit_lib = list_create( LIST_AUTO_DELETE, unit_lib_delete_entry );
        /* типы целей */
        if ( !parser_get_entries( pd, "target_types", &entries ) ) goto parser_failure;
        trgt_type_count = entries->count;
        if ( trgt_type_count > TARGET_TYPE_LIMIT ) {
            fprintf( stderr, tr("%i target types is the limit!\n"), TARGET_TYPE_LIMIT );
            trgt_type_count = TARGET_TYPE_LIMIT;
        }
        trgt_types = calloc( trgt_type_count, sizeof( Trgt_Type ) );
        list_reset( entries ); i = 0;
        while ( ( sub = list_next( entries ) ) ) {
            trgt_types[i].id = strdup( sub->name );
            if ( !parser_get_value( sub, "name", &str, 0 ) ) goto parser_failure;
            trgt_types[i].name = strdup(trd(domain, str));
            i++;
        }
        /* типы движения */
        if ( !parser_get_entries( pd, "move_types", &entries ) ) goto parser_failure;
        mov_types = calloc( entries->count, sizeof( Mov_Type ) );
        list_reset( entries ); mov_type_count = 0;
        while ( ( sub = list_next( entries ) ) ) {
            mov_types[mov_type_count].id = strdup( sub->name );
            if ( !parser_get_value( sub, "name", &str, 0 ) ) goto parser_failure;
            mov_types[mov_type_count].name = strdup(trd(domain, str));
#ifdef WITH_SOUND
            if ( parser_get_value( sub, "sound", &str, 0 ) )
            {
                search_file_name_exact( path, str, config.mod_name, "Sound" );
                mov_types[mov_type_count].wav_move = wav_load( path, 0 );
            }
#endif
            mov_type_count++;
        }
        /* классы юнитов */
        if ( !parser_get_entries( pd, "unit_classes", &entries ) ) goto parser_failure;
        unit_classes = calloc( entries->count, sizeof( Unit_Class ) );
        list_reset( entries ); unit_class_count = 0;
        while ( ( sub = list_next( entries ) ) ) {
            unit_classes[unit_class_count].id = strdup( sub->name );
            if ( !parser_get_value( sub, "name", &str, 0 ) ) goto parser_failure;
            unit_classes[unit_class_count].name = strdup(trd(domain, str));
	    unit_classes[unit_class_count].purchase = UC_PT_NONE;
	    if (parser_get_value( sub, "purchase", &str, 0 )) {
		    if (strcmp(str,"trsp") == 0)
			    unit_classes[unit_class_count].purchase = UC_PT_TRSP;
		    else if (strcmp(str,"normal") == 0)
			    unit_classes[unit_class_count].purchase = UC_PT_NORMAL;
	    }
	    unit_class_count++;
            /* игнорировать звуки пока */
        }
        /* значки плитки карты объекта */
        unit_info_icons = calloc( 1, sizeof( Unit_Info_Icons ) );
        if ( !parser_get_value( pd, "strength_icons", &str, 0 ) ) goto parser_failure;
        search_file_name_exact( path, str, config.mod_name, "Graphics" );
        unit_info_icons->str_w = 16;
        unit_info_icons->str_h = 12;
        if ( !parser_get_int( pd, "strength_icon_width", &outside_icon_width ) ) goto parser_failure;
        if ( !parser_get_int( pd, "strength_icon_height", &outside_icon_height ) ) goto parser_failure;
        if ( ( unit_info_icons->str = load_surf( path, SDL_SWSURFACE, outside_icon_width, outside_icon_height,
                                                 unit_info_icons->str_w, unit_info_icons->str_h ) ) == 0 ) goto failure;
        if ( !parser_get_value( pd, "attack_icon", &str, 0 ) ) goto parser_failure;
        search_file_name_exact( path, str, config.mod_name, "Theme" );
        if ( ( unit_info_icons->atk = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
        if ( !parser_get_value( pd, "move_icon", &str, 0 ) ) goto parser_failure;
        search_file_name_exact( path, str, config.mod_name, "Theme" );
        if ( ( unit_info_icons->mov = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
        if ( !parser_get_value( pd, "guard_icon", &str, 0 ) )
        {
            search_file_name( str, 0, "pg_guard", "", "", 'i' );
        }
        search_file_name_exact( path, str, config.mod_name, "Theme" );
        if ( ( unit_info_icons->guard = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
    }
    /* icons */
    if ( !parser_get_value( pd, "icon_type", &str, 0 ) ) goto parser_failure;
    if ( STRCMP( str, "fixed" ) )
        icon_type = UNIT_ICON_FIXED;
    else if ( STRCMP( str, "single" ) )
        icon_type = UNIT_ICON_SINGLE;
    else
        icon_type = UNIT_ICON_ALL_DIRS;
    if ( !parser_get_value( pd, "icons", &str, 0 ) ) goto parser_failure;
    search_file_name_exact( path, str, config.mod_name, "Graphics" );
    write_line( sdl.screen, log_font, tr("  Loading Tactical Icons"), log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
    if ( !parser_get_int( pd, "icon_width", &icon_width ) ) goto parser_failure;
    if ( !parser_get_int( pd, "icon_height", &icon_height ) ) goto parser_failure;
    if ( !parser_get_int( pd, "icon_columns", &icon_columns ) ) goto parser_failure;
    if ( ( icons = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
    if ( main != UNIT_LIB_BASE_DATA ) {
        /* записи библиотеки юнитов */
        if ( !parser_get_entries( pd, "unit_lib", &entries ) ) goto parser_failure;
          /* ЖУРНАЛ INIT */
          log_units_per_dot = entries->count / log_dot_limit;
          log_dot_count = 0;
          log_unit_count = 0;
          /* (ЖУРНАЛ) */
        list_reset( entries );
        while ( ( sub = list_next( entries ) ) ) {
            /* читать запись юнита */
            unit = calloc( 1, sizeof( Unit_Lib_Entry ) );
            /* идентификация */
            unit->id = strdup( sub->name );
            /* имя */
            if ( !parser_get_value( sub, "name", &str, 0) ) goto parser_failure;
            unit->name = strdup(trd(domain, str));
    	    /* нация (если не найдена или юнит "нет" нельзя купить) */
    	    unit->nation = -1; /* неопределенный */
    	    if ( parser_get_value( sub, "nation", &str, 0) && strcmp(str,"none") ) {
    	    	Nation *n = nation_find( str );
    	    	if (n)
    	    		unit->nation = nation_get_index( n );
    	    }
            /* ID класса */
            unit->class = 0;
            if ( !parser_get_value( sub, "class", &str, 0 ) ) goto parser_failure;
            for ( i = 0; i < unit_class_count; i++ )
                if ( STRCMP( str, unit_classes[i].id ) ) {
                    unit->class = i;
                    break;
                }
            /* идентификатор типа цели */
            unit->trgt_type = 0;
            if ( !parser_get_value( sub, "target_type", &str, 0 ) ) goto parser_failure;
            for ( i = 0; i < trgt_type_count; i++ )
                if ( STRCMP( str, trgt_types[i].id ) ) {
                    unit->trgt_type = i;
                    break;
                }
            /* инициатива */
            if ( !parser_get_int( sub, "initiative", &unit->ini ) ) goto parser_failure;
            /* обнаружение */
            if ( !parser_get_int( sub, "spotting", &unit->spt ) ) goto parser_failure;
            /* движение */
            if ( !parser_get_int( sub, "movement", &unit->mov ) ) goto parser_failure;
            /* идентификатор типа перемещения */
            unit->mov_type = 0;
            if ( !parser_get_value( sub, "move_type", &str, 0 ) ) goto parser_failure;
            for ( i = 0; i < mov_type_count; i++ )
                if ( STRCMP( str, mov_types[i].id ) ) {
                    unit->mov_type = i;
                    break;
                }
            /* топливо */
            if ( !parser_get_int( sub, "fuel", &unit->fuel ) ) goto parser_failure;
            /* дистанция */
            if ( !parser_get_int( sub, "range", &unit->rng ) ) goto parser_failure;
            /* боеприпасы */
            if ( !parser_get_int( sub, "ammo", &unit->ammo ) ) goto parser_failure;
            /* количество атак */
            if ( !parser_get_int( sub, "attack/count", &unit->atk_count ) ) goto parser_failure;
            /* значения атаки */
            if ( !parser_get_pdata( sub, "attack", &subsub ) ) goto parser_failure;
            for ( i = 0; i < trgt_type_count; i++ )
                if ( !parser_get_int( subsub, trgt_types[i].id, &unit->atks[i] ) ) goto parser_failure;
            /* наземная оборона */
            if ( !parser_get_int( sub, "def_ground", &unit->def_grnd ) ) goto parser_failure;
            /* воздушная оборона */
            if ( !parser_get_int( sub, "def_air", &unit->def_air ) ) goto parser_failure;
            /* близкая защита */
            if ( !parser_get_int( sub, "def_close", &unit->def_cls ) ) goto parser_failure;
            /* флаги */
            if ( parser_get_values( sub, "flags", &flags ) ) {
                list_reset( flags );
                while ( ( flag = list_next( flags ) ) ) {
                    int NumberInArray, Flag;
                    Flag = check_flag( flag, fct_units, &NumberInArray );
                    unit->flags[(int) (NumberInArray + 1) / 32] |= Flag;
                }
            }
            /* установить норму закрепления */
            unit->entr_rate = 2;
            if ( unit_has_flag( unit, "low_entr_rate" ) )
                unit->entr_rate = 1;
            else
                if ( unit_has_flag( unit, "infantry" ) )
                    unit->entr_rate = 3;
            /* период использования (0 == нельзя купить) */
            unit->start_year = unit->start_month = unit->last_year = 0;
            parser_get_int( sub, "start_year", &unit->start_year );
            parser_get_int( sub, "start_month", &unit->start_month );
            parser_get_int( sub, "last_year", &unit->last_year );
    	    /* стоимость единицы (0 == нельзя купить) */
	        unit->cost = 0;
        	parser_get_int( sub, "cost", &unit->cost );
            /* иконка */
            /* идентификатор значка */
            if ( !parser_get_int( sub, "icon_id", &icon_id ) ) goto parser_failure;
            /* icon_type */
            unit->icon_type = icon_type;
            /* установить маленькие и большие иконки */
            lib_entry_set_icons( icon_id, unit );
            /* читаю звуки - ну пока их нет ... */
#ifdef WITH_SOUND
            if ( parser_get_value( sub, "move_sound", &str, 0 ) ) {
                // FIXME перезагрузка одного и того же звука более одного раза является
                // большая трата времени загрузки, времени выполнения и памяти
                search_file_name_exact( path, str, config.mod_name, "Sound" );
                if ( ( unit->wav_move = wav_load( path, 0 ) ) )
                    unit->wav_alloc = 1;
                else {
                    unit->wav_move = mov_types[unit->mov_type].wav_move;
                    unit->wav_alloc = 0;
                }
            }
            else {
                unit->wav_move = mov_types[unit->mov_type].wav_move;
                unit->wav_alloc = 0;
            }
#endif
            /* добавить объект в базу данных */
            list_add( unit_lib, unit );
            /* абсолютная оценка */
            unit_lib_eval_unit( unit );
            /* ЖУРНАЛ */
            log_unit_count++;
            if ( log_unit_count >= log_units_per_dot ) {
                log_unit_count = 0;
                if ( log_dot_count < log_dot_limit ) {
                    log_dot_count++;
                    strcpy( log_str, "  [                                        ]" );
                    for ( i = 0; i < log_dot_count; i++ )
                        log_str[3 + i] = '*';
                    write_text( log_font, sdl.screen, log_x, log_y, log_str, 255 );
                    SDL_UpdateRect( sdl.screen, log_font->last_x, log_font->last_y, log_font->last_width, log_font->last_height );
                }
            }
        }
        write_line( sdl.screen, log_font, log_str, log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
    }
    parser_free( &pd );
    /* ЖУРНАЛ */
    relative_evaluate_units();
    free(domain);
    if ( main != UNIT_LIB_BASE_DATA )
        SDL_FreeSurface(icons);
    return 1;
parser_failure:
    fprintf( stderr, "%s\n", parser_get_error() );
failure:
    unit_lib_delete();
    if ( pd ) parser_free( &pd );
    free(domain);
    SDL_FreeSurface(icons);
    return 0;
}

/*
====================================================================
Загрузите библиотеку юнитов в формате lgdudb.
====================================================================
*/
int unit_lib_load_lgdudb( char *fname, char *path )
{
    int i;
    char *domain = 0;

    FILE *inf;
    Unit_Lib_Entry *unit;
    char line[1024], tokens[100][256], log_str[128];
    int j, block=0, last_line_length=-1, cursor=0, token=0, cur_trgt_type=0, cur_mov_type=0, cur_unit_class=0;
    int utf16 = 0, lines=0;
    trgt_type_count = 0;
    mov_type_count = 0;
    unit_class_count = 0;

    sprintf( log_str, tr("  Parsing '%s'"), fname );
    write_line( sdl.screen, log_font, log_str, log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
    write_line( sdl.screen, log_font, tr("  Loading Main Definitions"), log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
    unit_lib = list_create( LIST_AUTO_DELETE, unit_lib_delete_entry );

    inf=fopen(path,"rb");
    if (!inf)
    {
        fprintf( stderr, "Couldn't open nation database file\n");
        return 0;
    }

    //найти количество типов целей, типов движения и классов юнитов
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
            trgt_type_count++;
        if (block == 3 && strlen(line)>0)
            mov_type_count++;
        if (block == 4 && strlen(line)>0)
            unit_class_count++;
        if (block == 5)
        {
            fclose(inf);
            break;
        }
    }
    if ( trgt_type_count > TARGET_TYPE_LIMIT ) {
        fprintf( stderr, tr("%i target types is the limit!\n"), TARGET_TYPE_LIMIT );
        trgt_type_count = TARGET_TYPE_LIMIT;
    }
    trgt_types = calloc( trgt_type_count, sizeof( Trgt_Type ) );
    mov_types = calloc( mov_type_count, sizeof( Mov_Type ) );
    unit_classes = calloc( unit_class_count, sizeof( Unit_Class ) );
    unit_info_icons = calloc( 1, sizeof( Unit_Info_Icons ) );
    unit_info_icons->str_w = 16;
    unit_info_icons->str_h = 12;

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

        //Block#1 данные файлов значков
        if (block == 1 && strlen(line)>0)
        {
/*            if ( strcmp( tokens[0], "domain" ) == 0 )
            {
                domain = determine_domain(tokens[1], fname);
            }*/
            if ( strcmp( tokens[0], "icon_type" ) == 0 )
            {
                if ( STRCMP( tokens[1], "fixed" ) )
                    icon_type = UNIT_ICON_FIXED;
                else if ( STRCMP( tokens[1], "single" ) )
                    icon_type = UNIT_ICON_SINGLE;
                else
                    icon_type = UNIT_ICON_ALL_DIRS;
            }
            if ( strcmp( tokens[0], "icon_height" ) == 0 )
            {
                icon_height = atoi( tokens[1] );
            }
            if ( strcmp( tokens[0], "icon_width" ) == 0 )
            {
                icon_width = atoi( tokens[1] );
            }
            if ( strcmp( tokens[0], "icon_columns" ) == 0 )
            {
                icon_columns = atoi( tokens[1] );
            }
            if ( strcmp( tokens[0], "icons" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                write_line( sdl.screen, log_font, tr("  Loading Tactical Icons"), log_x, &log_y ); refresh_screen( 0, 0, 0, 0 );
                if ( ( icons = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
            }
            if ( strcmp( tokens[0], "strength_icon_width" ) == 0 )
            {
                outside_icon_width = atoi( tokens[1] );
            }
            if ( strcmp( tokens[0], "strength_icon_height" ) == 0 )
            {
                outside_icon_height = atoi( tokens[1] );
            }
            if ( strcmp( tokens[0], "strength_icons" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Graphics" );
                if ( ( unit_info_icons->str = load_surf( path, SDL_SWSURFACE, outside_icon_width, outside_icon_height,
                                                         unit_info_icons->str_w, unit_info_icons->str_h ) ) == 0 ) goto failure;
            }
            if ( strcmp( tokens[0], "attack_icon" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Theme" );
                if ( ( unit_info_icons->atk = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
            }
            if ( strcmp( tokens[0], "move_icon" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Theme" );
                if ( ( unit_info_icons->mov = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
            }
            if ( strcmp( tokens[0], "guard_icon" ) == 0 )
            {
                search_file_name_exact( path, tokens[1], config.mod_name, "Theme" );
                if ( ( unit_info_icons->guard = load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ) ) == 0 ) goto failure;
            }
        }

        //Block#2 описание типов целей
        if (block == 2 && strlen(line)>0)
        {
            trgt_types[cur_trgt_type].id = strdup( tokens[0] );
            trgt_types[cur_trgt_type].name = strdup(trd(domain, tokens[1]));
            cur_trgt_type++;
        }

        //Block#3 описание типов движения
        if (block == 3 && strlen(line)>0)
        {
            mov_types[cur_mov_type].id = strdup( tokens[0] );
            mov_types[cur_mov_type].name = strdup(trd(domain, tokens[1]));
#ifdef WITH_SOUND
            search_file_name_exact( path, tokens[2], config.mod_name, "Sound" );
            mov_types[cur_mov_type].wav_move = wav_load( path, 0 );
#endif
            cur_mov_type++;
        }

        //Block#4 описание классов юнитов
        if (block == 4 && strlen(line)>0)
        {
            unit_classes[cur_unit_class].id = strdup( tokens[0] );
            unit_classes[cur_unit_class].name = strdup(trd(domain, tokens[1]));
            unit_classes[cur_unit_class].purchase = UC_PT_NONE;
            if (strcmp(tokens[2],"trsp") == 0)
                unit_classes[cur_unit_class].purchase = UC_PT_TRSP;
            else if (strcmp(tokens[2],"normal") == 0)
                unit_classes[cur_unit_class].purchase = UC_PT_NORMAL;
            cur_unit_class++;
        }

        //Block#5 данные юнита
        if (block == 5 && strlen(line)>0)
        {
            /* читать запись юнита */
            unit = calloc( 1, sizeof( Unit_Lib_Entry ) );
            /* идентификация */
            unit->id = strdup( tokens[0] );
            /* имя */
            unit->name = strdup(trd(domain, tokens[1]));
            /* нация (если не найдена или юнит "нет" нельзя купить) */
            unit->nation = -1; /* неопределенный */
            if ( strcmp(tokens[2],"none") ) {
                Nation *n = nation_find( tokens[2] );
                if (n)
                    unit->nation = nation_get_index( n );
            }
            /* ID класса */
            unit->class = 0;
            for ( j = 0; j < unit_class_count; j++ )
                if ( STRCMP( tokens[3], unit_classes[j].id ) ) {
                    unit->class = j;
                    break;
                }
            /* идентификатор типа цели */
            unit->trgt_type = 0;
            for ( j = 0; j < trgt_type_count; j++ )
                if ( STRCMP( tokens[4], trgt_types[j].id ) ) {
                    unit->trgt_type = j;
                    break;
                }
            /* инициатива */
            unit->ini = atoi( tokens[5] );
            /* обнаружение */
            unit->spt = atoi( tokens[6] );
            /* движение */
            unit->mov = atoi( tokens[7] );
            /* идентификатор типа перемещения */
            unit->mov_type = 0;
            for ( j = 0; j < mov_type_count; j++ )
                if ( STRCMP( tokens[8], mov_types[j].id ) ) {
                    unit->mov_type = j;
                    break;
                }
            /* топливо */
            unit->fuel = atoi( tokens[9] );
            /* расстояние */
            unit->rng = atoi( tokens[10] );
            /* боеприпасы */
            unit->ammo = atoi( tokens[11] );
            /* количество атак */
            unit->atk_count = atoi( tokens[21] );
            /* значения атаки */
            for ( j = 0; j < trgt_type_count; j++ )
                unit->atks[j] = atoi( tokens[j + 22] );
            /* наземная оборона */
            unit->def_grnd = atoi( tokens[18] );
            /* воздушная оборона */
            unit->def_air = atoi( tokens[19] );
            /* близкая защита */
            unit->def_cls = atoi( tokens[20] );
            /* флаги */
            for ( j = 22 + trgt_type_count; j < token; j++ )
            {
                if ( atoi( tokens[j] ) == 1 )
                {
                    int NumberInArray, Flag;
                    Flag = check_flag( fct_units[j - 22 - trgt_type_count].string, fct_units, &NumberInArray );
                    unit->flags[(int) (NumberInArray + 1) / 32] |= Flag;
                }
            }
            /* установить норму окопанности */
            unit->entr_rate = 2;
            if ( unit_has_flag( unit, "low_entr_rate" ) )
                unit->entr_rate = 1;
            else
                if ( unit_has_flag( unit, "infantry" ) )
                    unit->entr_rate = 3;
            /* период использования (0 == нельзя купить) */
            unit->start_year = unit->start_month = unit->last_year = 0;
            unit->start_year = atoi( tokens[15] );
            unit->start_month = atoi( tokens[16] );
            unit->last_year = atoi( tokens[17] );
            /* стоимость единицы (0 == нельзя купить) */
            unit->cost = 0;
            unit->cost = atoi( tokens[13] );
            /* иконка */
            /* icon_type */
            unit->icon_type = icon_type;
            /* установить маленькие и большие иконки */
            lib_entry_set_icons( atoi( tokens[12] ), unit );
            /* читаю звуки - ну пока их нет ... */
#ifdef WITH_SOUND
            // FIXME перезагрузка одного и того же звука более одного раза является
            // большая трата времени загрузки, времени выполнения и памяти
            if ( strcmp( tokens[14], "" ) != 0 )
            {
                search_file_name_exact( path, tokens[14], config.mod_name, "Sound" );
                if ( ( unit->wav_move = wav_load( path, 0 ) ) )
                    unit->wav_alloc = 1;
                else {
                    unit->wav_move = mov_types[unit->mov_type].wav_move;
                    unit->wav_alloc = 0;
                }
            }
            else {
                unit->wav_move = mov_types[unit->mov_type].wav_move;
                unit->wav_alloc = 0;
            }
#endif
            /* добавить юнит в базу данных */
            list_add( unit_lib, unit );
            /* абсолютная оценка */
            unit_lib_eval_unit( unit );
        }
    }
    fclose(inf);
    /* ЖУРНАЛ */
    relative_evaluate_units();
    free(domain);
    SDL_FreeSurface(icons);
    return 1;
failure:
    unit_lib_delete();
    free(domain);
    SDL_FreeSurface(icons);
    return 0;
}

/*
====================================================================
Публичные
====================================================================
*/

/*
====================================================================
Загрузите библиотеку модулей. Если UNIT_LIB_MAIN передается target_types,
mov_types и юнит-классы будут загружены (может произойти только один раз)
====================================================================
*/
int unit_lib_load( char *fname, int main )
{
    char *path, *extension;
    path = calloc( 256, sizeof( char ) );
    extension = calloc( 10, sizeof( char ) );
    search_file_name_exact( path, fname, config.mod_name, "Scenario" );
    extension = strrchr(fname,'.');
    extension++;
    if ( strcmp( extension, "udb" ) == 0 )
        return unit_lib_load_udb( fname, path, main );
    else if ( strcmp( extension, "lgdudb" ) == 0 )
        return unit_lib_load_lgdudb( fname, path );
    return 0;
}

/*
====================================================================
Удалить библиотеку юнитов.
====================================================================
*/
void unit_lib_delete( void )
{
    int i;
    if ( unit_lib ) {
        list_delete( unit_lib );
        unit_lib = 0;
    }
    if ( trgt_types ) {
        for ( i = 0; i < trgt_type_count; i++ ) {
            if ( trgt_types[i].id ) free( trgt_types[i].id );
            if ( trgt_types[i].name ) free( trgt_types[i].name );
        }
        free( trgt_types );
        trgt_types = 0; trgt_type_count = 0;
    }
    if ( mov_types ) {
        for ( i = 0; i < mov_type_count; i++ ) {
            if ( mov_types[i].id ) free( mov_types[i].id );
            if ( mov_types[i].name ) free( mov_types[i].name );
#ifdef WITH_SOUND
            if ( mov_types[i].wav_move )
                wav_free( mov_types[i].wav_move );
#endif
        }
        free( mov_types );
        mov_types = 0; mov_type_count = 0;
    }
    if ( unit_classes ) {
        for ( i = 0; i < unit_class_count; i++ ) {
            if ( unit_classes[i].id ) free( unit_classes[i].id );
            if ( unit_classes[i].name ) free( unit_classes[i].name );
        }
        free( unit_classes );
        unit_classes = 0; unit_class_count = 0;
    }
    if ( unit_info_icons ) {
        if ( unit_info_icons->str ) SDL_FreeSurface( unit_info_icons->str );
        if ( unit_info_icons->atk ) SDL_FreeSurface( unit_info_icons->atk );
        if ( unit_info_icons->mov ) SDL_FreeSurface( unit_info_icons->mov );
        if ( unit_info_icons->guard ) SDL_FreeSurface( unit_info_icons->guard );
        free( unit_info_icons );
        unit_info_icons = 0;
    }
    unit_lib_main_loaded = 0;
}

/*
====================================================================
Найдите запись в библиотеке юнита по строке идентификатора.
====================================================================
*/
Unit_Lib_Entry* unit_lib_find( char *id )
{
    Unit_Lib_Entry *entry;
    list_reset( unit_lib );
    while ( ( entry = list_next( unit_lib ) ) )
        if ( STRCMP( entry->id, id ) )
            return entry;
    return 0;
}

/*
====================================================================
Найдите значки юнитов на листе изображений.
====================================================================
*/
void lib_entry_set_icons( int icon_id, Unit_Lib_Entry *unit )
{
    int i, j;
    int width, height, offset;
    Uint32 color_key;
    float scale;
    if ( icon_width == 0 || icon_height == 0 )
        unit_get_icon_geometry( icon_id, &width, &height, &offset, &color_key );
    /* изображение сначала копируется из unit_pics
    * если picture_type не ALL_DIRS, изображение является одиночным изображением
    */
    if ( unit->icon_type == UNIT_ICON_ALL_DIRS ) {
        unit->icon = create_surf( width * 6, height, SDL_SWSURFACE );
        unit->icon_w = width;
        unit->icon_h = height;
        FULL_DEST( unit->icon );
        SOURCE( icons, 0, offset );
        blit_surf();
        /* удалить точки измерения */
        set_pixel( unit->icon, 0, 0, color_key );
        set_pixel( unit->icon, 0, unit->icon_h - 1, color_key );
        set_pixel( unit->icon, unit->icon_w - 1, 0, color_key );
        /* установить прозрачность */
        SDL_SetColorKey( unit->icon, SDL_SRCCOLORKEY, color_key );
    }
    else
    {
        if ( unit->icon_type == UNIT_ICON_SINGLE )
        {
            /* установить размер */
            unit->icon_w = width;
            unit->icon_h = height;
            /* создать и скопировать первую картинку */
            unit->icon = create_surf( unit->icon_w * 2, unit->icon_h, SDL_SWSURFACE );
            DEST( unit->icon, 0, 0, width, height );
            SOURCE( icons, 0, offset );
            blit_surf();
            /* удалить точки измерения */
            set_pixel( unit->icon, 0, 0, color_key );
            set_pixel( unit->icon, 0, unit->icon_h - 1, color_key );
            set_pixel( unit->icon, unit->icon_w - 1, 0, color_key );
            /* установить прозрачность */
            SDL_SetColorKey( unit->icon, SDL_SRCCOLORKEY, color_key );
            /* установить маленькие иконки */
            color_key = get_pixel( unit->icon, 0, 0 );
            scale = 1.5;
            unit->icon_tiny = create_surf( unit->icon->w * ( 1.0 / scale ), unit->icon->h * ( 1.0 / scale ), SDL_SWSURFACE );
            unit->icon_tiny_w = unit->icon_w * ( 1.0 / scale );
            unit->icon_tiny_h = unit->icon_h * ( 1.0 / scale );
            for ( j = 0; j < unit->icon_tiny->h; j++ ) {
                for ( i = 0; i < unit->icon_tiny->w; i++ ) {
                    set_pixel( unit->icon_tiny, i, j, get_pixel( unit->icon, scale * i, scale * j ) );
                }
            }
        }
        else
        {
            color_key = get_pixel( icons, 0, 0 );
            /* установить размер */
            unit->icon_w = icon_width;
            unit->icon_h = icon_height;
            /* создать и скопировать первую картинку */
            unit->icon = create_surf( unit->icon_w * 2, icon_height, SDL_SWSURFACE );
            DEST( unit->icon, 0, 0, icon_width, icon_height );
            SOURCE( icons, ( icon_id % icon_columns ) * icon_width, ( (int)(icon_id / icon_columns) ) * icon_height );
            blit_surf();
            /* установить прозрачность */
            SDL_SetColorKey( unit->icon, SDL_SRCCOLORKEY, color_key );
        }
    }
}

/*
====================================================================
При необходимости переверните значки единиц.
====================================================================
*/
void adjust_fixed_icon_orientation()
{
    int i, j;
    int byte_size, y_offset;
    SDL_Surface *tempSurface;
    Unit_Lib_Entry *unit;
    float scale;
    Uint32 color_key;
    Unit *unit_scen;    //активные юниты в сценарии

    list_reset( unit_lib );
    while ( ( unit = list_next( unit_lib ) ) )
    {
        if ( unit->icon_type != UNIT_ICON_ALL_DIRS )
        {
            if ( player_get_by_nation( nation_find_by_id( unit->nation ) ) != 0 )
            {
                if ( strcmp( player_get_by_nation( nation_find_by_id( unit->nation ) )->id, "allies" ) == 0 )
                {
                    /* изображение на листе изображений обращено влево - скопируйте его вправо */
                    DEST( unit->icon, unit->icon->w / 2, 0, unit->icon->w, unit->icon->h );
                    SOURCE( unit->icon, 0, 0 );
                    blit_surf();
                    /* получить информацию о формате */
                    byte_size = unit->icon->format->BytesPerPixel;
                    tempSurface = create_surf( unit->icon_w, unit->icon_h, SDL_SWSURFACE );
                    y_offset = 0;
                    /* перевернуть изображение на левую сторону поверхности */
                    for ( j = 0; j < unit->icon_h; j++ )
                    {
                        for ( i = 0; i < unit->icon_w; i++ )
                        {
                            memcpy( unit->icon->pixels +
                                    y_offset +
                                    i * byte_size,
                                    unit->icon->pixels +
                                    y_offset +
                                    unit->icon_w * byte_size +
                                    ( unit->icon_w - 1 - i ) * byte_size,
                                    byte_size );
                        }
                        y_offset += unit->icon->pitch;
                    }
                    SDL_SetColorKey( unit->icon, SDL_SRCCOLORKEY, get_pixel( unit->icon, 0, 0 ) );
                }
                else
                {
                    /* получить информацию о формате */
                    byte_size = unit->icon->format->BytesPerPixel;
                    tempSurface = create_surf( unit->icon_w, unit->icon_h, SDL_SWSURFACE );
                    y_offset = 0;
                    /* перевернуть изображение на правую сторону поверхности */
                    for ( j = 0; j < unit->icon_h; j++ ) {
                        for ( i = 0; i < unit->icon_w; i++ ) {
                            memcpy( unit->icon->pixels +
                                    y_offset +
                                    unit->icon_w * byte_size +
                                    i * byte_size,
                                    unit->icon->pixels +
                                    y_offset +
                                    ( unit->icon_w - 1 - i ) * byte_size,
                                    byte_size );
                        }
                        y_offset += unit->icon->pitch;
                    }
                    SDL_SetColorKey( unit->icon, SDL_SRCCOLORKEY, get_pixel( unit->icon, 0, 0 ) );
                }
            }
            else
                if ( strspn( unit->name, "AD" ) >= 2 ) //Была у бункеров неправильная ориентация (стояло AF)
                {
                    /* изображение на листе изображений обращено влево - скопируйте его вправо */
                    DEST( unit->icon, unit->icon->w / 2, 0, unit->icon->w, unit->icon->h );
                    SOURCE( unit->icon, 0, 0 );
                    blit_surf();
                    /* получить информацию о формате */
                    byte_size = unit->icon->format->BytesPerPixel;
                    tempSurface = create_surf( unit->icon_w, unit->icon_h, SDL_SWSURFACE );
                    y_offset = 0;
                    /* перевернуть изображение на левую сторону поверхности */
                    for ( j = 0; j < unit->icon_h; j++ )
                    {
                        for ( i = 0; i < unit->icon_w; i++ )
                        {
                            memcpy( unit->icon->pixels +
                                    y_offset +
                                    i * byte_size,
                                    unit->icon->pixels +
                                    y_offset +
                                    unit->icon_w * byte_size +
                                    ( unit->icon_w - 1 - i ) * byte_size,
                                    byte_size );
                        }
                        y_offset += unit->icon->pitch;
                    }
                    SDL_SetColorKey( unit->icon, SDL_SRCCOLORKEY, get_pixel( unit->icon, 0, 0 ) );
                }
                else
                {
                    /* получить информацию о формате */
                    byte_size = unit->icon->format->BytesPerPixel;
                    tempSurface = create_surf( unit->icon_w, unit->icon_h, SDL_SWSURFACE );
                    y_offset = 0;
                    /* перевернуть изображение на правую сторону поверхности */
                    for ( j = 0; j < unit->icon_h; j++ ) {
                        for ( i = 0; i < unit->icon_w; i++ ) {
                            memcpy( unit->icon->pixels +
                                    y_offset +
                                    unit->icon_w * byte_size +
                                    i * byte_size,
                                    unit->icon->pixels +
                                    y_offset +
                                    ( unit->icon_w - 1 - i ) * byte_size,
                                    byte_size );
                        }
                        y_offset += unit->icon->pitch;
                    }
                    SDL_SetColorKey( unit->icon, SDL_SRCCOLORKEY, get_pixel( unit->icon, 0, 0 ) );
                }

            color_key = get_pixel( unit->icon, 0, 0 );
            scale = 1.5;
            unit->icon_tiny = create_surf( unit->icon->w * ( 1.0 / scale ), unit->icon->h * ( 1.0 / scale ), SDL_SWSURFACE );
            unit->icon_tiny_w = unit->icon_w * ( 1.0 / scale );
            unit->icon_tiny_h = unit->icon_h * ( 1.0 / scale );
            for ( j = 0; j < unit->icon_tiny->h; j++ ) {
                for ( i = 0; i < unit->icon_tiny->w; i++ ) {
                    set_pixel( unit->icon_tiny, i, j, get_pixel( unit->icon, scale * i, scale * j ) );
                }
            }
            /* использовать цветовой ключ «большого» изображения */
            SDL_SetColorKey( unit->icon_tiny, SDL_SRCCOLORKEY, color_key );

            //Передача маленьких иконок активным юнитам в сценарии
            list_reset( units );
            while ( ( unit_scen = list_next( units ) ) )
            {
                if ( unit_scen->prop.id == unit->id ) {
                    unit_scen->prop.icon_tiny = unit->icon_tiny;
                    unit_scen->prop.icon_tiny_w = unit->icon_tiny_w;
                    unit_scen->prop.icon_tiny_h = unit->icon_tiny_h;
                }
                if ( unit_scen->trsp_prop.id == unit->id ) {
                    unit_scen->trsp_prop.icon_tiny = unit->icon_tiny;
                    unit_scen->trsp_prop.icon_tiny_w = unit->icon_tiny_w;
                    unit_scen->trsp_prop.icon_tiny_h = unit->icon_tiny_h;
                }
            }

        }
    }
}

/*
====================================================================
Относительная оценка юнитов
====================================================================
*/
void relative_evaluate_units()
{
    Unit_Lib_Entry *unit;
    int best_ground = 0,
        best_air = 0,
        best_sea = 0; /* используется для относительности оценок */
    list_reset( unit_lib );
    while ( ( unit = list_next( unit_lib ) ) ) {
        if ( unit_has_flag( unit, "flying" ) )  //самолеты
            best_air = MAXIMUM( unit->eval_score, best_air );
        else {
            if ( unit_has_flag( unit, "swimming" ) )    //корабли
                best_sea = MAXIMUM( unit->eval_score, best_sea );
            else
                best_ground = MAXIMUM( unit->eval_score, best_ground );
        }
    }
    list_reset( unit_lib );
    while ( ( unit = list_next( unit_lib ) ) ) {
        unit->eval_score *= 1000;
        if ( unit_has_flag( unit, "flying" ) )  //самолеты
            unit->eval_score /= best_air;
        else {
            if ( unit_has_flag( unit, "swimming" ) )    //корабли
                unit->eval_score /= best_sea;
            else
                unit->eval_score /= best_ground;
        }
    }
}

/*
====================================================================
Проверьте, есть ли у юнита особый флаг, отображающий особые способности.
====================================================================
*/
int unit_has_flag( Unit_Lib_Entry *unit, char *flag )
{
    int NumberInArray, Flag;
    Flag = check_flag( flag, fct_units, &NumberInArray );
    if ( ( NumberInArray < 31 && unit->flags[0] & Flag ) || ( NumberInArray >= 31 && unit->flags[1] & Flag ) )
        return 1;
    return 0;
}

/*
====================================================================
Получить идентификатор (номер) типа движения
====================================================================
*/
int movement_type_get_index( char *movement_type )
{
    int i;
    for ( i = 0; i < mov_type_count; i++ )
    {
        if ( strcmp( movement_type, mov_types[i].id ) == 0 )
            return i;
    }
    return -1;
}

