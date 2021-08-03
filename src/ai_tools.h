/***************************************************************************
                          ai_tools.h  -  description
                             -------------------
    begin                : Sat Oct 5 2002
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

#ifndef __AI_TOOLS_H
#define __AI_TOOLS_H

#define AI_DEBUG( loglev, ... ) \
	do { if (loglev <= config.ai_debug) printf(__VA_ARGS__); } while (0)

/*
====================================================================
Проверьте окружение плитки и примените функцию оценка к
это. Результаты могут быть сохранены в контексте.
Если eval_func возвращает false, оценка прерывается.
====================================================================
*/
typedef struct {
    int x, y;
} AI_Pos;
void ai_eval_hexes( int x, int y, int range, int(*eval_func)(int,int,void*), void *ctx );

#endif
