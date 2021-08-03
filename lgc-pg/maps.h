/***************************************************************************
                          maps.h -  description
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

#ifndef __MAPS_H
#define __MAPS_H

/*
====================================================================
If map_id is -1 конвертировать все карты, найденные в 'source_path'.
If map_id is >= 0 это одна настраиваемая карта с данными в
текущий каталог.
If MAPNAMES.STR не предоставляется в текущем рабочем каталоге
он ищется в 'source_path', поэтому используются значения по умолчанию.
====================================================================
*/
int maps_convert( int map_id );

#endif
