/***************************************************************************
                          ai_group.c  -  description
                             -------------------
    begin                : Fri Jan 19 2001
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

#include "lgeneral.h"
#include "unit.h"
#include "action.h"
#include "map.h"
#include "ai_tools.h"
#include "ai_group.h"

/*
====================================================================
Внешние
====================================================================
*/
extern Player *cur_player;
extern List *units, *avail_units;
extern int map_w, map_h;
extern Map_Tile **map;
extern Mask_Tile **mask;
extern int trgt_type_count;
extern int cur_weather;
extern Config config;

/*
====================================================================
МЕСТНЫЕ
====================================================================
*/

/* О НЕТ, ВЗЛОМ! */
extern int ai_get_dist( Unit *unit, int x, int y, int type, int *dx, int *dy, int *dist );

/*
====================================================================
Проверьте, нет ли незапятнанной плитки или врага в радиусе 6
который может подойти достаточно близко, чтобы атаковать. Летающие юниты не учитываются
поскольку они в любом случае могут продвинуться очень далеко.
====================================================================
*/
typedef struct {
    Player *player;
    int unit_x, unit_y;
    int unsafe;
} MountCtx;
static int hex_is_safe( int x, int y, void *_ctx )
{
    MountCtx *ctx = _ctx;
    if ( !mask[x][y].spot ) {
        ctx->unsafe = 1;
        return 0;
    }
    if ( map[x][y].g_unit )
    if ( !player_is_ally( ctx->player, map[x][y].g_unit->player ) )
    if ( map[x][y].g_unit->sel_prop->mov >= get_dist( ctx->unit_x, ctx->unit_y, x, y ) - 1 ) {
        ctx->unsafe = 1;
        return 0;
    }
    return 1;
}
static int ai_unsafe_mount( Unit *unit, int x, int y ) 
{
    MountCtx ctx = { unit->player, x, y, 0 };
    ai_eval_hexes( x, y, 6, hex_is_safe, &ctx );
    return ctx.unsafe;
}

/*
====================================================================
Подсчитайте количество защитников.
====================================================================
*/
typedef struct {
    Unit *unit;
    int count;
} DefCtx;
static int hex_df_unit( int x, int y, void *_ctx )
{
    DefCtx *ctx = _ctx;
    if ( map[x][y].g_unit ) {
        if ( unit_has_flag( ctx->unit->sel_prop, "flying" ) ) {
            if ( unit_has_flag( map[x][y].g_unit->sel_prop, "air_defense" ) )
                ctx->count++;
        }
        else {
            if ( unit_has_flag( map[x][y].g_unit->sel_prop, "artillery" ) )
                ctx->count++;
        }
    }
    return 1;
}
static void ai_count_df_units( Unit *unit, int x, int y, int *result )
{
    DefCtx ctx = { unit, 0 };
    *result = 0;
    if ( unit_has_flag( unit->sel_prop, "artillery" ) )
        return;
    ai_eval_hexes( x, y, 3, hex_df_unit, &ctx );
    /* учитываются только три защитника */
    if ( *result > 3 )
        *result = 3;
}

/*
====================================================================
Соберите все допустимые цели юнита.
====================================================================
*/
typedef struct {
    Unit *unit;
    List *targets;
} GatherCtx;
static int hex_add_targets( int x, int y, void *_ctx )
{
    GatherCtx *ctx = _ctx;
    if ( mask[x][y].spot ) {
        if ( map[x][y].a_unit && unit_check_attack( ctx->unit, map[x][y].a_unit, UNIT_ACTIVE_ATTACK ) )
            list_add( ctx->targets, map[x][y].a_unit );
        if ( map[x][y].g_unit && unit_check_attack( ctx->unit, map[x][y].g_unit, UNIT_ACTIVE_ATTACK ) )
            list_add( ctx->targets, map[x][y].g_unit );
    }
    return 1;
}
static List* ai_gather_targets( Unit *unit, int x, int y )
{
    GatherCtx ctx;
    ctx.unit = unit;
    ctx.targets= list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    ai_eval_hexes( x, y, unit->sel_prop->rng + 1, hex_add_targets, &ctx );
    return ctx.targets;
}


/*
====================================================================
Оцените атаку юнита по цели.
  score_base: базовый счет для атаки
  score_rugged: очки добавляются за каждую точку защиты (0-100)
                целевой
  score_kill: единица очков получает за каждое (ожидаемое) очко
              урон силы нанесен цели
  score_loss: количество очков, вычитаемое за очко силы
              ожидается потеря единицы
Окончательная оценка сохраняется в 'result' и True, если возвращается, если
атака может быть выполнена иначе Ложь.
====================================================================
*/
static int unit_evaluate_attack( Unit *unit, Unit *target, int score_base, int score_rugged, int score_kill, int score_loss, int *result )
{
    int unit_dam = 0, target_dam = 0, rugged_def = 0;
    if ( !unit_check_attack( unit, target, UNIT_ACTIVE_ATTACK ) ) return 0;
    unit_get_expected_losses( unit, target, &unit_dam, &target_dam );
    if ( unit_check_rugged_def( unit, target ) )
        rugged_def = unit_get_rugged_def_chance( unit, target );
    if ( rugged_def < 0 ) rugged_def = 0;
    *result = score_base + rugged_def * score_rugged + target_dam * score_kill + unit_dam * score_loss;
    AI_DEBUG( 2, "  %s: %s: bas:%i, rug:%i, kil:%i, los: %i = %i\n", unit->name, target->name,
            score_base,
            rugged_def * score_rugged,
            target_dam * score_kill,
            unit_dam * score_loss, *result );
    /* если цель - единица df, дайте небольшой бонус */
    if ( unit_has_flag( target->sel_prop, "artillery" ) || unit_has_flag( target->sel_prop, "air_defense" ) )
        *result += score_kill;
    return 1;
}

/*
====================================================================
Получите лучшую цель для юнита, если таковая имеется.
====================================================================
*/
static int ai_get_best_target( Unit *unit, int x, int y, AI_Group *group, Unit **target, int *score )
{
    int old_x = unit->x, old_y = unit->y;
    int pos_targets;
    Unit *entry;
    List *targets;
    int score_atk_base, score_rugged, score_kill, score_loss;
    
    /* оценки */
    score_atk_base = 20 + group->order * 10;
    score_rugged   = -1;
    score_kill     = ( group->order + 3 ) * 10;
    score_loss     = ( 2 - group->order ) * -10;
    
    unit->x = x; unit->y = y;
    *target = 0; *score = -999999;
    /* если нужен транспортер, нападение - самоубийство */
    if ( mask[x][y].mount && unit->trsp_prop.id )
        return 0;
    /* собирать цели */
    targets = ai_gather_targets( unit, x, y );
    /* оценивать все атаки */
    if ( targets ) {
        list_reset( targets ); 
        while ( ( entry = list_next( targets ) ) )
            if ( !unit_evaluate_attack( unit, entry, score_atk_base, score_rugged, score_kill, score_loss, &entry->target_score ) )
                list_delete_item( targets, entry ); /* ошибочный ввод: нельзя атаковать */
        /* проверьте, существуют ли какие-либо положительные цели */
        pos_targets = 0;
        list_reset( targets ); 
        while ( ( entry = list_next( targets ) ) )
            if ( entry->target_score > 0 ) {
                pos_targets = 1;
                break;
            }
        /* получить лучшую цель */
        list_reset( targets ); 
        while ( ( entry = list_next( targets ) ) ) {
            /* если юнит находится на цели или в центре интереса, дайте бонус
            так как эта плитка должна быть захвачена во что бы то ни стало. но делайте это только если есть
            нет другой цели с положительным значением */
            if ( !pos_targets )
            if ( ( entry->x == group->x && entry->y == group->y ) || map[entry->x][entry->y].obj ) {
                entry->target_score += 100;
                AI_DEBUG( 2, "    + 100 for %s: capture by all means\n", entry->name );
            }
                
            if ( entry->target_score > *score ) {
                *target = entry;
                *score = entry->target_score;
            }
        }
        list_delete( targets );
    }
    unit->x = old_x; unit->y = old_y;
    return (*target) != 0;
}

/*
====================================================================
Оцените позицию отряда, проверив контекст группы.
Верните True, если эта оценка верна. Результаты сохраняются
в "eval".
====================================================================
*/
typedef struct {
    Unit *unit; /* юнит, который проверен */
    AI_Group *group;
    int x, y; /* позиция, которая была оценена */
    int mov_score; /* результат для переезда */
    Unit *target; /* если установлено atk_result, актуально */
    int atk_score; /* результат, включая оценку атаки */
} AI_Eval;
static AI_Eval *ai_create_eval( Unit* unit, AI_Group *group, int x, int y )
{
    AI_Eval *eval = calloc( 1, sizeof( AI_Eval ) );
    eval->unit = unit; eval->group = group;
    eval->x = x; eval->y = y;
    return eval;
}
int ai_evaluate_hex( AI_Eval *eval )
{
    int result;
    int i, nx, ny, ox, oy,odist, covered, j, nx2, ny2;
    eval->target = 0;
    eval->mov_score = eval->atk_score = 0;
    /* модификации местности, которые применяются только для наземных подразделений */
    if ( !unit_has_flag( eval->unit->sel_prop, "flying" ) ) {
        /* бонус за окоп. пехота получает больше других. */
        eval->mov_score += ( unit_has_flag( eval->unit->sel_prop, "infantry" )?2:1) *
                           ( terrain_type_find( map[eval->x][eval->y].terrain_id[0] )->min_entr * 2 + 
                             terrain_type_find( map[eval->x][eval->y].terrain_id[0] )->max_entr );
        /* если юнит теряет инициативу на этой местности, мы даем малус */
        if ( terrain_type_find( map[eval->x][eval->y].terrain_id[0] )->max_ini < eval->unit->sel_prop->ini )
            eval->mov_score -= 5 * ( eval->unit->sel_prop->ini - 
                                     terrain_type_find( map[eval->x][eval->y].terrain_id[0] )->max_ini );
        /* рек следует избегать */
        if ( terrain_type_find( map[eval->x][eval->y].terrain_id[0] )->flags[cur_weather] & RIVER )
            eval->mov_score -= 50;
        if ( terrain_type_find( map[eval->x][eval->y].terrain_id[0] )->flags[cur_weather] & SWAMP )
            eval->mov_score -= 30;
        /* inf_close_def принесет пользу пехоте, а
           другие юниты */
        if ( terrain_type_find( map[eval->x][eval->y].terrain_id[0] )->flags[cur_weather] & INF_CLOSE_DEF ) {
            if ( unit_has_flag( eval->unit->sel_prop, "infantry" ) )
                eval->mov_score += 30;
            else
                eval->mov_score -= 20;
        }
        /* если это верховая позиция и противник или туман меньше
           На 6 плиток мы даем большой малус */
        if ( mask[eval->x][eval->y].mount )
            if ( ai_unsafe_mount( eval->unit, eval->x, eval->y ) )
                eval->mov_score -= 1000;
        /* завоевание флага дает бонус */
        if ( map[eval->x][eval->y].player )
            if ( !player_is_ally( eval->unit->player, map[eval->x][eval->y].player ) )
                if ( map[eval->x][eval->y].g_unit == 0 ) {
                    eval->mov_score += 600;
                    if ( map[eval->x][eval->y].obj )
                        eval->mov_score += 600;
                }
        /* если эта позиция позволяет производить окорку или находится на расстоянии одного гекса
           эта плитка получает большой бонус. */
        if ( eval->unit->embark == EMBARK_SEA ) {
            if ( map_check_unit_debark( eval->unit, eval->x, eval->y, EMBARK_SEA, 0 ) )
                eval->mov_score += 1000;
            else
                for ( i = 0; i < 6; i++ )
                    if ( get_close_hex_pos( eval->x, eval->y, i, &nx, &ny ) )
                        if ( map_check_unit_debark( eval->unit, nx, ny, EMBARK_SEA, 0 ) ) {
                            eval->mov_score += 500;
                            break;
                        }
        }
    }
    /* модификации летательных аппаратов */
    if ( unit_has_flag( eval->unit->sel_prop, "flying" ) ) {
        /* если перехватчик прикрывает раскрытый бомбардировщик на этой плитке, дает бонус */
        if ( unit_has_flag( eval->unit->sel_prop, "interceptor" ) ) {
            for ( i = 0; i < 6; i++ )
                if ( get_close_hex_pos( eval->x, eval->y, i, &nx, &ny ) )
                if ( map[nx][ny].a_unit )
                if ( player_is_ally( cur_player, map[nx][ny].a_unit->player ) )
                if ( unit_has_flag( map[nx][ny].a_unit->sel_prop, "bomber" ) ) {
                    covered = 0;
                    for ( j = 0; j < 6; j++ )
                        if ( get_close_hex_pos( nx, ny, j, &nx2, &ny2 ) )
                        if ( map[nx2][ny2].a_unit )
                        if ( player_is_ally( cur_player, map[nx2][ny2].a_unit->player ) )
                        if ( unit_has_flag( map[nx2][ny2].a_unit->sel_prop, "interceptor" ) ) 
                        if ( map[nx2][ny2].a_unit != eval->unit ) {
                            covered = 1;
                            break;
                        }
                    if ( !covered )
                        eval->mov_score += 2000; /* 100 равно одной плитке получения
                                                    в центр интереса, который должен
                                                    быть преодоленным */
                }
        }
    }
    /* у каждой группы есть «центр интересов». приближается
       этому центру почитается. */
    if ( eval->group->x == -1 ) {
        /* перейти к ближайшему флагу */
        if ( !unit_has_flag( eval->unit->sel_prop, "flying" ) ) {
            if ( eval->group->order > 0 ) {
                if ( ai_get_dist( eval->unit, eval->x, eval->y, AI_FIND_ENEMY_OBJ, &ox, &oy, &odist ) )
                    eval->mov_score -= odist * 100;
            }
            else
                if ( eval->group->order < 0 ) {
                    if ( ai_get_dist( eval->unit, eval->x, eval->y, AI_FIND_OWN_OBJ, &ox, &oy, &odist ) )
                        eval->mov_score -= odist * 100;
                }
        }
    }
    else
        eval->mov_score -= 100 * get_dist( eval->x, eval->y, eval->group->x, eval->group->y );
    /* защитная поддержка */
    ai_count_df_units( eval->unit, eval->x, eval->y, &result );
    if ( result > 2 ) result = 2; /* осмысленный предел */
    eval->mov_score += result * 10;
    /* проверьте лучшую цель и сохраните результат в atk_score */
    eval->atk_score = eval->mov_score;
    if ( !mask[eval->x][eval->y].mount )
    if ( !unit_has_flag( eval->unit->sel_prop, "attack_first" ) || 
          ( eval->unit->x == eval->x && eval->unit->y == eval->y ) )
    if ( ai_get_best_target( eval->unit, eval->x, eval->y, eval->group, &eval->target, &result ) )
        eval->atk_score += result;
    return 1;
}

/*
====================================================================
Выберите и сохраните лучшие тактические действия отряда (найденные с помощью
ai_evaluate_hex). Если нет AI_SUPPLY, сохраняется.
====================================================================
*/
void ai_handle_unit( Unit *unit, AI_Group *group )
{
    int x, y, nx, ny, i, action = 0;
    List *list = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    AI_Eval *eval;
    Unit *target = 0;
    int score = -999999;
    /* получить маску движения */
    map_get_unit_move_mask( unit );
    x = unit->x; y = unit->y; target = 0;
    /* оценить все позиции */
    list_add( list, ai_create_eval( unit, group, unit->x, unit->y ) );
    while ( list->count > 0 ) {
        eval = list_dequeue( list );
        if ( ai_evaluate_hex( eval ) ) {
            /* оценка движения */
            if ( eval->mov_score > score ) {
                score = eval->mov_score;
                target = 0;
                x = eval->x; y = eval->y;
            }
            /* движение + оценка атаки. игнорировать для атаки_first
             * юниты, которые уже стреляли */
            if ( !unit_has_flag( unit->sel_prop, "attack_first" ) )
            if ( eval->target && eval->atk_score > score ) {
                score = eval->atk_score;
                target = eval->target;
                x = eval->x; y = eval->y;
            }
        }
        /* хранить следующие шестигранные плитки */
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( eval->x, eval->y, i, &nx, &ny ) )
                if ( ( mask[nx][ny].in_range && !mask[nx][ny].blocked ) || mask[nx][ny].sea_embark ) {
                    mask[nx][ny].in_range = 0; 
                    mask[nx][ny].sea_embark = 0;
                    list_add( list, ai_create_eval( unit, group, nx, ny ) );
                }
        free( eval );
    }
    list_delete( list );
    /* проверить результат и сохранить соответствующее действие */
    if ( unit->x != x || unit->y != y ) {
        if ( map_check_unit_debark( unit, x, y, EMBARK_SEA, 0 ) ) {
            action_queue_debark_sea( unit, x, y ); action = 1;
            AI_DEBUG( 1, "%s debarks at %i,%i\n", unit->name, x, y );
        }
        else {
            action_queue_move( unit, x, y ); action = 1;
            AI_DEBUG( 1, "%s moves to %i,%i\n", unit->name, x, y );
        }
    }
    if ( target ) {
        action_queue_attack( unit, target ); action = 1;
        AI_DEBUG( 1, "%s attacks %s\n", unit->name, target->name );
    }
    if ( !action ) {
        action_queue_supply( unit );
        AI_DEBUG( 1, "%s supplies: %i,%i\n", unit->name, unit->cur_ammo, unit->cur_fuel );
    }
}

/*
====================================================================
Получите лучшую цель и атакуйте по дальности. Не пытайтесь переместить
юнита пока нет. Если цели совсем нет, ничего не делайте.
====================================================================
*/
void ai_fire_artillery( Unit *unit, AI_Group *group )
{
    AI_Eval *eval = ai_create_eval( unit, group, unit->x, unit->y );
    if ( ai_evaluate_hex( eval ) && eval->target )
    {
        action_queue_attack( unit, eval->target );
        AI_DEBUG( 1, "%s attacks first %s\n", unit->name, eval->target->name );
    }
    free( eval );
}

/*
====================================================================
Публичный
====================================================================
*/

/*
====================================================================
Создать / Удалить группу
====================================================================
*/
AI_Group *ai_group_create( int order, int x, int y )
{
    AI_Group *group = calloc( 1, sizeof( AI_Group ) );
    group->state = GS_ART_FIRE; 
    group->order = order;
    group->x = x; group->y = y;
    group->units = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    list_reset( group->units );
    return group;
}
void ai_group_add_unit( AI_Group *group, Unit *unit )
{
    /* отсортировать юниты в список единиц в том порядке, в котором юниты
     * должен двигаться. артиллерийский огонь идет первым, а идет последним
     * Таким образом, артиллерия занимает последнее место в этом списке. что он срабатывает первым
     * обрабатывается ai_group_handle_next_unit. порядок списка:
     * бомбардировщики, наземные части, истребители, артиллерия */
    if ( unit_has_flag( &unit->prop, "artillery" ) || 
         unit_has_flag( &unit->prop, "air_defense" ) )
        list_add( group->units, unit );
    else
    if ( unit->prop.class == 9 || unit->prop.class == 10 )
    {
        /* тактический бомбардировщик высокого уровня */
        list_insert( group->units, unit, 0 );
        group->bomber_count++;
    }
    else
    if ( unit_has_flag( &unit->prop, "flying" ) )
    {
        /* авиационные наземные подразделения не входят в этот раздел ... */
        list_insert( group->units, unit, 
                     group->bomber_count + group->ground_count );
        group->aircraft_count++;
    }
    else
    {
        /* все остальное: корабли и наземные подразделения */
        list_insert( group->units, unit, group->bomber_count );
        group->ground_count++;
    }
    /* HACK: установите флаг hold_pos для тех юнитов, которые не должны двигаться из-за
       к высокому окопу или близости к артиллерии и т. п .; но эти
       юниты тоже будут атаковать и могут двигаться, если оно того стоит */
    if (!unit_has_flag( unit->sel_prop, "flying" ))
    if (!unit_has_flag( unit->sel_prop, "swimming" ))
    {
        int i,nx,ny,no_move = 0;
        if (map[unit->x][unit->y].obj) no_move = 1;
        if (group->order<0)
        {
            if (map[unit->x][unit->y].nation) no_move = 1;
            if (unit->entr>=6) no_move = 1;
            if ( unit_has_flag( unit->sel_prop, "artillery" ) ) no_move = 1;
            if ( unit_has_flag( unit->sel_prop, "air_defense" ) ) no_move = 1;
            for (i=0;i<6;i++) 
                if (get_close_hex_pos(unit->x,unit->y,i,&nx,&ny))
                    if (map[nx][ny].g_unit)
                    {
                        if ( unit_has_flag( map[nx][ny].g_unit->sel_prop, "artillery" ) )
                        {
                            no_move = 1;
                            break;
                        }
                        if ( unit_has_flag( map[nx][ny].g_unit->sel_prop, "air_defense" ) )
                        {
                            no_move = 1;
                            break;
                        }
                    }
        }
        if (no_move) unit->cur_mov = 0;
    }
}
void ai_group_delete_unit( AI_Group *group, Unit *unit )
{
    /* удалить блок */
    int contained_unit = list_delete_item( group->units, unit );
    if ( !contained_unit ) return;
    
    /* обновить соответствующий счетчик */
    if ( unit_has_flag( &unit->prop, "artillery" ) || 
         unit_has_flag( &unit->prop, "air_defense" ) )
        /* ничего не поделаешь */;
    else
    if ( unit->prop.class == 9 || unit->prop.class == 10 )
    {
        /* тактический бомбардировщик высокого уровня */
        group->bomber_count--;
    }
    else
    if ( unit_has_flag( &unit->prop, "flying" ) )
    {
        /* авиационные наземные подразделения не входят в этот раздел ... */
        group->aircraft_count--;
    }
    else
    {
        /* все остальное: корабли и наземные подразделения */
        group->ground_count--;
    }
}
void ai_group_delete( void *ptr )
{
    AI_Group *group = ptr;
    if ( group ) {
        if ( group->units )
            list_delete( group->units );
        free( group );
    }
}
/*
====================================================================
Обращайтесь к следующему отряду в группе, чтобы следовать порядку. Хранит все необходимое
единичные действия. Если группа полностью обработана, возвращается False.
====================================================================
*/
int ai_group_handle_next_unit( AI_Group *group )
{
    Unit *unit = list_next( group->units );
    if ( unit == 0 ) 
    {
        if ( group->state == GS_MOVE )
            return 0;
        else
        {
            group->state = GS_MOVE;
            list_reset( group->units );
            unit = list_next( group->units );
            if ( unit == 0 ) return 0;
        }
    }
    if ( unit == 0 ) {
        AI_DEBUG(0,"ERROR: %s: null unit detected\n",__FUNCTION__);
	return 0;
    }
    /* Юнит мертв? Может быть только нападающий, которого убил защитник */
    if ( unit->killed ) {
      AI_DEBUG(0, "Removing killed attacker %s(%d,%d) from group\n", unit->name, unit->x, unit->y);
      ai_group_delete_unit( group, unit );
      return ai_group_handle_next_unit( group );
    }
    if ( group->state == GS_ART_FIRE )
    {
        if ( unit_has_flag( unit->sel_prop, "attack_first" ) )
            ai_fire_artillery( unit, group ); /* не проверяет оптимально
                                                 движение а просто
                                                 пожары */
    }
    else
        ai_handle_unit( unit, group ); /* проверки в полной тактической
                                          расширенные */
    return 1;
}
