/***************************************************************************
                          ai.c  -  description
                             -------------------
    begin                : Thu Apr 11 2002
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
#include "date.h"
#include "unit.h"
#include "action.h"
#include "map.h"
#include "ai_tools.h"
#include "ai_group.h"
#include "ai.h"
#include "scenario.h"

#include <assert.h>

/*
====================================================================
Внешние
====================================================================
*/
extern Player *cur_player;
extern List *units, *avail_units, *unit_lib;
extern int map_w, map_h;
extern Map_Tile **map;
extern Mask_Tile **mask;
extern int trgt_type_count;
extern int deploy_turn;
extern Nation *nations;
extern Scen_Info *scen_info;
extern Config config;

/*
====================================================================
Внутренний материал
====================================================================
*/
enum {
    AI_STATUS_INIT = 0, /* инициировать ход */
    AI_STATUS_FINALIZE, /* финализировать ход */
    AI_STATUS_DEPLOY,   /* развернуть новые подразделения */
    AI_STATUS_SUPPLY,   /* единицы снабжения, которые в этом нуждаются */
    AI_STATUS_MERGE,    /* слияние поврежденных юнитов */
    AI_STATUS_GROUP,    /* группировать и управлять другими единицами */
    AI_STATUS_PURCHASE, /* заказать новые единицы */
    AI_STATUS_END       /* конец хода АИ */
};
static int ai_status = AI_STATUS_INIT; /* текущий статус ИИ */
static List *ai_units = 0; /* юниты, которые должны быть обработаны */
static AI_Group *ai_group = 0; /* пока это только одна группа */
static int finalized = 1; /* установите значение true при финализации */

/*
====================================================================
Местные
====================================================================
*/

/*
====================================================================
Получить уровень поддержки, необходимый для получения желаемого абсолютного
значения, где боеприпасы или топливо 0 означает, что это значение не проверять.
====================================================================
*/
#ifdef _1
static int unit_get_supply_level( Unit *unit, int abs_ammo, int abs_fuel )
{
    int ammo_level = 0, fuel_level = 0, miss_ammo, miss_fuel;
    unit_check_supply( unit, UNIT_SUPPLY_ANYTHING, &miss_ammo, &miss_fuel );
    if ( abs_ammo > 0 && miss_ammo > 0 ) {
        if ( miss_ammo > abs_ammo ) miss_ammo = abs_ammo;
        ammo_level = 100 * unit->prop.ammo / miss_ammo;
    }
    if ( abs_fuel > 0 && miss_fuel > 0 ) {
        if ( miss_fuel > abs_fuel ) miss_fuel = abs_fuel;
        ammo_level = 100 * unit->prop.fuel / miss_fuel;
    }
    if ( fuel_level > ammo_level ) 
        return fuel_level;
    return ammo_level;
}
#endif

/*
====================================================================
Получите расстояние «юнита» и положение объекта специального
типа.
====================================================================
*/
static int ai_check_hex_type( Unit *unit, int type, int x, int y )
{
    switch ( type ) {
        case AI_FIND_DEPOT:
            if ( map_is_allied_depot( &map[x][y], unit ) )
                return 1;
            break;
        case AI_FIND_ENEMY_OBJ:
            if ( !map[x][y].obj ) return 0;
        case AI_FIND_ENEMY_TOWN:
            if ( map[x][y].player && !player_is_ally( unit->player, map[x][y].player ) )
                return 1;
            break;
        case AI_FIND_OWN_OBJ:
            if ( !map[x][y].obj ) return 0;
        case AI_FIND_OWN_TOWN:
            if ( map[x][y].player && player_is_ally( unit->player, map[x][y].player ) )
                return 1;
            break;
    }
    return 0;
}
int ai_get_dist( Unit *unit, int x, int y, int type, int *dx, int *dy, int *dist )
{
    int found = 0, length;
    int i, j;
    *dist = 999;
    for ( i = 0; i < map_w; i++ )
        for ( j = 0; j < map_h; j++ )
            if ( ai_check_hex_type( unit, type, i, j ) ) {
                length = get_dist( i, j, x, y );
                if ( *dist > length ) {
                    *dist = length;
                    *dx = i; *dy = j;
                    found = 1;
                }
            }
    return found;
}

/*
====================================================================
Приблизительный пункт назначения по наилучшей позиции перемещения в диапазоне.
====================================================================
*/
static int ai_approximate( Unit *unit, int dx, int dy, int *x, int *y )
{
    int i, j, dist = get_dist( unit->x, unit->y, dx, dy ) + 1;
    *x = unit->x; *y = unit->y;
    map_clear_mask( F_AUX );
    map_get_unit_move_mask( unit );
    for ( i = 0; i < map_w; i++ )
        for ( j = 0; j < map_h; j++ )
            if ( mask[i][j].in_range && !mask[i][j].blocked )
                mask[i][j].aux = get_dist( i, j, dx, dy ) + 1;
    for ( i = 0; i < map_w; i++ )
        for ( j = 0; j < map_h; j++ )
            if ( dist > mask[i][j].aux && mask[i][j].aux > 0 ) {
                dist = mask[i][j].aux;
                *x = i; *y = j;
            }
    return ( *x != unit->x || *y != unit->y );
}

/*
====================================================================
Оцените все юниты, проверяя не только реквизит, но и
текущие значения.
====================================================================
*/
static int get_rel( int value, int limit )
{
    return 1000 * value / limit;
}
static void ai_eval_units()
{
    Unit *unit;
    list_reset( units );
    while ( ( unit = list_next( units ) ) ) {
        if ( unit->killed ) continue;
        unit->eval_score = 0;
        if ( unit->prop.ammo > 0 ) {
            if ( unit->cur_ammo >= 5 )
                /* крайне маловероятно, что будет больше
				пяти атак на юнит в течение одного хода, поэтому мы
                можно считать юнит с 5 + патронами 100% готовым к
				бою */
                unit->eval_score += 1000;
            else
                unit->eval_score += get_rel( unit->cur_ammo, 
                                             MINIMUM( 5, unit->prop.ammo ) );
        }
        if ( unit->prop.fuel > 0 ) {
            if ( ( unit_has_flag( unit->sel_prop, "flying" ) && unit->cur_fuel >= 20 ) ||
                 ( !unit_has_flag( unit->sel_prop, "flying" ) && unit->cur_fuel >= 10 ) )
                /* юнит с таким диапазоном считается работоспособным на 100%  */
                unit->eval_score += 1000;
            else {
                if ( unit_has_flag( unit->sel_prop, "flying" ) )
                    unit->eval_score += get_rel( unit->cur_fuel, MINIMUM( 20, unit->prop.fuel ) );
                else
                    unit->eval_score += get_rel( unit->cur_fuel, MINIMUM( 10, unit->prop.fuel ) );
            }
        }
        unit->eval_score += unit->exp_level * 250;
        unit->eval_score += unit->str * 200; /* сила считается удвоенной */
        /* опыт единицы не считается нормальным, но дает бонус
           это может увеличить оценку */
        unit->eval_score /= 2 + (unit->prop.ammo > 0) + (unit->prop.fuel > 0);
        /* это значение от 0 до 1000 указывает на готовность юнита
           и, следовательно, разрешение eval_score */
        unit->eval_score = unit->eval_score * unit->prop.eval_score / 1000;
    }
}

/*
====================================================================
Установите контрольную маску для земли/воздуха/моря для всех блоков. (не только
видимые) Друзья дают положительный результат, а враги
-отрицательный, который является относительным самым высоким контрольным значением и колеблется между
-1000 и 1000.
====================================================================
*/
typedef struct {
    Unit *unit;
    int score;
    int op_region; /* 0-земля, 1-воздух, 2-море */
} CtrlCtx;
static int ai_eval_ctrl( int x, int y, void *_ctx )
{
    CtrlCtx *ctx = _ctx;
    /* наша основная функция ai_get_ctrl_masks () добавляет счет
       для всех плиток в диапазоне и как мы только хотим добавить счет
       как только мы должны проверить только плитки в диапазоне атаки, которые
не находятся в диапазоне перемещения */
    if ( mask[x][y].in_range )
        return 1;
    /* хорошо, это дальность стрельбы, но не дальность перемещения */
    switch ( ctx->op_region ) {
        case 0: mask[x][y].ctrl_grnd += ctx->score; break;
        case 1: mask[x][y].ctrl_air += ctx->score; break;
        case 2: mask[x][y].ctrl_sea += ctx->score; break;
    }
    return 1;
}
void ai_get_ctrl_masks()
{
    CtrlCtx ctx;
    int i, j;
    Unit *unit;
    map_clear_mask( F_CTRL_GRND | F_CTRL_AIR | F_CTRL_SEA );
    list_reset( units );
    while ( ( unit = list_next( units ) ) ) {
        if ( unit->killed ) continue;
        map_get_unit_move_mask( unit );
        /* контекст сборки */
        ctx.unit = unit;
        ctx.score = (player_is_ally( unit->player, cur_player ))?unit->eval_score:-unit->eval_score;
        ctx.op_region = ( unit_has_flag( unit->sel_prop, "flying" ) )?1:( unit_has_flag( unit->sel_prop, "swimming" ) )?2:0;
        /* работать через маску перемещения и изменять маску ctrl, добавляя счет
           для всех плиток в движении и дальности атаки один раз */
        for ( i  = MAXIMUM( 0, unit->x - unit->cur_mov ); 
              i <= MINIMUM( map_w - 1, unit->x + unit->cur_mov );
              i++ )
            for ( j  = MAXIMUM( 0, unit->y - unit->cur_mov ); 
                  j <= MINIMUM( map_h - 1, unit->y + unit->cur_mov ); 
                  j++ )
                if ( mask[i][j].in_range ) {
                    switch ( ctx.op_region ) {
                        case 0: mask[i][j].ctrl_grnd += ctx.score; break;
                        case 1: mask[i][j].ctrl_air += ctx.score; break;
                        case 2: mask[i][j].ctrl_sea += ctx.score; break;
                    }
                    ai_eval_hexes( i, j, MAXIMUM( 1, unit->sel_prop->rng ), 
                                   ai_eval_ctrl, &ctx );
                }
    }
}

/** Найдите юнит в уменьшенном списке записей lib unit @ulist, который имеет флаг @flag set
 * и является лучшим в отношении соотношения оценки и стоимости. Возвращает NULL, если такового нет
 * единица измерения найдена. */
Unit_Lib_Entry *get_cannonfodder( List *ulist, char *flag )
{
	Unit_Lib_Entry *e, *u = NULL;
	
	list_reset(ulist);
	while ((e = list_next(ulist))) {
		if (! unit_has_flag( e, flag ))
			continue;
		if (u == NULL) {
			u = e;
			continue;
		}
		if ((100*e->eval_score)/e->cost > (100*u->eval_score)/u->cost)
			u = e;
	}
	return u;
}
	
/** Найдите юнит в уменьшенном списке записей lib unit @ulist, который имеет флаг @flag set
 * и лучше всего в отношении оценки eval слегка наказывается высокой стоимостью.
 * Возвращает NULL, если такая единица не найдена. */
Unit_Lib_Entry *get_good_unit( List *ulist, char *flag )
{
	Unit_Lib_Entry *e, *u = NULL;
	
	list_reset(ulist);
	while ((e = list_next(ulist))) {
		if (!( unit_has_flag( e, flag ) ))
			continue;
		if (u == NULL) {
			u = e;
			continue;
		}
		if (e->eval_score-(e->cost/5) > u->eval_score-(u->cost/5))
			u = e;
	}
	return u;
}

/** Покупайте новые юниты (для этого бесстыдно используйте функции из purchase_dlg).*/
extern int player_get_purchase_unit_limit( Player *player );
extern List *get_purchase_nations( void );
extern List *get_purchasable_unit_lib_entries( const char *nationid, 
				const char *uclassid, const Date *date );
extern int player_can_purchase_unit( Player *p, Unit_Lib_Entry *unit,
							Unit_Lib_Entry *trsp);
extern void player_purchase_unit( Player *player, Nation *nation, int type,
			Unit_Lib_Entry *unit_prop, Unit_Lib_Entry *trsp_prop );
static void ai_purchase_units()
{
	List *nlist = NULL; /* список стран игрока */
	List *ulist = NULL; /* список приобретаемых юнитов lib записей */
	Unit_Lib_Entry *e = NULL;
	Nation *n = NULL;
	int ulimit = player_get_purchase_unit_limit(cur_player);
	struct {
		Unit_Lib_Entry *unit; /* юнит, подлежащий покупке */
		Unit_Lib_Entry *trsp; /* его транспортер если не нулевой */
		int weight; /* относительный вес к другим юнитам в списке */
	} buy_options[5];
	int num_buy_options;
	int maxeyes, i, j;
	
	AI_DEBUG( 1, "Prestige: %d, Units available: %d\n", 
					cur_player->cur_prestige, ulimit);
	
	/* если нет емкости, вернитесь */
	if (ulimit == 0)
		return;
	
	ulist = get_purchasable_unit_lib_entries( NULL, NULL, 
						&scen_info->start_date);
	nlist = get_purchase_nations();
	
	/* удалите все записи из ulist, в которых нет ни одной из наших наций */
	list_reset(ulist);
	while ((e = list_next(ulist))) {
		if (e->nation == -1) {
			list_delete_current(ulist); /* не покупается */
			continue;
		}
		list_reset(nlist);
		while ((n = list_next(nlist)))
			if (&nations[e->nation] == n)
				break;
		if (n == NULL) {
			list_delete_current(ulist); /* не наша нация */
			continue;
		}
		AI_DEBUG(2, "%s can be purchased (score: %d, cost: %d)\n",
						e->name,e->eval_score,e->cost);
	}
	
	memset(buy_options,0,sizeof(buy_options));
	num_buy_options = 0;
	
	/* FIXME: это, конечно, должно быть более детализированным и сложным, но для
	 * теперь все будет просто:
	 * 
	 * для обороны: покупайте самую дешевую пехоту (30%), противотанковую (30%), 
	 * воздушная защита (20%) или танк (20%) 
	 * для атаки: купить хороший танк (40%), пехоту с транспортером (15%),
	 * хорошая артиллерия (15%), истребитель (10%) или бомбардировщик (20%) */
	if (cur_player->strat < 0) {
		buy_options[0].unit = get_cannonfodder( ulist, "infantry" );
		buy_options[0].weight = 30;
		buy_options[1].unit = get_cannonfodder( ulist, "anti_tank" );
		buy_options[1].weight = 30;
		buy_options[2].unit = get_cannonfodder( ulist, "air_defense" );
		buy_options[2].weight = 20;
		buy_options[3].unit = get_cannonfodder( ulist, "tank" );
		buy_options[3].weight = 20;
		num_buy_options = 4;
	} else {
		buy_options[0].unit = get_good_unit( ulist, "infantry" );
		buy_options[0].weight = 15;
		buy_options[1].unit = get_good_unit( ulist, "tank" );
		buy_options[1].weight = 40;
		buy_options[2].unit = get_good_unit( ulist, "artillery" );
		buy_options[2].weight = 15;
		buy_options[3].unit = get_good_unit( ulist, "interceptor" );
		buy_options[3].weight = 10;
		buy_options[4].unit = get_good_unit( ulist, "bomber" );
		buy_options[4].weight = 20;
		num_buy_options = 5;
	}
	
	/* получить " размер кубика" :-) */
	maxeyes = 0;
	for (i = 0; i < num_buy_options; i++) {
		maxeyes += buy_options[i].weight;
		AI_DEBUG(1, "Purchase option %d (w=%d): %s%s\n", i+1,
					buy_options[i].weight, 
					buy_options[i].unit?
					buy_options[i].unit->name:"empty",
					buy_options[i].trsp?
					buy_options[i].trsp->name:"");
		if (buy_options[i].unit == NULL) {
			/* это относится только к самым первым сценариям
			 * чтобы поймать его, просто отступите к пехоте */
			AI_DEBUG(1, "Option %d empty (use option 1)\n", i+1);
			buy_options[i].unit = buy_options[0].unit;
		}
	}
	
	/* старайтесь покупать юниты; если это невозможно (стоимость превышает престиж), ничего не делайте
	 * (=сохраните престиж и попробуйте купить следующий ход) */
	for (i = 0; i < ulimit; i++) {
		int roll = DICE(maxeyes);
		for (j = 0; j < num_buy_options; j++)
			if (roll > buy_options[j].weight)
				roll -= buy_options[j].weight;
			else 
				break;
		AI_DEBUG(1, "Choosing option %d\n",j+1);
		if (buy_options[j].unit && player_can_purchase_unit( cur_player,
						buy_options[j].unit,
						buy_options[j].trsp)) {
			player_purchase_unit( cur_player, 
                &nations[buy_options[j].unit->nation], AUXILIARY,
				buy_options[j].unit,buy_options[j].trsp);
			AI_DEBUG(1, "Prestige remaining: %d\n",
						cur_player->cur_prestige);
		} else
			AI_DEBUG(1, "Could not purchase unit?!?\n");
	}
		
	list_delete( nlist );
	list_delete( ulist );
}

/*
====================================================================
Экспорт
====================================================================
*/

/*
====================================================================
Инициировать ход
====================================================================
*/
void ai_init( void )
{
    List *list; /* используется для ускорения создания ai_units */
    Unit *unit;
	
    AI_DEBUG(0, "AI Turn: %s\n", cur_player->name );
    if ( ai_status != AI_STATUS_INIT ) {
        AI_DEBUG(0, "Aborted: Bad AI Status: %i\n", ai_status );
        return;
    }
    
    finalized = 0;
    /* получите все подразделения cur_player, те, у кого есть оборонительный огонь, идут первыми */
    list = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    list_reset( units );
    while ( ( unit = list_next( units ) ) )
        if ( unit->player == cur_player )
            list_add( list, unit );
    ai_units = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    list_reset( list );
    while ( ( unit = list_next( list ) ) )
        if ( unit_has_flag( unit->sel_prop, "artillery" ) || unit_has_flag( unit->sel_prop, "air_defense" ) ) {
            list_add( ai_units, unit );
            list_delete_item( list, unit );
        }
    list_reset( list );
    while ( ( unit = list_next( list ) ) ) {
        if ( unit->killed ) 
		AI_DEBUG( 0, "!!Unit %s is dead!!\n", unit->name );
        list_add( ai_units, unit );
    }
    list_delete( list );
    list_reset( ai_units );
    AI_DEBUG(1, "Units: %i\n", ai_units->count );
    /* оцените все юниты для стратегических вычислений */
    AI_DEBUG( 1, "Evaluating units...\n" );
    ai_eval_units();
    /* построение контрольных масок */
    AI_DEBUG( 1, "Building control mask...\n" );
    //ai_get_ctrl_masks();
    if (config.purchase == INSTANT_PURCHASE)
    {
        ai_status = deploy_turn ? AI_STATUS_DEPLOY : AI_STATUS_SUPPLY;
        AI_DEBUG( 0, "AI Initialized\n" );
        AI_DEBUG( 0, deploy_turn ? "*** DEPLOY ***\n" :
               "*** SUPPLY ***\n" );
    }
    else
    {
        /* сначала проверьте новые блоки */
        ai_status = AI_STATUS_DEPLOY; 
        list_reset( avail_units );
        AI_DEBUG( 0, "AI Initialized\n" );
        AI_DEBUG( 0, "*** DEPLOY ***\n" );
    }
}

/*
====================================================================
Очередь следующих действий (если эти действия были обработаны движком
, то эта функция вызывается снова и снова до тех
пор, пока не будет получено действие end_turn).
====================================================================
*/
void ai_run( void )
{
    Unit *partners[MAP_MERGE_UNIT_LIMIT];
    int partner_count;
    int i, j, x, y, dx, dy, dist, found;
    Unit *unit = 0, *best = 0;
    switch ( ai_status ) {
        case AI_STATUS_DEPLOY:
            /* развернуть подразделение? */
            if ( avail_units->count > 0 && ( unit = list_next( avail_units ) ) ) {
                if ( deploy_turn ) {
                    x = unit->x; y = unit->y;
                    assert( x >= 0 && y >= 0 );
                    map_remove_unit( unit );
                    found = 1;
                } else {
                    map_get_deploy_mask(cur_player,unit,0);
                    map_clear_mask( F_AUX );
                    for ( i = 1; i < map_w - 1; i++ )
                        for ( j = 1; j < map_h - 1; j++ )
                            if ( mask[i][j].deploy )
                                if ( ai_get_dist( unit, i, j, AI_FIND_ENEMY_OBJ, &x, &y, &dist ) )
                                    mask[i][j].aux = dist + 1;
                    dist = 1000; found = 0;
                    for ( i = 1; i < map_w - 1; i++ )
                        for ( j = 1; j < map_h - 1; j++ )
                            if ( mask[i][j].aux > 0 && mask[i][j].aux < dist ) {
                                dist = mask[i][j].aux;
                                x = i; y = j;
                                found = 1; /* развернитесь поближе к врагу */
                            }
                }
                if ( found ) {
                    action_queue_deploy( unit, x, y );
                    list_reset( avail_units );
                    list_add( ai_units, unit );
                    AI_DEBUG( 1, "%s deployed to %i,%i\n", unit->name, x, y );
                    return;
                }
            }
            else
            {
                if (config.purchase == INSTANT_PURCHASE)
                    if (deploy_turn)
                    {
                        ai_status = AI_STATUS_END;
                    }
                    else
                    {
                        ai_status = AI_STATUS_GROUP;
                        /* постройте группу со всеми юнитами, -1,-1 в качестве пункта назначения означает, что она будет
                           просто атакуйте / защищайте ближайшую цель. позже это должно произойти
                           разделитесь на несколько групп с разными целями и стратегией */
                        ai_group = ai_group_create( cur_player->strat, -1, -1 );
                        list_reset( ai_units );
                        while ( ( unit = list_next( ai_units ) ) )
                            ai_group_add_unit( ai_group, unit );
                    }
                else
                    if (config.merge_replacements == OPTION_MERGE)
                        ai_status = deploy_turn ? AI_STATUS_END : AI_STATUS_MERGE;
                    else
                        ai_status = deploy_turn ? AI_STATUS_END : AI_STATUS_SUPPLY;
                list_reset( ai_units );
                if (config.purchase == INSTANT_PURCHASE)
                    if (deploy_turn)
                        AI_DEBUG( 0, "*** END TURN ***\n" );
                    else
                        AI_DEBUG( 0, "*** MOVE & ATTACK ***\n" );
                else
                    if (config.merge_replacements == OPTION_MERGE)
                        AI_DEBUG( 0, deploy_turn ? "*** END TURN ***\n" : 
							"*** MERGE ***\n" );
                    else
                        AI_DEBUG( 0, deploy_turn ? "*** END TURN ***\n" :
                            "*** SUPPLY ***\n" );
            }
            break;
        case AI_STATUS_SUPPLY:
            /* получить следующий юнит */
            if ( ( unit = list_next( ai_units ) ) == 0 ) {
                if (config.purchase == INSTANT_PURCHASE)
                {
                    ai_status = AI_STATUS_PURCHASE;
                    AI_DEBUG( 0, "*** PURCHASE ***\n" );
                }
                else
                {
                    ai_status = AI_STATUS_GROUP;
                    /* постройте группу со всеми единицами, -1,-1 в качестве пункта назначения означает, что она будет
                       просто атакуйте / защищайте ближайшую цель. позже это должно произойти
                       разделитесь на несколько групп с разными целями и стратегией */
                    ai_group = ai_group_create( cur_player->strat, -1, -1 );
                    list_reset( ai_units );
                    while ( ( unit = list_next( ai_units ) ) )
                        ai_group_add_unit( ai_group, unit );
                    AI_DEBUG( 0, "*** MOVE & ATTACK ***\n" );
                }
            }
            else {
                /* проверьте, нуждается ли юнит в снабжении и удалите
его из ai_units, если это так */
                if (config.merge_replacements == OPTION_REPLACEMENTS)
                {
                    if ( unit_check_replacements(unit, ELITE_REPLACEMENTS) )
                    {
                        if (unit_get_replacement_strength( unit, ELITE_REPLACEMENTS ) > 2 ||
                            !unit_check_replacements(unit, REPLACEMENTS))
                        {
                            action_queue_elite_replace( unit );
                            AI_DEBUG( 1, "%s gets elite replacements\n", unit->name );
                            list_delete_item( ai_units, unit );
                        }
                    }
                    else if ( unit_check_replacements(unit, REPLACEMENTS) )
                    {
                        if (unit_get_replacement_strength( unit, REPLACEMENTS ) > 2 )
                        {
                            action_queue_replace( unit );
                            AI_DEBUG( 1, "%s gets replacements\n", unit->name );
                            list_delete_item( ai_units, unit );
                        }
                    }
                }
                if ( ( unit_low_fuel( unit ) || unit_low_ammo( unit ) ) ) {
                    if ( unit->supply_level > 0 ) {
                        action_queue_supply( unit );
                        list_delete_item( ai_units, unit );
                        AI_DEBUG( 1, "%s supplies\n", unit->name );
                        break;
                    }
                    else {
                        AI_DEBUG( 1, "%s searches depot\n", unit->name );
                        if ( ai_get_dist( unit, unit->x, unit->y, AI_FIND_DEPOT,
                                          &dx, &dy, &dist ) )
                            if ( ai_approximate( unit, dx, dy, &x, &y ) ) {
                                action_queue_move( unit, x, y );
                                list_delete_item( ai_units, unit );
                                AI_DEBUG( 1, "%s moves to %i,%i\n", unit->name, x, y );
                                break;
                            }
                    }
                }
            }
            break;
        case AI_STATUS_MERGE:
            if ( ( unit = list_next( ai_units ) ) ) {
                map_get_merge_units( unit, partners, &partner_count );
                best = 0; /* слейтесь с тем, у кого больше всего очков силы */
                for ( i = 0; i < partner_count; i++ )
                    if ( best == 0 )
                        best = partners[i];
                    else
                        if ( best->str < partners[i]->str )
                            best = partners[i];
                if ( best ) {
                    AI_DEBUG( 1, "%s merges with %s\n", unit->name, best->name );
                    action_queue_merge( unit, best );
                    /* оба юнита уже обработаны */
                    list_delete_item( ai_units, unit );
                    list_delete_item( ai_units, best ); 
                }
            }
            else {
                ai_status = AI_STATUS_SUPPLY;
                list_reset( ai_units );
                AI_DEBUG( 0, "*** SUPPLY ***\n" );
            }
            break;
        case AI_STATUS_GROUP:
            if ( !ai_group_handle_next_unit( ai_group ) ) {
                ai_group_delete( ai_group );
                if (config.purchase == DELAYED_PURCHASE)
                {
                    ai_status = AI_STATUS_PURCHASE;
                    AI_DEBUG( 0, "*** PURCHASE ***\n" );
                } else {
                    ai_status = AI_STATUS_END;
                    AI_DEBUG( 0, "*** END TURN ***\n" );
                }
            }
            break;
        case AI_STATUS_PURCHASE:
            ai_purchase_units();
            if (config.purchase == INSTANT_PURCHASE)
            {
                /* сначала проверьте новые юниты */
                ai_status = AI_STATUS_DEPLOY;
                list_reset( avail_units );
                AI_DEBUG( 0, "AI Initialized\n" );
                AI_DEBUG( 0, "*** DEPLOY ***\n" );
            }
            else
            {
                ai_status = AI_STATUS_END;
                AI_DEBUG( 0, "*** END TURN ***\n" );
            }
            break;
        case AI_STATUS_END:
            action_queue_end_turn();
            ai_status = AI_STATUS_FINALIZE;
            break;
    }
}

/*
====================================================================
Отменить шаги (например, выделение памяти), выполненные в ai_init()
====================================================================
*/
void ai_finalize( void )
{
    AI_DEBUG(2, "%s entered\n",__FUNCTION__);
    if ( finalized )
        return;
    AI_DEBUG(2, "Really finalized\n");
    list_delete( ai_units );
    AI_DEBUG( 0, "AI Finalized\n" );
    ai_status = AI_STATUS_INIT;
    finalized = 1;
}
