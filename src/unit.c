/***************************************************************************
                          unit.c  -  description
                             -------------------
    begin                : Fri Jan 19 2001
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
#include "list.h"
#include "unit.h"
#include "localize.h"
#include "map.h"
#include "campaign.h"

/*
====================================================================
Внешние
====================================================================
*/
extern int scen_get_weather( void );
extern Sdl sdl;
extern Config config;
extern int trgt_type_count;
extern List *vis_units; /* юниты, известные до текущего игрока */
extern List *units;
extern int cur_weather;
extern Weather_Type *weather_types;
extern Unit *cur_target;
extern Map_Tile **map;
extern int camp_loaded;
extern Camp_Entry *camp_cur_scen;
extern Player *cur_player;

/*
====================================================================
Лоальные
====================================================================
*/

//#define DEBUG_ATTACK

/*
====================================================================
Обновить информацию на панели юнита в соответствии с силой.
====================================================================
*/
static void update_bar( Unit *unit )
{
    /* ширина полосы */
    unit->damage_bar_width = unit->str * BAR_TILE_WIDTH;
    if ( unit->damage_bar_width == 0 && unit->str > 0 )
        unit->damage_bar_width = BAR_TILE_WIDTH;
    /* цвет полосы определяется вертикальным смещением в map->life_icons */
    if ( unit->str > 4 )
        unit->damage_bar_offset = 0;
    else
        if ( unit->str > 2 )
            unit->damage_bar_offset = BAR_TILE_HEIGHT;
        else
            unit->damage_bar_offset = BAR_TILE_HEIGHT * 2;
}

/*
====================================================================
Получите текущую силу юнита, которая составляет:
  max { 0, unit->str - unit->suppr - unit->turn_suppr }
====================================================================
*/
static int unit_get_cur_str( Unit *unit )
{
    int cur_str = unit->str - unit->suppr - unit->turn_suppr;
    if ( cur_str < 0 ) cur_str = 0;
    return cur_str;
}

/*
====================================================================
Примените подавление и урон к юниту. Верните оставшиеся
фактическая сила.
Если нападающий - бомбардировщик, подавление засчитывается как ход
подавление.
====================================================================
*/
static int unit_apply_damage( Unit *unit, int damage, int suppr, Unit *attacker )
{
    unit->str -= damage;
    if ( unit->str < 0 ) {
        unit->str = 0;
        return 0;
    }
    if ( attacker && unit_has_flag( attacker->sel_prop, "turn_suppr" ) ) {
        unit->turn_suppr += suppr;
        if ( unit->str - unit->turn_suppr - unit->suppr < 0 ) {
            unit->turn_suppr = unit->str - unit->suppr;
            return 0;
        }
    }
    else {
        unit->suppr += suppr;
        if ( unit->str - unit->turn_suppr - unit->suppr < 0 ) {
            unit->suppr = unit->str - unit->turn_suppr;
            return 0;
        }
    }
    return unit_get_cur_str( unit );
}

/** Добавить престиж для игрока юнита @unit в зависимости от свойств атакованного
 * юнит @target и нанесенный ему урон @target_dam. */
static void unit_gain_prestige( Unit *unit, const Unit *target, int target_dam )
{
	int gain = 0;

	/* Я немного поигрался с PG, но не понял, как
	* формула работает. Некоторые моменты, кажется, верны:
	* - собственный урон / потеря юнита не дает штрафа престижа
	* (поэтому мы не рассматриваем здесь урон юнитов)
	* - если цель повреждена, но не уничтожена, то случайный престиж
	* рассчитывается исходя из стоимости, опыта и урона
	* - если цель уничтожена, то престиж увеличивается в зависимости от
	* стоимость и опыт с небольшой случайной модификацией
	*
	* FIXME: формула, используемая при уничтожении юнита, кажется вполне
	* хорошо, но тот, что наносит простой урон, не очень, особенно когда
	* опыт увеличивается: бросание кубика больше раз не увеличивает
	* шанс очень велик для недорогих юнитов, поэтому мы даем меньше престижа для
	* дешевые, опытные агрегаты по сравнению с PG ... */

	if (target->str == 0) {
		/* если юнит уничтожен: ((1/24 of cost) * exp) +- 10% */
		gain = ((target->exp_level + 1) * target->prop.cost * 10 +
							120/*округлять*/) / 240;
		gain = ((111 - DICE(21)) * gain) / 100;
		if (gain == 0)
			gain = 1;
	} else {
		/* если поврежден: попробуйте половину урона, умноженную на броски опыта
		* на 50 кубиков против одной десятой стоимости. максимум четыре раза
		* опыт можно получить */
		int throws = ((target_dam + 1) / 2) * (target->exp_level+1);
		int i, limit = 4 * (target->exp_level+1) - 1;
		for (i = 0; i < throws; i++)
			if( DICE(50) < target->prop.cost/10 ) {
				gain++;
				if (gain == limit)
					break;
			}
	}

	unit->player->cur_prestige += gain;
	//printf("%s attacked %s (%d damage%s): prestige +%d\n",
	//			unit->name, target->name, target_dam,
	//			(target->str==0)?", killed":"", gain);
}

/*
====================================================================
Проведите одиночный бой (без проверки оборонительного огня) со случайным
ценности. (только если установлено "удача")
Если установлен параметр force_rugged. Надежная защита будет вынуждена.
====================================================================
*/
enum {
    ATK_BOTH_STRIKE = 0,
    ATK_UNIT_FIRST,
    ATK_TARGET_FIRST,
    ATK_NO_STRIKE
};
static int unit_attack( Unit *unit, Unit *target, int type, int real, int force_rugged )
{
    int unit_old_str = unit->str;//, target_old_str = target->str;
    int unit_old_ini = unit->sel_prop->ini, target_old_ini = target->sel_prop->ini;
    int unit_dam = 0, unit_suppr = 0, target_dam = 0, target_suppr = 0;
    int rugged_chance, rugged_def = 0;
    int exp_mod;
    int ret = AR_NONE; /* очистить флаги */
    int strike;
    /* проверьте, есть ли надежная защита */
    if ( real && type == UNIT_ACTIVE_ATTACK )
        if ( unit_check_rugged_def( unit, target ) || ( force_rugged && unit_is_close( unit, target ) ) ) {
            rugged_chance = unit_get_rugged_def_chance( unit, target );
            if ( DICE(100) <= rugged_chance || force_rugged )
                rugged_def = 1;
        }
    /* Формула инициативы PG:
       min { base initiative, terrain max initiative } +
       ( exp_level + 1 ) / 2 + D3 */
    /* против самолетов используется инициатива, так как местность не имеет значения */
    /* местность цели используется для боя */
    if ( !unit_has_flag( unit->sel_prop, "flying" ) && !unit_has_flag( target->sel_prop, "flying" ) )
    {
        unit->sel_prop->ini = MINIMUM( unit->sel_prop->ini, target->terrain->max_ini );
        target->sel_prop->ini = MINIMUM( target->sel_prop->ini, target->terrain->max_ini );
    }
    unit->sel_prop->ini += ( unit->exp_level + 1  ) / 2;
    target->sel_prop->ini += ( target->exp_level + 1  ) / 2;
    /* правила особой инициативы:
       противотанковый атакующий танк | разведка: atk 0, def 99
       танк в атаке против антитанк: atk 0, def 99
       защитный огонь: atk 99, def 0
       подводные атаки: atk 99, def 0
       дальняя атака: atk 99, def 0
       прочная защита: atk 0
       авиация атакует противовоздушную оборону: atk = def
       не-искусство против искусства: atk 0, def 99 */
    if ( unit_has_flag( unit->sel_prop, "anti_tank" ) )
        if ( unit_has_flag( target->sel_prop, "tank" ) ) {
            unit->sel_prop->ini = 0;
            target->sel_prop->ini = 99;
        }
    if ( unit_has_flag( unit->sel_prop, "diving" ) ||
         unit_has_flag( unit->sel_prop, "artillery" ) ||
         unit_has_flag( unit->sel_prop, "air_defense" ) ||
         type == UNIT_DEFENSIVE_ATTACK
    ) {
        unit->sel_prop->ini = 99;
        target->sel_prop->ini = 0;
    }
    if ( unit_has_flag( unit->sel_prop, "flying" ) )
        if ( unit_has_flag( target->sel_prop, "air_defense" ) )
            unit->sel_prop->ini = target->sel_prop->ini;
    if ( rugged_def )
        unit->sel_prop->ini = 0;
    if ( force_rugged )
        target->sel_prop->ini = 99;
    /* кости бросаются после этих изменений */
    if ( real ) {
        unit->sel_prop->ini += DICE(3);
        target->sel_prop->ini += DICE(3);
    }
#ifdef DEBUG_ATTACK
    if ( real ) {
        printf( "%s Initiative: %i\n", unit->name, unit->sel_prop->ini );
        printf( "%s Initiative: %i\n", target->name, target->sel_prop->ini );
        if ( unit_check_rugged_def( unit, target ) )
            printf( "\nRugged Defense: %s (%i%%)\n",
                    rugged_def ? "yes" : "no",
                    unit_get_rugged_def_chance( unit, target ) );
    }
#endif
    /* в реальном бою подводная лодка может уклониться */
    if ( real && type == UNIT_ACTIVE_ATTACK && unit_has_flag( target->sel_prop, "diving" ) ) {
        if ( DICE(10) <= 7 + ( target->exp_level - unit->exp_level ) / 2 )
        {
            strike = ATK_NO_STRIKE;
            ret |= AR_EVADED;
        }
        else
            strike = ATK_UNIT_FIRST;
#ifdef DEBUG_ATTACK
        printf ( "\nSubmarine Evasion: %s (%i%%)\n",
                 (strike==ATK_NO_STRIKE)?"yes":"no",
                 10 * (7 + ( target->exp_level - unit->exp_level ) / 2) );
#endif
    }
    else
    /* кто первый? */
    if ( unit->sel_prop->ini == target->sel_prop->ini )
        strike = ATK_BOTH_STRIKE;
    else
        if ( unit->sel_prop->ini > target->sel_prop->ini )
            strike = ATK_UNIT_FIRST;
        else
            strike = ATK_TARGET_FIRST;
    /* тот, у кого наибольшая инициатива, начинает первым, если не оборонительный огонь или артиллерия */
    if ( strike == ATK_BOTH_STRIKE ) {
        /* оба бьют одновременно */
        unit_get_damage( unit, unit, target, type, real, rugged_def, &target_dam, &target_suppr );
        if ( unit_check_attack( target, unit, UNIT_PASSIVE_ATTACK ) )
            unit_get_damage( unit, target, unit, UNIT_PASSIVE_ATTACK, real, rugged_def, &unit_dam, &unit_suppr );
        unit_apply_damage( target, target_dam, target_suppr, unit );
        unit_apply_damage( unit, unit_dam, unit_suppr, target );
    }
    else
        if ( strike == ATK_UNIT_FIRST ) {
            /* юнит наносит удар первым */
            unit_get_damage( unit, unit, target, type, real, rugged_def, &target_dam, &target_suppr );
            if ( unit_apply_damage( target, target_dam, target_suppr, unit ) )
                if ( unit_check_attack( target, unit, UNIT_PASSIVE_ATTACK ) && type != UNIT_DEFENSIVE_ATTACK ) {
                    unit_get_damage( unit, target, unit, UNIT_PASSIVE_ATTACK, real, rugged_def, &unit_dam, &unit_suppr );
                    unit_apply_damage( unit, unit_dam, unit_suppr, target );
                }
        }
        else
            if ( strike == ATK_TARGET_FIRST ) {
                /* цель поражает первой */
                if ( unit_check_attack( target, unit, UNIT_PASSIVE_ATTACK ) ) {
                    unit_get_damage( unit, target, unit, UNIT_PASSIVE_ATTACK, real, rugged_def, &unit_dam, &unit_suppr );
                    if ( !unit_apply_damage( unit, unit_dam, unit_suppr, target ) )
                        ret |= AR_UNIT_ATTACK_BROKEN_UP;
                }
                if ( unit_get_cur_str( unit ) > 0 ) {
                    unit_get_damage( unit, unit, target, type, real, rugged_def, &target_dam, &target_suppr );
                    unit_apply_damage( target, target_dam, target_suppr, unit );
                }
            }
    /* проверить возвращаемое значение */
    if ( unit->str == 0 )
        ret |= AR_UNIT_KILLED;
    else
        if ( unit_get_cur_str( unit ) == 0 )
            ret |= AR_UNIT_SUPPRESSED;
    if ( target->str == 0 )
        ret |= AR_TARGET_KILLED;
    else
        if ( unit_get_cur_str( target ) == 0 )
            ret |= AR_TARGET_SUPPRESSED;
    if ( rugged_def )
        ret |= AR_RUGGED_DEFENSE;
    if ( real ) {
        /* стоимость боеприпасов */
        if ( config.supply ) {
            //if (DICE(10)<=target_old_str)
                unit->cur_ammo--;
            if ( unit_check_attack( target, unit, UNIT_PASSIVE_ATTACK ) && target->cur_ammo > 0 )
                //if (DICE(10)<=unit_old_str)
                    target->cur_ammo--;
        }
        /* стоимость атаки */
        if ( unit->cur_atk_count > 0 ) unit->cur_atk_count--;
        /* цель: ослабить окоп, если был получен урон или с unit->str*10% chance */
        if ( target->entr > 0 )
            if (target_dam > 0 || DICE(10)<=unit_old_str)
                target->entr--;
        /* злоумышленник теряет окоп, если он получил травму */
        if ( unit->entr > 0 && unit_dam > 0 )
            unit->entr--;
        /* получить опыт */
        exp_mod = target->exp_level - unit->exp_level;
        if ( exp_mod < 1 ) exp_mod = 1;
        unit_add_exp( unit, exp_mod * target_dam + unit_dam );
        exp_mod = unit->exp_level - target->exp_level;
        if ( exp_mod < 1 ) exp_mod = 1;
        unit_add_exp( target, exp_mod * unit_dam + target_dam );
        if ( unit_is_close( unit, target ) ) {
            unit_add_exp( unit, 10 );
            unit_add_exp( target, 10 );
        }
	/* завоевать престиж */
	unit_gain_prestige( unit, target, target_dam );
	unit_gain_prestige( target, unit, unit_dam );
        /* настроить планку жизни */
        update_bar( unit );
        update_bar( target );
    }
    unit->sel_prop->ini = unit_old_ini;
    target->sel_prop->ini = target_old_ini;
    return ret;
}

/*
====================================================================
Публичные
====================================================================
*/

/*
====================================================================
Создайте единицу, передав структуру единицы со следующим набором материалов:
  name, x, y, str, entr, exp_level, delay, orient, nation, player.
Эта функция будет использовать переданные значения для создания структуры Unit
со всеми установленными значениями.
====================================================================
*/
Unit *unit_create( Unit_Lib_Entry *prop, Unit_Lib_Entry *trsp_prop,
                   Unit_Lib_Entry *land_trsp_prop, Unit *base )
{
    Unit *unit = 0;
    if ( prop == 0 ) return 0;
    unit = calloc( 1, sizeof( Unit ) );
    /* неглубокая копия собственности */
    memcpy( &unit->prop, prop, sizeof( Unit_Lib_Entry ) );
    unit->sel_prop = &unit->prop;
    unit->embark = EMBARK_NONE;
    /* назначить пропущенный транспортер без проверки */
    if ( trsp_prop && !unit_has_flag( prop, "flying" ) && !unit_has_flag( prop, "swimming" ) ) {
        memcpy( &unit->trsp_prop, trsp_prop, sizeof( Unit_Lib_Entry ) );
        /* по умолчанию активен наземный транспортер море / воздух */
        if ( unit_has_flag( trsp_prop, "swimming" ) ) {
            unit->embark = EMBARK_SEA;
            unit->sel_prop = &unit->trsp_prop;
            if ( land_trsp_prop )
                memcpy( &unit->land_trsp_prop, land_trsp_prop, sizeof( Unit_Lib_Entry ) );
        }
        if ( unit_has_flag( trsp_prop, "flying" ) ) {
            unit->embark = EMBARK_AIR;
            unit->sel_prop = &unit->trsp_prop;
        }
    }
    /* скопировать базовые значения */
    unit->delay = base->delay;
    unit->x = base->x; unit->y = base->y;
    unit->str = base->str; unit->entr = base->entr;
    unit->max_str = base->max_str;
//    unit->cur_str_repl = 0;
    unit->player = base->player;
    unit->nation = base->nation;
    strcpy_lt( unit->name, base->name, 20 );
    unit_add_exp( unit, base->exp_level * 100 );
    unit->orient = base->orient;
    unit_adjust_icon( unit );
    unit->unused = 1;
    unit->supply_level = 100;
    unit->cur_ammo = unit->prop.ammo;
    unit->cur_fuel = unit->prop.fuel;
    unit->core = base->core;
    if ( unit->cur_fuel == 0 && unit->trsp_prop.id && unit->trsp_prop.fuel > 0 )
        unit->cur_fuel = unit->trsp_prop.fuel;
    strcpy_lt( unit->tag, base->tag, 31 );
    /* обновить свойства индикатора жизни */
    update_bar( unit );
    /* выделить резервную память */
    unit->backup = calloc( 1, sizeof( Unit ) );
    return unit;
}

/*
====================================================================
Удалить объект. Передайте указатель как void *, чтобы разрешить использование как
обратный вызов для списка.
====================================================================
*/
void unit_delete( void *ptr )
{
    Unit *unit = (Unit*)ptr;
    if ( unit == 0 ) return;
    if ( unit->backup ) free( unit->backup );
    free( unit );
}

/*
====================================================================
Дайте единице общее имя.
====================================================================
*/
void unit_set_generic_name( Unit *unit, int number, const char *stem )
{
    char numbuf[8];

    locale_write_ordinal_number(numbuf, sizeof numbuf, number);
    snprintf(unit->name, 24, "%s %s", numbuf, stem);
}

/*
====================================================================
Обновите значок объекта в соответствии с его ориентацией.
====================================================================
*/
void unit_adjust_icon( Unit *unit )
{
    //unit->icon_offset = unit->prop.icon_w * unit->orient;
    //unit->icon_tiny_offset = unit->prop.icon_tiny_w * unit->orient;
    unit->icon_offset = unit->sel_prop->icon_w * unit->orient;
    unit->icon_tiny_offset = unit->sel_prop->icon_tiny_w * unit->orient;
}

/*
====================================================================
Отрегулируйте ориентацию (и настройте значок) устройства, если смотреть в направлении x, y.
====================================================================
*/
void unit_adjust_orient( Unit *unit, int x, int y )
{
    if ( unit->prop.icon_type != UNIT_ICON_ALL_DIRS ) {
        if ( x < unit->x )  {
            unit->orient = UNIT_ORIENT_LEFT;
            //unit->icon_offset = unit->prop.icon_w;
            //unit->icon_tiny_offset = unit->prop.icon_tiny_w;
            unit->icon_offset = unit->sel_prop->icon_w;
            unit->icon_tiny_offset = unit->sel_prop->icon_tiny_w;
        }
        else
            if ( x > unit->x ) {
                unit->orient = UNIT_ORIENT_RIGHT;
                unit->icon_offset = 0;
                unit->icon_tiny_offset = 0;
            }
    }
    else {
        /* еще не реализовано */
    }
}

/*
====================================================================
Проверьте, может ли установка что-то подавать (боеприпасы, топливо, что-нибудь) и
вернуть сумму, которая может быть предоставлена.
====================================================================
*/
int unit_check_supply( Unit *unit, int type, int *missing_ammo, int *missing_fuel )
{
    int ret = 0;
    int max_fuel = unit->sel_prop->fuel;
    if ( missing_ammo )
        *missing_ammo = 0;
    if ( missing_fuel )
        *missing_fuel = 0;
    /* нет рядом или уже переехали? */
    if ( unit->embark == EMBARK_SEA || unit->embark == EMBARK_AIR ) return 0;
    if ( unit->supply_level == 0 ) return 0;
    if ( !unit->unused ) return 0;
    /* поставлять боеприпасы? */
    if ( type == UNIT_SUPPLY_AMMO || type == UNIT_SUPPLY_ANYTHING )
        if ( unit->cur_ammo < unit->prop.ammo ) {
            ret = 1;
            if ( missing_ammo )
                *missing_ammo = unit->prop.ammo - unit->cur_ammo;
        }
    if ( type == UNIT_SUPPLY_AMMO ) return ret;
    /* если у нас назначен наземный транспортер, нам нужно использовать его топливо как макс. */
    if ( unit_check_fuel_usage( unit ) && max_fuel == 0 )
        max_fuel = unit->trsp_prop.fuel;
    /* подавать топливо? */
    if ( type == UNIT_SUPPLY_FUEL || type == UNIT_SUPPLY_ANYTHING )
        if ( unit->cur_fuel < max_fuel ) {
            ret = 1;
            if ( missing_fuel )
                *missing_fuel = max_fuel - unit->cur_fuel;
        }
    return ret;
}

/*
====================================================================
Процент подачи максимального количества топлива / боеприпасов /both.
_intern не блокирует движение и т. д.
Верните True, если модуль был поставлен.
====================================================================
*/
int unit_supply_intern( Unit *unit, int type )
{
    int amount_ammo, amount_fuel, max, supply_amount;
    int supplied = 0;
    /* боеприпасы */
    if ( type == UNIT_SUPPLY_AMMO || type == UNIT_SUPPLY_ALL )
    if ( unit_check_supply( unit, UNIT_SUPPLY_AMMO, &amount_ammo, &amount_fuel ) ) {
        max = unit->cur_ammo + amount_ammo ;
        supply_amount = unit->supply_level * max / 100;
        if ( supply_amount == 0 ) supply_amount = 1; /* хотя бы один */
        unit->cur_ammo += supply_amount;
        if ( unit->cur_ammo > max ) unit->cur_ammo = max;
        supplied = 1;
    }
    /* топливо */
    if ( type == UNIT_SUPPLY_FUEL || type == UNIT_SUPPLY_ALL )
    if ( unit_check_supply( unit, UNIT_SUPPLY_FUEL, &amount_ammo, &amount_fuel ) ) {
        max = unit->cur_fuel + amount_fuel ;
        supply_amount = unit->supply_level * max / 100;
        if ( supply_amount == 0 ) supply_amount = 1; /* хотя бы один */
        unit->cur_fuel += supply_amount;
        if ( unit->cur_fuel > max ) unit->cur_fuel = max;
        supplied = 1;
    }
    return supplied;
}
int unit_supply( Unit *unit, int type )
{
    int supplied = unit_supply_intern(unit,type);
    if (supplied) {
        /* никакие другие действия не разрешены */
        unit->unused = 0; unit->cur_mov = 0; unit->cur_atk_count = 0;
    }
    return supplied;
}

/*
====================================================================
Проверить, использует ли объект топливо в текущем состоянии (на борту или нет).
====================================================================
*/
int unit_check_fuel_usage( Unit *unit )
{
    if ( unit->embark == EMBARK_SEA || unit->embark == EMBARK_AIR ) return 0;
    if ( unit->prop.fuel > 0 ) return 1;
    if ( unit->trsp_prop.id && unit->trsp_prop.fuel > 0 ) return 1;
    return 0;
}

/*
====================================================================
Добавьте опыт и вычислите уровень опыта.
Верните True, если levelup.
====================================================================
*/
int unit_add_exp( Unit *unit, int exp )
{
    int old_level = unit->exp_level;
    unit->exp += exp;
    if ( unit->exp >= 500 ) unit->exp = 500;
    unit->exp_level = unit->exp / 100;
    if ( old_level < unit->exp_level && config.use_core_units && (camp_loaded != NO_CAMPAIGN) && unit->core)
    {
        int i = old_level;
        while ( i < unit->exp_level ) {
            strcpy_lt(unit->star[i], camp_cur_scen->scen, 20);
            i++;
        }
    }
    return ( old_level < unit->exp_level );
}

/*
====================================================================
Установить / размонтировать блок на наземном транспортере.
====================================================================
*/
void unit_mount( Unit *unit )
{
    if ( unit->trsp_prop.id == 0 || unit->embark != EMBARK_NONE ) return;
    /* установить указатель свойства */
    unit->sel_prop = &unit->trsp_prop;
    unit->embark = EMBARK_GROUND;
    /* отрегулировать смещение изображения */
    unit_adjust_icon( unit );
    /* без окопов при монтаже */
    unit->entr = 0;
}
void unit_unmount( Unit *unit )
{
    if ( unit->embark != EMBARK_GROUND ) return;
    /* установить указатель свойства */
    unit->sel_prop = &unit->prop;
    unit->embark = EMBARK_NONE;
    /* отрегулировать смещение изображения */
    unit_adjust_icon( unit );
    /* без окопов при монтаже */
    unit->entr = 0;
}

/*
====================================================================
Проверьте, находятся ли блоки близко друг к другу. Это значит на соседних
гексах плитки.
====================================================================
*/
int unit_is_close( Unit *unit, Unit *target )
{
    return is_close( unit->x, unit->y, target->x, target->y );
}

/*
====================================================================
Проверить, может ли отряд активно атаковать (атака, инициированная отрядом) или
пассивная атака (атака по инициированной цели, защита юнита) цель.
====================================================================
*/
int unit_check_attack( Unit *unit, Unit *target, int type )
{
    if ( target == 0 || unit == target ) return 0;
    if ( player_is_ally( unit->player, target->player ) ) return 0;
    if ( unit_has_flag( unit->sel_prop, "flying" ) && !unit_has_flag( target->sel_prop, "flying" ) )
        if ( unit->sel_prop->rng == 0 )
            if ( unit->x != target->x || unit->y != target->y )
                return 0; /* диапазон 0 означает выше единицы для самолета */
    /* если цель летит и юнит находится на земле с дальностью 0, самолет
       может быть поврежден только когда над юнитом */
    if ( !unit_has_flag( unit->sel_prop, "flying" ) && unit_has_flag( target->sel_prop, "flying" ) )
        if ( unit->sel_prop->rng == 0 )
            if ( unit->x != target->x || unit->y != target->y )
                return 0;
    /* только эсминцы могут нанести вред подводным лодкам */
    if ( unit_has_flag( target->sel_prop, "diving" ) && !unit_has_flag( unit->sel_prop, "destroyer" ) ) return 0;
    if ( weather_types[cur_weather].flags & NO_AIR_ATTACK ) {
        if ( unit_has_flag( unit->sel_prop, "flying" ) ) return 0;
        if ( unit_has_flag( target->sel_prop, "flying" ) ) return 0;
    }
    if ( type == UNIT_ACTIVE_ATTACK ) {
        /* агрессор */
        if ( unit->cur_ammo <= 0 ) return 0;
        if ( unit->sel_prop->atks[target->sel_prop->trgt_type] <= 0 ) return 0;
        if ( unit->cur_atk_count == 0 ) return 0;
        if ( !unit_is_close( unit, target ) && get_dist( unit->x, unit->y, target->x, target->y ) > unit->sel_prop->rng ) return 0;
    }
    else
    if ( type == UNIT_DEFENSIVE_ATTACK ) {
        /* оборонительный огонь */
        if ( unit->sel_prop->atks[target->sel_prop->trgt_type] <= 0 ) return 0;
        if ( unit->cur_ammo <= 0 ) return 0;
        if ( !unit_has_flag( unit->sel_prop, "interceptor" ) && !unit_has_flag( unit->sel_prop, "artillery" ) &&
             !unit_has_flag( unit->sel_prop, "air_defense" ) ) return 0;
        if ( unit_has_flag( target->sel_prop, "artillery" ) || unit_has_flag( target->sel_prop, "air_defense" ) ||
             unit_has_flag( target->sel_prop, "swimming" ) ) return 0;
        if ( unit_has_flag( unit->sel_prop, "interceptor" ) ) {
            /* перехватчик, вероятно, не находится рядом с атакующим, поэтому проверка дальности отличается
             * не может быть выполнено здесь, потому что юнит, которого атакует цель, не прошел, поэтому
             * unit_get_df_units () должен иметь внешний вид
             */
        }
        else
            if ( get_dist( unit->x, unit->y, target->x, target->y ) > unit->sel_prop->rng ) return 0;
    }
    else {
        /* контр-атака */
        if ( unit->cur_ammo <= 0 ) return 0;
        if ( !unit_is_close( unit, target ) && get_dist( unit->x, unit->y, target->x, target->y ) > unit->sel_prop->rng ) return 0;
        if ( unit->sel_prop->atks[target->sel_prop->trgt_type] == 0 ) return 0;
        /* артиллерия может защищаться только от близких частей */
        if ( unit_has_flag( unit->sel_prop, "artillery" ) )
            if ( !unit_is_close( unit, target ) )
                return 0;
        /* вы можете защищаться от артиллерии только когда близко */
        if ( unit_has_flag( target->sel_prop, "artillery" ) )
            if ( !unit_is_close( unit, target ) )
                return 0;
    }
    return 1;
}

/*
====================================================================
Вычислить урон / подавление, которое получает цель при атаке юнита
цель. Никакие свойства не будут изменены. Если установлено значение "реальный"
кости брошены, иначе это стохастический прогноз.
«агрессор» - это юнит, инициировавший атаку, либо «юнит»
или «цель». Это не всегда «юнит», поскольку «юнит» и «цель»
переключается на get_damage в зависимости от того, есть ли поражающий
назад и у кого была самая высокая инициатива.
====================================================================
*/
void unit_get_damage( Unit *aggressor, Unit *unit, Unit *target,
                      int type,
                      int real, int rugged_def,
                      int *damage, int *suppr )
{
    int atk_strength, max_roll, min_roll, die_mod;
    int atk_grade, def_grade, diff, result;
    float suppr_chance, kill_chance;
    /* используйте формулу PG для вычисления степени атаки / защиты*/
    /* базовая атака */
    atk_grade = abs( unit->sel_prop->atks[target->sel_prop->trgt_type] );
#ifdef DEBUG_ATTACK
    if ( real ) printf( "\n%s attacks:\n", unit->name );
    if ( real ) printf( "  base:   %2i\n", atk_grade );
    if ( real ) printf( "  exp:    +%i\n", unit->exp_level);
#endif
    /* опыт */
    atk_grade += unit->exp_level;
    /* цель на реке? */
    if ( !unit_has_flag( target->sel_prop, "flying" ) )
    if ( target->terrain->flags[cur_weather] & RIVER ) {
        atk_grade += 4;
#ifdef DEBUG_ATTACK
        if ( real ) printf( "  river:  +4\n" );
#endif
    }
    /* контратака прочной защиты? */
    if ( type == UNIT_PASSIVE_ATTACK && rugged_def ) {
        atk_grade += 4;
#ifdef DEBUG_ATTACK
            if ( real ) printf( "  rugdef: +4\n" );
#endif
    }
#ifdef DEBUG_ATTACK
    if ( real ) printf( "---\n%s defends:\n", target->name );
#endif
    /* основная защита */
    if ( unit_has_flag( unit->sel_prop, "flying" ) )
        def_grade = target->sel_prop->def_air;
    else {
        def_grade = target->sel_prop->def_grnd;
        /* применять ближнюю оборону? */
        if ( unit_has_flag( unit->sel_prop, "infantry" ) )
            if ( !unit_has_flag( target->sel_prop, "infantry" ) )
                if ( !unit_has_flag( target->sel_prop, "flying" ) )
                    if ( !unit_has_flag( target->sel_prop, "swimming" ) )
                    {
                        if ( target == aggressor )
                        if ( unit->terrain->flags[cur_weather]&INF_CLOSE_DEF )
                            def_grade = target->sel_prop->def_cls;
                        if ( unit == aggressor )
                        if ( target->terrain->flags[cur_weather]&INF_CLOSE_DEF )
                            def_grade = target->sel_prop->def_cls;
                    }
    }
#ifdef DEBUG_ATTACK
    if ( real ) printf( "  base:   %2i\n", def_grade );
    if ( real ) printf( "  exp:    +%i\n", target->exp_level );
#endif
    /* опыт */
    def_grade += target->exp_level;
    /* нападающий на реке или болоте? */
    if ( !unit_has_flag( unit->sel_prop, "flying" ) )
    if ( !unit_has_flag( unit->sel_prop, "swimming" ) )
    if ( !unit_has_flag( target->sel_prop, "flying" ) )
    {
        if ( unit->terrain->flags[cur_weather] & SWAMP )
        {
            def_grade += 4;
#ifdef DEBUG_ATTACK
            if ( real ) printf( "  swamp:  +4\n" );
#endif
        } else
        if ( unit->terrain->flags[cur_weather] & RIVER ) {
            def_grade += 4;
#ifdef DEBUG_ATTACK
            if ( real ) printf( "  river:  +4\n" );
#endif
        }
    }
    /* прочная защита? */
    if ( type == UNIT_ACTIVE_ATTACK && rugged_def ) {
        def_grade += 4;
#ifdef DEBUG_ATTACK
        if ( real ) printf( "  rugdef: +4\n" );
#endif
    }
    /* окопанность */
    if ( unit_has_flag( unit->sel_prop, "ignore_entr" ) )
        def_grade += 0;
    else {
        if ( unit_has_flag( unit->sel_prop, "infantry" ) )
            def_grade += target->entr / 2;
        else
            def_grade += target->entr;
#ifdef DEBUG_ATTACK
        if ( real ) printf( "  entr:   +%i\n",
                (unit_has_flag( unit->sel_prop, "infantry" )) ? target->entr / 2 : target->entr );
#endif
    }
    /* военно-морской против наземного подразделения */
    if ( !unit_has_flag( unit->sel_prop, "swimming" ) )
        if ( !unit_has_flag( unit->sel_prop, "flying" ) )
            if ( unit_has_flag( target->sel_prop, "swimming" ) ) {
                def_grade += 8;
#ifdef DEBUG_ATTACK
                if ( real ) printf( "  naval: +8\n" );
#endif
            }
    /* плохая погода? */
    if ( unit->sel_prop->rng > 0 )
        if ( weather_types[cur_weather].flags & BAD_SIGHT ) {
            def_grade += 3;
#ifdef DEBUG_ATTACK
            if ( real ) printf( "  sight: +3\n" );
#endif
        }
    /* атаковать артиллерию? */
    if ( type == UNIT_PASSIVE_ATTACK )
        if ( unit_has_flag( unit->sel_prop, "artillery" ) ) {
            def_grade += 3;
#ifdef DEBUG_ATTACK
            if ( real ) printf( "  def vs art: +3\n" );
#endif
        }
    /* пехота против anti_tank? */
    if ( unit_has_flag( target->sel_prop, "infantry" ) )
        if ( unit_has_flag( unit->sel_prop, "anti_tank" ) ) {
            def_grade += 2;
#ifdef DEBUG_ATTACK
            if ( real ) printf( "  antitnk:+2\n" );
#endif
        }
    /* отсутствие топлива снижает эффективность атакующего */
    if ( unit_check_fuel_usage( unit ) && unit->cur_fuel == 0 )
    {
        def_grade += 4;
#ifdef DEBUG_ATTACK
            if ( real ) printf( "  lowfuel:+4\n" );
#endif
    }
    /* сила атакующего */
    atk_strength = unit_get_cur_str( unit );
#ifdef DEBUG_ATTACK
    if ( real && atk_strength != unit_get_cur_str( unit ) )
        printf( "---\n%s with half strength\n", unit->name );
#endif
    /*  PG's formula:
        получить разницу между атакой и защитой
        ударь за каждое очко силы
          if ( diff <= 4 )
              D20 + diff
          else
              D20 + 4 + 0.4 * ( diff - 4 )
        установлен флаг suppr_fire: 1-10 miss, 11-18 suppr, 19+ kill
        нормальный: 1-10 miss, 11-12 suppr, 13+ kill */
    diff = atk_grade - def_grade; if ( diff < -7 ) diff = -7;
    *damage = 0; *suppr = 0;
#ifdef DEBUG_ATTACK
    if ( real ) {
        printf( "---\n%i x %i --> %i x %i\n",
                atk_strength, atk_grade, unit_get_cur_str( target ), def_grade );
    }
#endif
    /* получить шансы на подавление и убийства (вычислено здесь
       также использовать для отладки */
    suppr_chance = kill_chance = 0;
    die_mod = ( diff <= 4 ? diff : 4 + 2 * ( diff - 4 ) / 5 );
    min_roll = 1 + die_mod; max_roll = 20 + die_mod;
    /* получить шансы на подавление и убийства */
    if ( unit_has_flag( unit->sel_prop, "suppr_fire" ) ) {
        int limit = (type==UNIT_DEFENSIVE_ATTACK)?20:18;
        if (limit-min_roll>=0)
            suppr_chance = 0.05*(MINIMUM(limit,max_roll)-MAXIMUM(11,min_roll)+1);
        if (max_roll>limit)
            kill_chance = 0.05*(max_roll-MAXIMUM(limit+1,min_roll)+1);
    }
    else {
        if (12-min_roll>=0)
            suppr_chance = 0.05*(MINIMUM(12,max_roll)-MAXIMUM(11,min_roll)+1);
        if (max_roll>12)
            kill_chance = 0.05*(max_roll-MAXIMUM(13,min_roll)+1);
    }
    if (suppr_chance<0) suppr_chance=0; if (kill_chance<0) kill_chance=0;
    /* Убер юниты чит-код атака убивает цель */
    if ( cur_player->uber_units && target->player != cur_player ) {
        *suppr = 0;
        *damage = target->str;
    }
    else {
        if ( real ) {
#ifdef DEBUG_ATTACK
            printf( "Roll: D20 + %i (Kill: %i%%, Suppr: %i%%)\n",
                    diff <= 4 ? diff : 4 + 2 * ( diff - 4 ) / 5,
                    (int)(100 * kill_chance), (int)(100 * suppr_chance) );
#endif
            while ( atk_strength-- > 0 ) {
                if ( diff <= 4 )
                    result = DICE(20) + diff;
                else
                    result = DICE(20) + 4 + 2 * ( diff - 4 ) / 5;
                if ( unit_has_flag( unit->sel_prop, "suppr_fire" ) ) {
                    int limit = (type==UNIT_DEFENSIVE_ATTACK)?20:18;
                    if ( result >= 11 && result <= limit )
                        (*suppr)++;
                    else
                        if ( result >= limit+1 )
                            (*damage)++;
                }
                else {
                    if ( result >= 11 && result <= 12 )
                        (*suppr)++;
                    else
                        if ( result >= 13 )
                            (*damage)++;
                }
            }
#ifdef DEBUG_ATTACK
            printf( "Kills: %i, Suppression: %i\n\n", *damage, *suppr );
#endif
        }
        else {
            *suppr = (int)(suppr_chance * atk_strength);
            *damage = (int)(kill_chance * atk_strength);
        }
    }
}

/*
====================================================================
Проведите одиночный бой (без проверки оборонительного огня) со случайными значениями.
unit_surprise_attack () обрабатывает атаку с неожиданной целью
(например, Out Of The Sun)
Если в обычном бою возникла надежная защита (неожиданная_атака равна
всегда прочный) 'rugged_def' установлен.
====================================================================
*/
int unit_normal_attack( Unit *unit, Unit *target, int type )
{
    return unit_attack( unit, target, type, 1, 0 );
}
int unit_surprise_attack( Unit *unit, Unit *target )
{
    return unit_attack( unit, target, UNIT_ACTIVE_ATTACK, 1, 1 );
}

/*
====================================================================
Пройдите полную боевую юнит против цели, включая известные (!)
защитные средства поддержки и без случайных модификаций.
Вернуть окончательный урон, полученный обоими юнитами.
Поскольку местность может влиять на идентификатор местности, бой
имеет место (гекс защищающегося юнита).
====================================================================
*/
void unit_get_expected_losses( Unit *unit, Unit *target, int *unit_damage, int *target_damage )
{
    int damage, suppr;
    Unit *df;
    List *df_units = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
#ifdef DEBUG_ATTACK
    printf( "***********************\n" );
#endif
    unit_get_df_units( unit, target, vis_units, df_units );
    unit_backup( unit ); unit_backup( target );
    /* позвольте защитному огню работать (нет шансов защититься от этого) */
    list_reset( df_units );
    while ( ( df = list_next( df_units ) ) ) {
        unit_get_damage( unit, df, unit, UNIT_DEFENSIVE_ATTACK, 0, 0, &damage, &suppr );
        if ( !unit_apply_damage( unit, damage, suppr, 0 ) ) break;
    }
    /* настоящий бой, если у атаки осталась сила */
    if ( unit_get_cur_str( unit ) > 0 )
        unit_attack( unit, target, UNIT_ACTIVE_ATTACK, 0, 0 );
    /* нанести ущерб */
    *unit_damage = unit->str;
    *target_damage = target->str;
    unit_restore( unit ); unit_restore( target );
    *unit_damage = unit->str - *unit_damage;
    *target_damage = target->str - *target_damage;
    list_delete( df_units );
}

/*
====================================================================
Эта функция проверяет «юниты» на наличие сторонников «цели».
что даст оборонительный огонь перед настоящим сражением
«юнита» против «цели» имеет место. Эти юниты помещаются в 'df_units'
(который здесь не создается)

Защищаются только соседние юниты и только ОДИН (выбирается случайным образом)
сторонник будет стрелять, пока у него нет флага
'full_def_str'. (Это означает, что все юниты с этим флагом плюс один
нормальный сторонник будет стрелять.)
====================================================================
*/
void unit_get_df_units( Unit *unit, Unit *target, List *units, List *df_units )
{
    Unit *entry;
    list_clear( df_units );
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        list_reset( units );
        while ( ( entry = list_next( units ) ) ) {
            if ( entry->killed > 1 ) continue;
            if ( entry == target ) continue;
            if ( entry == unit ) continue;
            /* бомбардировщики - перехват невозможно прикрыть unit_check_attack () */
            if ( !unit_has_flag( target->sel_prop, "interceptor" ) )
                if ( unit_is_close( target, entry ) )
                    if ( unit_has_flag( entry->sel_prop, "interceptor" ) )
                        if ( player_is_ally( entry->player, target->player ) )
                            if ( entry->cur_ammo > 0 ) {
                                list_add( df_units, entry );
                                continue;
                            }
            /* ПВО */
            if ( unit_has_flag( entry->sel_prop, "air_defense" ) )
                /* FlaK не будет оказывать поддержку при атаке воздух-воздух.
                 * происходит. Во-первых, на самом деле это было бы ужасно,
                 * во-вторых, Panzer General этого не позволяет.
                 */
                if ( !unit_has_flag( target->sel_prop, "flying" ) )
                    if ( unit_is_close( target, entry ) ) /* adjacent help only */
                        if ( unit_check_attack( entry, unit, UNIT_DEFENSIVE_ATTACK ) )
                            list_add( df_units, entry );
        }
    }
    else if ( unit->sel_prop->rng==0 ) {
        /* артиллерия для рукопашного боя; если юнит атакует на расстоянии, нет
           поддержка */
        list_reset( units );
        while ( ( entry = list_next( units ) ) ) {
            if ( entry->killed > 1 ) continue;
            if ( entry == target ) continue;
            if ( entry == unit ) continue;
            /* HACK: Артиллерия с дальностью 1 не может поддерживать соседние подразделения, но
               должен это сделать. Таким образом, мы позволяем вести оборонительный огонь на дальности 2
               как обычная артиллерия */
            if ( unit_has_flag( entry->sel_prop, "artillery" ) && entry->sel_prop->rng == 1 )
                if ( unit_is_close( target, entry ) )
                    if ( player_is_ally( entry->player, target->player ) )
                        if ( entry->cur_ammo > 0 ) {
                            list_add( df_units, entry );
                            continue;
                        }
            /* нормальная артиллерия */
            if ( unit_has_flag( entry->sel_prop, "artillery" ) )
                if ( unit_is_close( target, entry ) ) /* только смежная помощь */
                    if ( unit_check_attack( entry, unit, UNIT_DEFENSIVE_ATTACK ) )
                        list_add( df_units, entry );
        }
    }
    /* случайным образом удалить всю поддержку, кроме одной */
    if (df_units->count>0)
    {
        entry = list_get(df_units,rand()%df_units->count);
        list_clear(df_units);
        list_add( df_units, entry );
    }
}

/*
====================================================================
Проверьте, разрешено ли этим двум юнитам сливаться друг с другом.
====================================================================
*/
int unit_check_merge( Unit *unit, Unit *source )
{
    /* единицы не должны быть погружены в море / воздух */
    if ( unit->embark != EMBARK_NONE || source->embark != EMBARK_NONE ) return 0;
    /* тот же класс */
    if ( unit->prop.class != source->prop.class ) return 0;
    /* тот же игрок */
    if ( !player_is_ally( unit->player, source->player ) ) return 0;
    /* первый юнит не должен был двигаться так далеко */
    if ( !unit->unused ) return 0;
    /* оба юнита должны иметь одинаковый тип движения */
    if ( unit->prop.mov_type != source->prop.mov_type ) return 0;
    /* численность отряда не должна превышать лимит */
    if ( unit->str + source->str > 13 ) return 0;
    /* крепости (юнит-класс 7) не могли объединиться */
    if ( unit->prop.class == 7 ) return 0;
    /* артиллерия с разной дальностью не может сливаться */
    if ( unit_has_flag( &unit->prop, "artillery" ) && unit->prop.rng!=source->prop.rng) return 0;
    /* пока не удалось: разрешить слияние */
    return 1;
}

/*
====================================================================
Проверьте, можно ли заменить устройство. тип REPLACEMENTS или
ELITE_REPLACEMENTS
====================================================================
*/
int unit_check_replacements( Unit *unit, int type )
{
    /* установка не должна быть погружена в море / воздух */
    if ( unit->embark != EMBARK_NONE ) return 0;
    /* юнит не должен был перемещаться так далеко */
    if ( !unit->unused ) return 0;
    /* численность отряда не должна превышать лимит */
    if (type == REPLACEMENTS)
    {
        if ( unit->str >= unit->max_str ) return 0;
    }
    else
    {
        if ( unit->str >= unit->max_str + unit->exp_level ) return 0;
    }
    /* юнит находится в воздухе, а не на аэродроме */
    if ( unit_has_flag( &unit->prop, "flying" ) && ( map_is_allied_depot(&map[unit->x][unit->y],unit) == 0) ) return 0;
    /* подразделение военно-морское, а не в порту */
    if ( unit_has_flag( &unit->prop, "swimming" ) && ( map_is_allied_depot(&map[unit->x][unit->y],unit) == 0) ) return 0;
    /* юнит получает 0 силы по расчетам */
    if ( unit_get_replacement_strength(unit,type) < 1 ) return 0;
    /* пока не провалился: разрешить замену */
    return 1;
}

/*
====================================================================
Получите максимальную силу, которую юнит может дать, разделив его
Текущее состояние. У юнита должно быть не менее 3 оставшихся сил.
====================================================================
*/
int unit_get_split_strength( Unit *unit )
{
    if ( unit->embark != EMBARK_NONE ) return 0;
    if ( !unit->unused ) return 0;
    if ( unit->str <= 4 ) return 0;
    if ( unit->prop.class == 7 ) return 0; /* fortress */
    return unit->str - 4;
}

/*
====================================================================
Объедините эти два юнита: юнит - это новый юнит, а источник должен быть
удаляется с карты и из памяти после вызова этой функции.
====================================================================
*/
void unit_merge( Unit *unit, Unit *source )
{
    /*  юнит относительный вес */
    float weight1, weight2, total;
    int i, neg;
    /* вычислить вес */
    weight1 = unit->str; weight2 = source->str;
    total = unit->str + source->str;
    /* отрегулировать так weight1 + weigth2 = 1 */
    weight1 /= total; weight2 /= total;
    /* никакие другие действия не разрешены */
    unit->unused = 0; unit->cur_mov = 0; unit->cur_atk_count = 0;
    /* стоимость обновления с момента использования для получения престижа */
    unit->prop.cost = (unit->prop.cost * unit->str +
					source->prop.cost * source->str) /
					(unit->str + source->str);
    /* ремонт повреждений */
    unit->str += source->str;
    /* реорганизация требует некоторого укрепления: предполагается, что новые юниты будут иметь
       окоп 0 с тех пор, как они пришли. новое значение - округленная взвешенная сумма */
    unit->entr = floor((float)unit->entr*weight1+0.5); /* + 0 * weight2 */
    /* обновить опыт */
    i = (int)( weight1 * unit->exp + weight2 * source->exp );
    unit->exp = 0; unit_add_exp( unit, i );
    /* обновить unit::prop */
    /* родственная инициатива */
    unit->prop.ini = (int)( weight1 * unit->prop.ini + weight2 * source->prop.ini );
    /* минимальное движение */
    if ( source->prop.mov < unit->prop.mov )
        unit->prop.mov = source->prop.mov;
    /* максимальное обнаружение */
    if ( source->prop.spt > unit->prop.spt )
        unit->prop.spt = source->prop.spt;
    /* максимальная дальность */
    if ( source->prop.rng > unit->prop.rng )
        unit->prop.rng = source->prop.rng;
    /* относительное количество атак */
    unit->prop.atk_count = (int)( weight1 * unit->prop.atk_count + weight2 * source->prop.atk_count );
    if ( unit->prop.atk_count == 0 ) unit->prop.atk_count = 1;
    /* относительные атаки */
    /* если атака отрицательная, просто используйте абсолютное значение; только восстанавливайте отрицательный результат, если оба юнита отрицательны */
    for ( i = 0; i < trgt_type_count; i++ ) {
        neg = ( unit->prop.atks[i] < 0 && source->prop.atks[i] < 0 );
        unit->prop.atks[i] = (int)( weight1 * abs( unit->prop.atks[i] ) + weight2 * ( source->prop.atks[i] ) );
        if ( neg ) unit->prop.atks[i] *= -1;
    }
    /* относительная защита */
    unit->prop.def_grnd = (int)( weight1 * unit->prop.def_grnd + weight2 * source->prop.def_grnd );
    unit->prop.def_air = (int)( weight1 * unit->prop.def_air + weight2 * source->prop.def_air );
    unit->prop.def_cls = (int)( weight1 * unit->prop.def_cls + weight2 * source->prop.def_cls );
    /* относительные боеприпасы */
    unit->prop.ammo = (int)( weight1 * unit->prop.ammo + weight2 * source->prop.ammo );
    unit->cur_ammo = (int)( weight1 * unit->cur_ammo + weight2 * source->cur_ammo );
    /* относительное топливо */
    unit->prop.fuel = (int)( weight1 * unit->prop.fuel + weight2 * source->prop.fuel );
    unit->cur_fuel = (int)( weight1 * unit->cur_fuel + weight2 * source->cur_fuel );
    /* флаги слияния */
    unit->prop.flags[0] |= source->prop.flags[0];
    unit->prop.flags[1] |= source->prop.flags[1];
    /* звуки, картинка сохраняются */
    /* unit::trans_prop пока не обновляется: */
    /* транспортер первой юнита сохраняется, если используется какой-либо другой транспортер второго юнита */
    if ( unit->trsp_prop.id == 0 && source->trsp_prop.id ) {
        memcpy( &unit->trsp_prop, &source->trsp_prop, sizeof( Unit_Lib_Entry ) );
        /* поскольку это должен быть наземный транспортер, копирующий текущее значение топлива */
        unit->cur_fuel = source->cur_fuel;
    }
    update_bar( unit );
}

/*
====================================================================
Получайте юниты силы за счет замены. Тип
REPLACEMENTS или ELITE_REPLACEMENTS.
====================================================================
*/
int unit_get_replacement_strength( Unit *unit, int type )
{
    int replacement_number = 0, replacement_cost;
    Unit *entry;
    List *close_units;
    close_units = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    list_reset( units );

    /* поиск соседних вражеских юнитов влияние */
    while ( ( entry = list_next( units ) ) ) {
        if ( entry->killed > 1 ) continue;
        if ( entry == unit ) continue;
        if ( unit_is_close( unit, entry ) )
            if ( !player_is_ally( entry->player, unit->player ) )
                list_add( close_units, entry );
    }

    /* для элитных замен сверхпрочные */
    if ( (unit->max_str <= unit->str) && (unit->str < unit->max_str + unit->exp_level ) )
        replacement_number = 1;
    else
    /* для военно-морских частей */
        if ( unit_has_flag( &unit->prop, "swimming" ) )
            if ( unit_has_flag( &unit->prop, "diving" ) || unit_has_flag( &unit->prop, "destroyer" ) )
                replacement_number = MINIMUM( 2, unit->max_str - unit->str );
            else
                replacement_number = MINIMUM( 1, unit->max_str - unit->str );
        else
            replacement_number = unit->max_str - unit->str;

    replacement_number = (int)replacement_number * (3 - close_units->count) / 3;
    list_delete(close_units);

    /* влияние пустынной местности */
    if ( *(terrain_type_find( map[unit->x][unit->y].terrain_id[0] )->flags) & DESERT ){
        if ( replacement_number / 4 < 1 )
            replacement_number = 1;
        else
            replacement_number = replacement_number / 4;
    }

    /* оценить стоимость замены */
    replacement_number++;
    do
    {
        replacement_number--;
        replacement_cost = replacement_number * (unit->prop.cost + unit->trsp_prop.cost) / 12;
        if ( type == REPLACEMENTS )
            replacement_cost = replacement_cost / 4;
    }
    while (replacement_cost > unit->player->cur_prestige);

    unit->cur_str_repl = replacement_number;
    unit->repl_prestige_cost = replacement_cost;

    /* оценить потерянный опыт */
    unit->repl_exp_cost = unit->exp - (10 - unit->cur_str_repl) * unit->exp / 10;
    return replacement_number;
}

/*
====================================================================
Получите замену для устройства. Тип REPLACEMENTS или
ELITE_REPLACEMENTS.
====================================================================
*/
void unit_replace( Unit *unit, int type )
{
    unit->str = unit->str + unit->cur_str_repl;
    /* оценить стоимость замены */
    unit->player->cur_prestige -= unit->repl_prestige_cost;
    /* оценить стоимость опыта */
    if ( type == REPLACEMENTS )
    {
        unit_add_exp( unit, -unit->repl_exp_cost );
    }

    /* поставка юнита */
    unit_supply_intern(unit, UNIT_SUPPLY_ALL);

    /* никакие другие действия не разрешены */
    unit_set_as_used(unit);

    update_bar( unit );
}

/*
====================================================================
Верните True, если юнит использует наземный транспортер.
====================================================================
*/
int unit_check_ground_trsp( Unit *unit )
{
    if ( unit->trsp_prop.id == 0 ) return 0;
    if ( unit_has_flag( &unit->trsp_prop, "flying" ) ) return 0;
    if ( unit_has_flag( &unit->trsp_prop, "swimming" ) ) return 0;
    return 1;
}

/*
====================================================================
Резервное копирование на указатель резервного копирования (неглубокая копия)
====================================================================
*/
void unit_backup( Unit *unit )
{
    memcpy( unit->backup, unit, sizeof( Unit ) );
}
void unit_restore( Unit *unit )
{
    if ( unit->backup->prop.id != 0 ) {
        memcpy( unit, unit->backup, sizeof( Unit ) );
        memset( unit->backup, 0, sizeof( Unit ) );
    }
    else
        fprintf( stderr, "%s: can't restore backup: not set\n", unit->name );
}

/*
====================================================================
Проверьте, может ли цель обеспечить надежную защиту
====================================================================
*/
int unit_check_rugged_def( Unit *unit, Unit *target )
{
    if ( unit_has_flag( unit->sel_prop, "flying" ) || unit_has_flag( target->sel_prop, "flying" ) )
        return 0;
    if ( unit_has_flag( unit->sel_prop, "swimming" ) || unit_has_flag( target->sel_prop, "swimming" ) )
        return 0;
    if ( unit_has_flag( unit->sel_prop, "artillery" ) ) return 0; /* нет прочной защиты от дальних атак */
    if ( unit_has_flag( unit->sel_prop, "ignore_entr" ) ) return 0; /* нет прочного определения для пионеров и тому подобное */
    if ( !unit_is_close( unit, target ) ) return 0;
    if ( target->entr == 0 ) return 0;
    return 1;
}

/*
====================================================================
Вычислить шанс надежной защиты цели.
====================================================================
*/
int unit_get_rugged_def_chance( Unit *unit, Unit *target )
{
    /* Формула PG:
       5% * def_entr *
       ( (def_exp_level + 2) / (atk_exp_level + 2) ) *
       ( (def_entr_rate + 1) / (atk_entr_rate + 1) ) */
    return (int)( 5.0 * target->entr *
           ( (float)(target->exp_level + 2) / (unit->exp_level + 2) ) *
           ( (float)(target->sel_prop->entr_rate + 1) / (unit->sel_prop->entr_rate + 1) ) );
}

/*
====================================================================
Рассчитайте количество использованного топлива. "стоимость" - это базовая стоимость топлива, которая должна быть
вычитается за счет движения по местности. Стоимость будет корректироваться по мере необходимости.
====================================================================
*/
int unit_calc_fuel_usage( Unit *unit, int cost )
{
    int used = cost;

    /* воздушные юниты используют * как минимум * половину своих начальных очков движения.
     */
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        int half = unit->sel_prop->mov / 2;
        if ( used < half ) used = half;
    }

    /* наземным подразделениям грозит штраф в плохую погоду */
    if ( !unit_has_flag( unit->sel_prop, "swimming" )
         && !unit_has_flag( unit->sel_prop, "flying" )
         && weather_types[scen_get_weather()].flags & DOUBLE_FUEL_COST )
        used *= 2;
    return used;
}

/*
====================================================================
Обновить панель юнита.
====================================================================
*/
void unit_update_bar( Unit *unit )
{
    update_bar(unit);
}

/*
====================================================================
Отключить все действия.
====================================================================
*/
void unit_set_as_used( Unit *unit )
{
    unit->unused = 0;
    unit->cur_mov = 0;
    unit->cur_atk_count = 0;
}

/*
====================================================================
Дублируйте юнит, создав новое имя (для разделения) или без
новое имя (для информации о перезапуске кампании).
====================================================================
*/
Unit *unit_duplicate( Unit *unit, int generate_new_name )
{
    Unit *new = calloc(1,sizeof(Unit));
    memcpy(new,unit,sizeof(Unit));
    if (generate_new_name)
        unit_set_generic_name(new, units->count + 1, unit->prop.name);
    if (unit->sel_prop==&unit->prop)
        new->sel_prop=&new->prop;
    else
        new->sel_prop=&new->trsp_prop;
    new->backup = calloc( 1, sizeof( Unit ) );
    /* местность не может быть обновлена ​​здесь */
    return new;
}

/*
====================================================================
Проверьте, есть ли у юнита мало боеприпасов или топлива
====================================================================
*/
int unit_low_fuel( Unit *unit )
{
    if ( !unit_check_fuel_usage( unit ) )
        return 0;
    if ( unit_has_flag( unit->sel_prop, "flying" ) ) {
        if ( unit->cur_fuel <= 20 )
            return 1;
        return 0;
    }
    if ( unit->cur_fuel <= 10 )
        return 1;
    return 0;
}
int unit_low_ammo( Unit *unit )
{
    /* у юнита мало боеприпасов, если у него меньше двадцати процентов
     * запас боеприпасов класса остался или меньше двух количеств,
     * любое значение ниже
     */
    int percentage = unit->sel_prop->ammo / 5;
    return unit->embark == EMBARK_NONE && unit->cur_ammo <= MINIMUM( percentage, 2 );
}

/*
====================================================================
Проверьте, можно ли рассматривать устройство для развертывания.
====================================================================
*/
int unit_supports_deploy( Unit *unit )
{
    return !unit_has_flag( &unit->prop, "swimming" ) /* корабли и */
           && !(unit->killed) /* убитых единиц и */
           && unit->prop.mov > 0; /* крепости не могут быть развернуты */
}

/*
====================================================================
Сбрасывает атрибуты юнита (подготовить юниту к следующему сценарию).
====================================================================
*/
void unit_reset_attributes( Unit *unit )
{
    if (unit->str < unit->max_str)
        unit->str = unit->max_str;
    unit->entr = 0;
    unit->cur_fuel = unit->prop.fuel;
    if ( unit->cur_fuel == 0 && unit->trsp_prop.id && unit->trsp_prop.fuel > 0 )
        unit->cur_fuel = unit->trsp_prop.fuel;
    unit->cur_ammo = unit->prop.ammo;
    unit->killed = 0;
    unit_unmount(unit);
    unit->cur_mov = 1;
    unit->cur_atk_count = 1;
    /* обновить свойства индикатора жизни */
    update_bar( unit );
}
