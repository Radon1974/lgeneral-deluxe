/***************************************************************************
                          nation.h  -  description
                             -------------------
    begin                : Wed Jan 24 2001
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

#ifndef __NATION_H
#define __NATION_H

struct PData;
struct Translateables;

/** Проверяет, похоже ли это дерево синтаксического анализа на описание нации. */
int nation_detect( struct PData *pd );

/** Извлекает переводимые из страны в переводимый набор. */
int nation_extract( struct PData *pd, struct Translateables *xl );

#endif
