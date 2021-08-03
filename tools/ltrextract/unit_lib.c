/***************************************************************************
                          unit_lib.c  -  description
                             -------------------
    begin                : Sat Mar 16 2002
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

#include "unit_lib.h"

#include "list.h"
#include "parser.h"
#include "util.h"

int unit_lib_detect( struct PData *pd ) {
    List *entries;
    if ( !parser_get_entries( pd, "unit_lib", &entries ) ) return 0;
    return 1;
}

int unit_lib_extract( struct PData *pd, struct Translateables *xl )
{
    List *entries;
    PData *sub, *subsub;
    /* если основные читаемые целевые типы и компания */
    /* типы целей */
    if ( parser_get_entries( pd, "target_types", &entries ) ) {
        list_reset( entries );
        while ( ( sub = list_next( entries ) ) ) {
            if ( !parser_get_pdata( sub, "name", &subsub ) ) goto parser_failure;
            translateables_add_pdata(xl, subsub);
        }
    }
    /* типы движения */
    if ( parser_get_entries( pd, "move_types", &entries ) ) {
        list_reset( entries );
        while ( ( sub = list_next( entries ) ) ) {
            if ( !parser_get_pdata( sub, "name", &subsub ) ) goto parser_failure;
            translateables_add_pdata(xl, subsub);
        }
    }
    /* классы юнитов */
    if ( parser_get_entries( pd, "unit_classes", &entries ) ) {
        list_reset( entries );
        while ( ( sub = list_next( entries ) ) ) {
            if ( !parser_get_pdata( sub, "name", &subsub ) ) goto parser_failure;
            translateables_add_pdata(xl, subsub);
        }
    }
    /* юниты значки плитки карты */
    /* иконки */
    /* записи библиотеки юнитов */
    if ( !parser_get_entries( pd, "unit_lib", &entries ) ) goto parser_failure;
    list_reset( entries );
    while ( ( sub = list_next( entries ) ) ) {
        /* читать запись юнитов */
        /* identification */
        /* имя */
        if ( !parser_get_pdata( sub, "name", &subsub ) ) goto parser_failure;
        translateables_add_pdata(xl, subsub);
        /* ID класса */
        /* идентификатор типа цели */
        /* инициатива */
        /* пятно */
        /* движение */
        /* идентификатор типа перемещения */
        /* топливо */
        /* ассортимент */
        /* боеприпасы */
        /* количество атак */
        /* значения атаки */
        /* наземная оборона */
        /* воздушная оборона */
        /* близкая защита */
        /* флаги */
        /* устанавливаем скорость закрепления */
        /* период использования */
        /* икона */
        /* идентификатор значка */
        /* icon_type */
        /* получаем положение и размер на поверхности иконок */
        /* изображение сначала копируется из unit_pics
         * если тип_изображения не ALL_DIRS, изображение представляет собой одно изображение, смотрящее вправо;
         * добавить перевернутое изображение, смотрящее влево
         */
        /* читаем звуки - ну пока их нет ... */
        /* добавляем объект в базу данных */
        /* абсолютная оценка */
    }
    return 1;
parser_failure:
    fprintf( stderr, "%s\n", parser_get_error() );
    return 0;
}

