/***************************************************************************
                          scenario.c  -  description
                             -------------------
    begin                : Fri Jan 19 2001
    copyright            : (C) 2001 by Michael Speck
                           (C) 2005 by Leo Savernik
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

#include "scenario.h"

#include "list.h"
#include "parser.h"

#include "map.h"
#include "unit_lib.h"
#include "util.h"

#include <string.h>

int scen_detect( struct PData *pd ) {
    char *dummy;
    List *entries;
    if ( !parser_get_value( pd, "authors", &dummy, 0 ) ) return 0;
    if ( !parser_get_value( pd, "date", &dummy, 0 ) ) return 0;
    if ( !parser_get_value( pd, "turns", &dummy, 0 ) ) return 0;
    if ( !parser_get_value( pd, "turns_per_day", &dummy, 0 ) ) return 0;
    if ( !parser_get_value( pd, "days_per_turn", &dummy, 0 ) ) return 0;
    if ( !parser_get_entries( pd, "players", &entries ) ) return 0;
    return 1;
}

int scen_extract( struct PData *pd, struct Translateables *xl )
{
    PData *sub, *subsub;
    PData *pd_vcond;
    List *entries;
    /* получить информацию о сценарии */
    if ( !parser_get_pdata( pd, "name", &sub ) ) goto parser_failure;
    translateables_add_pdata(xl, sub);
    if ( !parser_get_pdata( pd, "desc", &sub ) ) goto parser_failure;
    translateables_add_pdata(xl, sub);
    if ( !parser_get_pdata( pd, "authors", &sub ) ) goto parser_failure;
    translateables_add_pdata(xl, sub);
    /* нации */
    /* библиотеки юнитов */
    if ( !parser_get_pdata( pd, "unit_db", &sub ) ) {
        unit_lib_extract(pd, xl);
    }
    /* карта и погода */
    if ( !parser_get_pdata( pd, "map", &sub ) ) {
        map_extract(pd, xl);
    }
    /* игроки */
    if ( !parser_get_entries( pd, "players", &entries ) ) goto parser_failure;
    list_reset( entries );
    while ( ( sub = list_next( entries ) ) ) {
        /* создать игрока */
        if ( !parser_get_pdata( sub, "name", &subsub ) ) goto parser_failure;
        translateables_add_pdata(xl, subsub);
    }
    /* устанавливать союзы */
    /* флаги */
    /* условия победы */
    /* тип чека */
    if ( !parser_get_entries( pd, "result", &entries ) ) goto parser_failure;
    /* reset vic conds не может быть выполнен в scene_delete (), так как это называется
       перед проверкой */
    /* count conditions */
    /* create conditions */
    list_reset( entries );
    while ( ( pd_vcond = list_next( entries ) ) ) {
        if ( strcmp( pd_vcond->name, "cond" ) == 0 ) {
            if ( parser_get_pdata( pd_vcond, "message", &subsub ) )
                translateables_add_pdata(xl, subsub);
            /* и связь */
            /* или связь */
            /* никаких дополнительных условий? */
            /* следующее условие */
        }
    }
    /* else условие (используется, если никакое другое условие не выполнено и сценарий завершен) */
    translateables_add(xl, "Defeat", "");
    if ( parser_get_pdata( pd, "result/else", &pd_vcond ) ) {
        if ( parser_get_pdata( pd_vcond, "message", &subsub ) )
            translateables_add_pdata(xl, subsub);
    }
    /* юниты */
    /* гексагоны развертывания нагрузки */
    return 1;
parser_failure:        
    fprintf( stderr, "%s\n", parser_get_error() );
    return 0;
}

