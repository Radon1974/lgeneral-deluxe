/***************************************************************************
                          map.h  -  description
                             -------------------
    begin                : Sat Jan 20 2001
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

#ifndef __MAP_H
#define __MAP_H

struct PData;
struct Translateables;

/** Проверяет, похоже ли это дерево синтаксического анализа на описание карты. */
int map_detect( struct PData *pd );

/** Извлекает переводимые объекты из карты в набор для перевода. */
int map_extract( struct PData *pd, struct Translateables *xl );

#endif
