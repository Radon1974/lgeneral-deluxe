/***************************************************************************
                          ai_tools.h  -  description
                             -------------------
    begin                : Sun Jul 14 2002
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

#ifndef __AI_GROUP_H
#define __AI_GROUP_H

/*
====================================================================
Группа армий ИИ
====================================================================
*/
enum {
    GS_ART_FIRE = 0,
    GS_MOVE
};
typedef struct {
    List *units;
    int order; /* -2: защищать позицию всеми средствами
                  -1: защищать позицию
                   0: перейти к позиции
                   1: позиция атаки
                   2: атакуйте позицию всеми средствами */
    int x, y; /* позиция, которая является центром интересов этой группы */
    int state; /* указать, как указано выше */
    /* счетчики для отсортированного списка объектов */
    int ground_count, aircraft_count, bomber_count;
} AI_Group;

/*
====================================================================
Создать / Удалить группу
====================================================================
*/
AI_Group *ai_group_create( int order, int x, int y );
void ai_group_add_unit( AI_Group *group, Unit *unit );
void ai_group_delete_unit( AI_Group *group, Unit *unit );
void ai_group_delete( void *ptr );
/*
====================================================================
Обращайтесь к следующему юниту в группе, чтобы следовать порядку Хранит все необходимое
единичные действия. Если группа полностью обработана, возвращается False.
====================================================================
*/
int ai_group_handle_next_unit( AI_Group *group );

/* HACK - это обычно находится в ai.c, но должно быть помещено здесь как
   мы временно используем его в ai_groups.c */
enum {
    AI_FIND_DEPOT,
    AI_FIND_OWN_TOWN,
    AI_FIND_ENEMY_TOWN,
    AI_FIND_OWN_OBJ,
    AI_FIND_ENEMY_OBJ
};

#endif
