/***************************************************************************
                          action.h  -  description
                             -------------------
    begin                : Mon Apr 1 2002
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

#ifndef __ACTION_H
#define __ACTION_H

/*
====================================================================
действиЯ двигателя
====================================================================
*/
enum {
    ACTION_NONE = 0,
    ACTION_END_TURN,
    ACTION_MOVE,
    ACTION_ATTACK,
    ACTION_SUPPLY,
    ACTION_EMBARK_SEA,
    ACTION_DEBARK_SEA,
    ACTION_EMBARK_AIR,
    ACTION_DEBARK_AIR,
    ACTION_MERGE,
    ACTION_SPLIT,
    ACTION_REPLACE,
    ACTION_ELITE_REPLACE,
    ACTION_DISBAND,
    ACTION_DEPLOY,
    ACTION_DRAW_MAP,
    ACTION_SET_SPOT_MASK,
    ACTION_SET_VMODE,
    ACTION_QUIT,
    ACTION_RESTART,
    ACTION_LOAD,
    ACTION_OVERWRITE,
    ACTION_START_SCEN,
    ACTION_START_CAMP,
    ACTION_CHANGE_MOD
};
typedef struct {
    int type;       /* введите, как указано выше */
    Unit *unit;     /* юнит, выполнЯющий действие */
    Unit *target;   /* цель, если атака */
    int x, y;       /* место назначениЯ, если движение */
    int w, h, full; /* настройки видеорежима */
    int id;         /* идентификатор, используемый воздушным блоком дебарка */
    char *name;     /* имЯ слота если таковое имеется */
    int str;        /* сила раскола */
} Action;

/*
====================================================================
создать / удалить очередь действий двигателя
====================================================================
*/
void actions_create();
void actions_delete();

/*
====================================================================
получить следующее действие или отменить все действиЯ. Возвращенная структура действия
должен очищаться двигателем после использования.
====================================================================
*/
Action* actions_dequeue();
void actions_clear();

/*
====================================================================
Удалить последнее действие в очереди (отмененное подтверждение)
====================================================================
*/
void action_remove_last();

/*
====================================================================
Возвращает самое верхнее действие в очереди или 0, если ничего не доступно.
====================================================================
*/
Action *actions_top(void);

/*
====================================================================
Получить количество действий в очереди
====================================================================
*/
int actions_count();

/*
====================================================================
Создайте действие двигателя и автоматически поставьте его в очередь. Двигатель
выполнит проверки безопасности перед обработкой действия по предотвращению
противоправные действия.
====================================================================
*/
void action_queue_none();
void action_queue_end_turn();
void action_queue_move( Unit *unit, int x, int y );
void action_queue_attack( Unit *unit, Unit *target );
void action_queue_supply( Unit *unit );
void action_queue_embark_sea( Unit *unit, int x, int y );
void action_queue_debark_sea( Unit *unit, int x, int y );
void action_queue_embark_air( Unit *unit, int x, int y );
void action_queue_debark_air( Unit *unit, int x, int y, int normal_landing );
void action_queue_merge( Unit *unit, Unit *partner );
void action_queue_split( Unit *unit, int str, int x, int y, Unit *partner );
void action_queue_replace( Unit *unit );
void action_queue_elite_replace( Unit *unit );
void action_queue_disband( Unit *unit );
void action_queue_modify( Unit *unit );
void action_queue_deploy( Unit *unit, int x, int y );
void action_queue_draw_map();
void action_queue_set_spot_mask();
void action_queue_set_vmode( int w, int h, int fullscreen );
void action_queue_quit();
void action_queue_restart();
void action_queue_load( char *name );
void action_queue_overwrite( char *name );
void action_queue_start_scen();
void action_queue_start_camp();
void action_queue_change_mod();

#endif
