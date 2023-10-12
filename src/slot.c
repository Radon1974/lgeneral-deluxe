/***************************************************************************
                          slot.c  -  description
                             -------------------
    begin                : Sat Jun 23 2001
    copyright            : (C) 2001 by Michael Speck
    email                : kulkanie@gmx.net
 ***************************************************************************/
/***************************************************************************
                          Modifications
    Patch by Galland 2012 http://sourceforge.net/tracker/?group_id=23757&atid=379520
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

#include <assert.h>
#include <math.h>
#include <dirent.h>
#include "lgeneral.h"
#include "date.h"
#include "nation.h"
#include "unit.h"
#include "player.h"
#include "map.h"
#include "misc.h"
#include "scenario.h"
#include "slot.h"
#include "campaign.h"
#include "localize.h"

#include <SDL_endian.h>

/*
====================================================================
Внешние
====================================================================
*/
extern Config config;
extern Trgt_Type *trgt_types;
extern int trgt_type_count;
extern Mov_Type *mov_types;
extern int mov_type_count;
extern Unit_Class *unit_classes;
extern int unit_class_count;
extern int map_w, map_h;
extern Player *cur_player;
extern int turn;
extern int deploy_turn;
extern int cur_weather;
extern Scen_Info *scen_info;
extern List *units;
extern List *reinf, *avail_units;
extern List *players;
extern int camp_loaded;
extern char *camp_fname;
extern char *camp_name;
extern Camp_Entry *camp_cur_scen;
extern Map_Tile **map;
extern List *prev_scen_core_units;
extern char *prev_scen_fname;

char fname[256]; /* имя файла */

/*
====================================================================
Локальные
====================================================================
*/

/**
 * Это перечисление охватывает управление версиями сохраненных игр.
 * Каждый раз, когда макет сохраненной игры изменяется, новый
 * константа перечисления версий вставляется перед
 * StoreMaxVersion, а функции save_ * / load_ * должны
 * быть адаптированным для работы с более высокой версией.
 */
enum StorageVersion {
    StoreVersionLegacy = 1000,
    StoreUnitIsGuarding,	/* до этой версии хранятся на единицу */
    StoreGlobalVersioning,
    StorePurchaseData, /* стоимость входов в библиотеку юнитов, престиж для игроков */
    StoreCoreVersionData, /* Основные данные армии хранятся в сохранении */
    StoreNewFolderStructure, /* сценарии имеют новую структуру папок */
    StoreLandTransportData, /* сухопутный транспортник на морских судах сохранен */
    StoreCampaignEntryName, /* название кампании магазина */
    StoreAdditionalFlags, /* хранить дополнительную переменную флагов */
    StoreCoreTransferList, /* предыдущие 'сценарии основные блоки для кампаний */
    /* вставьте новые версии перед этим комментарием */
    StoreMaxVersion,
    StoreHighestSupportedVersion = StoreMaxVersion - 1,
    StoreVersionLimit = 65535	/* не должно быть больше, иначе определение порядка байтов не сработает */
};

/** обработка байта сигнала текущего обрабатываемого файла */
static int endian_need_swap;
/** версия загруженного файла */
static enum StorageVersion store_version;

/*
====================================================================
Сохранить / загрузить имя слота в / из файла.
====================================================================
*/
static void slot_read_name( char *name, FILE *file )
{
    fread( fname, 256, 1, file );
}
static void slot_write_name( char *name, FILE *file )
{
    fwrite( fname, 256, 1, file );
}

/*
====================================================================
загрузить одно целое число с откатом
====================================================================
*/
static int try_load_int( FILE *file, int deflt )
{
    int i;
    if ( fread( &i, sizeof( int ), 1, file ) )
        i = endian_need_swap ? SDL_Swap32(i) : i;
    else
        i = deflt;
    return i;
}

/*
====================================================================
Сохранить / загрузить одно целое число
====================================================================
*/
static void save_int( FILE *file, int i )
{
    /* мы всегда пишем порядок байтов, зависящий от платформы */
    fwrite( &i, sizeof( int ), 1, file );
}
static int load_int( FILE *file )
{
    return try_load_int( file, 0 );
}

/*
====================================================================
загрузить один указатель с откатом
====================================================================
*/
static void *try_load_pointer( FILE *file, void *deflt )
{
    void *p;
    /* здесь мы игнорируем порядок байтов, вы не можете использовать указатель
     * так или иначе.
     */
    if ( !fread( &p, sizeof( void * ), 1, file ) ) p = deflt;
    return p;
}

/*
====================================================================
Сохранить / загрузить один указатель
====================================================================
*/
static void save_pointer( FILE *file, const void *p )
{
    fwrite( &p, sizeof( void * ), 1, file );
}
static void *load_pointer( FILE *file )
{
    return try_load_pointer( file, 0 );
}
/*
====================================================================
Сохранить / загрузить строку в / из файла.
====================================================================
*/
static void save_string( FILE *file, char *str )
{
    int length;
    /* сохранить длину, а затем строку */
    length = strlen( str );
    fwrite( &length, sizeof( int ), 1, file );
    fwrite( str, sizeof( char ), length, file );
}
static char* load_string( FILE *file )
{
    char *str = 0;
    int length;

    fread( &length, sizeof( int ), 1, file );
    str = calloc( length + 1, sizeof( char ) );
    fread( str, sizeof( char ), length, file );
    str[length] = 0;
    return str;
}

/*
====================================================================
Сохранить / загрузить объект из / в файл.
Сохраните саму структуру блока, после чего строки идентификаторов
свойства, свойства транспортеров, игрок, нация. sel_prop можно перестроить с помощью
проверка посадки. Как мы можем объединить блоки свойства и свойства транспортеров
не могут быть перезаписаны исходными данными. Вместо этого все
указатели разрешаются строкой идентификатора.
====================================================================
*/
enum UnitStoreParams {
    UnitLibSize = 4*sizeof(void *) + 26*sizeof(int) + TARGET_TYPE_LIMIT*sizeof(int)
#ifdef WITH_SOUND
            + 1*sizeof(void *) + 1*sizeof(int)
#endif
            ,
    UnitNameSize = 21*sizeof(char) + 3*sizeof(char)/*набивка*/,
    UnitTagSize = 32*sizeof(char),
    UnitBattleHonorsSize = 20*sizeof(char),
    UnitSize = 2*UnitLibSize + 5*sizeof(void *) + UnitNameSize + UnitTagSize + 31*sizeof(int) + 6*UnitBattleHonorsSize
};
static void save_unit_lib_entry( FILE *file, Unit_Lib_Entry *entry )
{
    int i;

    //COMPILE_TIME_ASSERT(sizeof(Unit_Lib_Entry) == UnitLibSize);
    /* напишите каждому члену */
    /* зарезервировано (было: id) */
    save_pointer(file, entry->id);
    /* зарезервированный (was: name) */
    save_pointer(file, entry->name);
    /* нация 1..24 */
    save_int(file, entry->nation);
    /* класс (0 = пехота, 1 = танк, ..) 0..16 */
    save_int(file, entry->class);
    /* тип цели (0 = Soft 1 = Hard 2 = Air 3 = Naval) 0..3 */
    save_int(file, entry->trgt_type);
    /* инициатива 0..12 */
    save_int(file, entry->ini);
    /* движение 0..14 */
    save_int(file, entry->mov);
    /* тип движения 3 = Leg 4 = Towed 2 = Wheeled 1 = Half-Tracked 0 = Tracked 7 = All-Terrain 6 = Naval 5 = Air 0..7 */
    save_int(file, entry->mov_type);
    /* обнаружение 1..6 */
    save_int(file, entry->spt);
    /* дистанция стрельбы 0..5 */
    save_int(file, entry->rng);
    /* счетчик атак */
    save_int(file, entry->atk_count);
    /* atks[TARGET_TYPE_LIMIT] */
    for (i = 0; i < TARGET_TYPE_LIMIT; i++)
        save_int(file, entry->atks[i]);
    /* наземная оборона 1..22 */
    save_int(file, entry->def_grnd);
    /* воздушная оборона 0..17 */
    save_int(file, entry->def_air);
    /* близкая защита 0..12 */
    save_int(file, entry->def_cls);
    /* окопанность */
    save_int(file, entry->entr_rate);
    /* боеприпасы 2..50 */
    save_int(file, entry->ammo);
    /* топливо 24..240 */
    save_int(file, entry->fuel);
    /* зарезервировано (было: значок) */
    save_pointer(file, entry->icon);
    /* зарезервировано (было: icon_tiny) */
    save_pointer(file, entry->icon_tiny);
    /* тип иконки */
    save_int(file, entry->icon_type);
    /* icon_w */
    save_int(file, entry->icon_w);
    /* icon_h */
    save_int(file, entry->icon_h);
    /* icon_tiny_w */
    save_int(file, entry->icon_tiny_w);
    /* icon_tiny_h */
    save_int(file, entry->icon_tiny_h);
    /* флаг */
    save_int(file, entry->flags[0]);
    save_int(file, entry->flags[1]);
    /* начальный год */
    save_int(file, entry->start_year);
    /* начальный месяц */
    save_int(file, entry->start_month);
    /* последний год */
    save_int(file, entry->last_year);
    /* стоимость */
    save_int(file, entry->cost);
    /* выделить звук */
#ifdef WITH_SOUND
    //COMPILE_TIME_ASSERT_SYMBOL(entry->wav_alloc);
#endif
    save_int(file, 0);
    /* звук движения */
#ifdef WITH_SOUND
    //COMPILE_TIME_ASSERT_SYMBOL(entry->wav_move);
#endif
    save_pointer(file, 0);
    /* зарезервировано (было: eval_score) */
    save_int(file, entry->eval_score);
}
static void save_unit( FILE *file, Unit *unit )
{
    /* Не забудьте обновить save_unit и load_unit, если вы измените Unit.
     * Если вам нужно добавить поле, используйте зарезервированное поле.
     * Затем увеличьте номер версии,
     * и предоставить значения по умолчанию при чтении файла с более низкой версией.
     * Ах, да: не притворяйтесь, что sizeof (int) == sizeof (void *)
     */
    //COMPILE_TIME_ASSERT(sizeof(Unit) == UnitSize);
    /* запись каждого члена */
    /* свойства */
    save_unit_lib_entry(file, &unit->prop);
    /* свойства транспорта */
    save_unit_lib_entry(file, &unit->trsp_prop);
    /* зарезервировано (было: sel_prop) */
    save_pointer(file, unit->sel_prop);
    /* зарезервировано (было: резервное копирование) */
    save_pointer(file, unit->backup);
    /* имя */
    fwrite(unit->name, UnitNameSize, 1, file);
    /* зарезервировано (было: игрок) */
    save_pointer(file, unit->player);
    /* зарезервировано (было: нация) */
    save_pointer(file, unit->nation);
    /* зарезервировано (было: местность) */
    save_pointer(file, unit->terrain);
    /* x */
    save_int(file, unit->x);
    /* y */
    save_int(file, unit->y);
    /* сила */
    save_int(file, unit->str);
    /* окопанность */
    save_int(file, unit->entr);
    /* опыт */
    save_int(file, unit->exp);
    /* версия хранилища юнитов(было: exp_level) */
    //COMPILE_TIME_ASSERT_SYMBOL(unit->exp_level);
    save_int(file, StoreCoreTransferList);
    /* задержка */
    save_int(file, unit->delay);
    /* садиться */
    save_int(file, unit->embark);
    /* ориентация */
    save_int(file, unit->orient);
    /* зарезервировано (было: icon_offset) */
    save_int(file, unit->icon_offset);
    /* зарезервировано (было: icon_tiny_offset) */
    save_int(file, unit->icon_tiny_offset);
    /* уровень поддержки */
    save_int(file, unit->supply_level);
    /* текущиее топливо */
    save_int(file, unit->cur_fuel);
    /* текущиие боеприпасы */
    save_int(file, unit->cur_ammo);
    /* текущиее движение */
    save_int(file, unit->cur_mov);
    /* текущий счетчик атак */
    save_int(file, unit->cur_atk_count);
    /* неиспользуемый (* член * называется неиспользованным. Он не зарезервирован ;-) ) */
    save_int(file, unit->unused);
    /* зарезервировано (было: damage_bar_width) */
    save_int(file, unit->damage_bar_width);
    /* является охранником (было: damage_bar_offset) */
    //COMPILE_TIME_ASSERT_SYMBOL(unit->damage_bar_offset);
    save_int(file, unit->is_guarding);
    /* reserved (was: suppr) */
    save_int(file, unit->suppr);
    /* очередь поддержки */
    save_int(file, unit->turn_suppr);
    /* убит */
    save_int(file, unit->killed);
    /* reserved (was: fresh_deploy) */
    save_int(file, unit->fresh_deploy);
    /* тег */
    fwrite(unit->tag, UnitTagSize, 1, file);
    /* reserved (was: eval_score) */
    save_int(file, unit->eval_score);
    /* reserved (was: target_score) */
    save_int(file, unit->target_score);
    /* prop.id */
    save_string( file, unit->prop.id );
    /* trsp_prop.id */
    if ( unit->trsp_prop.id )
        save_string( file, unit->trsp_prop.id );
    else
        save_string( file, "none" );
    /* игрок */
    save_string( file, unit->player->id );
    /* нация */
    save_string( file, unit->nation->id );
    /* максимальная сила */
    save_int(file, unit->max_str);
    /* ядро */
    save_int(file, unit->core);
    /* боевые награды */
    int i;
    for (i = 0;i < 5;i++)
        fwrite(unit->star[i], UnitBattleHonorsSize, 1, file);
    /* land_trsp_prop */
    save_unit_lib_entry(file, &unit->land_trsp_prop);
    /* land_trsp_prop.id */
    if ( unit->land_trsp_prop.id )
        save_string( file, unit->land_trsp_prop.id );
    else
        save_string( file, "none" );
}
static void load_unit_lib_entry( FILE *file, Unit_Lib_Entry *entry )
{
    int i;
    void *p;
    /* write each member */
    /* reserved */
    p = load_pointer(file);
    /* reserved (was: name) */
    p = load_pointer(file);
    /* нация 1..24 nation (since StorePurchaseData) */
    entry->nation = -1;
    if (store_version >= StorePurchaseData)
        entry->nation = load_int(file);
    /* класс (0 = пехота, 1 = танк, ..) 0..16 */
    entry->class = load_int(file);
    /* тип цели (0 = Soft 1 = Hard 2 = Air 3 = Naval) 0..3 */
    entry->trgt_type = load_int(file);
    /* инициатива 0..12 */
    entry->ini = load_int(file);
    /* движение 0..14 */
    entry->mov = load_int(file);
    /* тип движения 3 = Leg 4 = Towed 2 = Wheeled 1 = Half-Tracked 0 = Tracked 7 = All-Terrain 6 = Naval 5 = Air 0..7 */
    entry->mov_type = load_int(file);
    /* обнаружение 1..6 */
    entry->spt = load_int(file);
    /* дистанция стрельбы 0..5 */
    entry->rng = load_int(file);
    /* счетчик атак */
    entry->atk_count = load_int(file);
    /* atks[TARGET_TYPE_LIMIT] */
    for (i = 0; i < TARGET_TYPE_LIMIT; i++)
        entry->atks[i] = load_int(file);
    /* наземная оборона 1..22 */
    entry->def_grnd = load_int(file);
    /* воздушная оборона 0..17 */
    entry->def_air = load_int(file);
    /* близкая защита 0..12 */
    entry->def_cls = load_int(file);
    /* окопанность */
    entry->entr_rate = load_int(file);
    /* боеприпасы 2..50 */
    entry->ammo = load_int(file);
    /* топливо 24..240 */
    entry->fuel = load_int(file);
    /* reserved (was: icon) */
    load_pointer(file);
    /* reserved (was: icon_tiny) */
    load_pointer(file);
    /* тип иконки */
    entry->icon_type = load_int(file);
    /* icon_w */
    entry->icon_w = load_int(file);
    /* icon_h */
    entry->icon_h = load_int(file);
    /* icon_tiny_w */
    entry->icon_tiny_w = load_int(file);
    /* icon_tiny_h */
    entry->icon_tiny_h = load_int(file);
    /* флаг */
    entry->flags[0] = load_int(file);
    if (store_version >= StoreAdditionalFlags)
        entry->flags[1] = load_int(file);
    /* начальный год */
    entry->start_year = load_int(file);
    /* начальный месяц */
    entry->start_month = load_int(file);
    /* последний год */
    entry->last_year = load_int(file);
    /* стоимость cost (since StorePurchaseData) */
    entry->cost = 0;
    if (store_version >= StorePurchaseData)
        entry->cost = load_int(file);
    /* выделить звук */
#ifdef WITH_SOUND
    entry->wav_alloc =
#endif
    load_int(file);
    /* звук движения */
    load_pointer(file);
    /* reserved (was: eval_score) (эффективность юнита) */
    load_int(file);

    /* пересчет элементов, которые не хранятся (оценка эффективности юнита) */
    unit_lib_eval_unit( entry );
}
Unit* load_unit( FILE *file )
{
    Unit_Lib_Entry *lib_entry;
    Unit *unit = 0;
    char *str;
    int unit_store_version;
    int val;

    unit = calloc( 1, sizeof( Unit ) );
    /* читать каждого члена */
    /* свойства */
    load_unit_lib_entry(file, &unit->prop);
    /* свойства транспорта */
    load_unit_lib_entry(file, &unit->trsp_prop);
    /* зарезервированный */
    load_pointer(file);
    /* зарезервированный */
    load_pointer(file);
    /* имя */
    fread(unit->name, UnitNameSize, 1, file);
    unit->name[UnitNameSize - 1] = 0;
    /* зарезервированный */
    load_pointer(file);
    /* зарезервированный */
    load_pointer(file);
    /* зарезервированный */
    load_pointer(file);
    /* x */
    unit->x = load_int(file);
    /* y */
    unit->y = load_int(file);
    /* сила */
    unit->str = load_int(file); /*ERROR Неправильное значение считывается */
    /* окопанность */
    unit->entr = load_int(file);
    /* опыт */
    unit->exp = load_int(file);

    /* версия хранилища юнитов (было: exp_level) */
    unit_store_version = load_int(file);
    /* задержка */
    unit->delay = load_int(file);
    /* садиться */
    unit->embark = load_int(file);
    /* ориентация */
    unit->orient = load_int(file);
    /* зарезервировано (было: icon_offset) */
    load_int(file);
    /* зарезервировано (было: icon_tiny_offset) */
    load_int(file);
    /* уровень поддержки */
    unit->supply_level = load_int(file);
    /* текущиее топливо */
    unit->cur_fuel = load_int(file);
    /* текущиие боеприпасы */
    unit->cur_ammo = load_int(file);
    /* текущиее движение */
    unit->cur_mov = load_int(file);
    /* текущий счетчик атак */
    unit->cur_atk_count = load_int(file);
    /* неиспользуемый (* член * называется неиспользованным. Он не зарезервирован ;-) ) */
    unit->unused = load_int(file);
    /* зарезервировано (было: damage_bar_width) */
    load_int(file);
    /* является охранником (was: damage_bar_offset) */
    val = load_int(file);
    unit->is_guarding = unit_store_version >= StoreUnitIsGuarding ? val : 0;
    /* зарезервировано (было: suppr) */
    load_int(file);
    /* очередь поддержки */
    unit->turn_suppr = load_int(file);
    /* убит */
    unit->killed = load_int(file);
    /* зарезервировано (было: fresh_deploy) */
    load_int(file);
    /* тег */
    fread(unit->tag, UnitTagSize, 1, file);
    unit->tag[UnitTagSize - 1] = 0;
    /* зарезервировано (было: eval_score) */
    load_int(file);
    /* зарезервировано (было: target_score) */
    load_int(file);

    unit->backup = calloc( 1, sizeof( Unit ) );
    /* sel_prop */
    if ( unit->embark == EMBARK_NONE )
        unit->sel_prop = &unit->prop;
    else
        unit->sel_prop = &unit->trsp_prop;
    /* реквизит */
    str = load_string( file );
    lib_entry = unit_lib_find( str );
    unit->prop.id = lib_entry->id;
    unit->prop.name = lib_entry->name;
    unit->prop.icon = lib_entry->icon;
    unit->prop.icon_tiny = lib_entry->icon_tiny;
#ifdef WITH_SOUND
    unit->prop.wav_move = lib_entry->wav_move;
#endif
    free( str );
    /* транспортер */
    str = load_string( file );
    lib_entry = unit_lib_find( str );
    if ( lib_entry ) {
        unit->trsp_prop.id = lib_entry->id;
        unit->trsp_prop.name = lib_entry->name;
        unit->trsp_prop.icon = lib_entry->icon;
        unit->trsp_prop.icon_tiny = lib_entry->icon_tiny;
#ifdef WITH_SOUND
        unit->trsp_prop.wav_move = lib_entry->wav_move;
#endif
    }
    free( str );
    /* игрок */
    str = load_string( file );
    unit->player = player_get_by_id( str );
    free( str );
    /* нация */
    str = load_string( file );
    unit->nation = nation_find( str );
    free( str );
    /* пересчитать члены, которые не сохранены */
    if ( store_version >= StoreCoreVersionData )
    {
		/* patch by Galland 2012 http://sourceforge.net/tracker/?group_id=23757&atid=379520 */
		unit->terrain = terrain_type_find( map[unit->x][unit->y].terrain_id[0] );
		/* end patch */
		val = load_int(file);
		/* max_str (since StoreCoreVersionData) */
		unit->max_str = unit_store_version >= StoreCoreVersionData ? val : 10;
		/* core (since StoreCoreVersionData) */
		val = load_int(file);
		unit->core = unit_store_version >= StoreCoreVersionData ? val : AUXILIARY;
		/* боевые награды (начиная с StoreCoreVersionData) */
		int i;
		if (store_version >= StoreCoreVersionData)
		    for (i = 0;i < 5;i++)
		    {
		        fread(unit->star[i], UnitBattleHonorsSize, 1, file);
		        unit->star[i][UnitBattleHonorsSize - 1] = 0;
		    }
    }
    if ( store_version >= StoreLandTransportData )
    {
        /* land_trsp_prop */
        load_unit_lib_entry(file, &unit->land_trsp_prop);
        /* наземный транспортер */
        str = load_string( file );
        lib_entry = unit_lib_find( str );
        if ( lib_entry ) {
            unit->land_trsp_prop.id = lib_entry->id;
            unit->land_trsp_prop.name = lib_entry->name;
            unit->land_trsp_prop.icon = lib_entry->icon;
            unit->land_trsp_prop.icon_tiny = lib_entry->icon_tiny;
#ifdef WITH_SOUND
            unit->land_trsp_prop.wav_move = lib_entry->wav_move;
#endif
        }
    }

    unit_adjust_icon( unit );
    unit->exp_level = unit->exp / 100;
    unit_update_bar( unit );
    return unit;
}
/*
====================================================================
Сохранение / загрузка структур игрока
Порядок игроков в файле сценария не должен изменяться.
Предполагается, что он сохраняется, поэтому сохраняются только некоторые значения.
====================================================================
*/
static void save_player( FILE *file, Player *player )
{
    /* контроль, воздушный / морской транспорт */
    save_int( file, player->ctrl - 1/* совместимость со старыми сохраненными играми */ );
    save_int( file, player->air_trsp_used );
    save_int( file, player->sea_trsp_used );
    save_int( file, player->cur_prestige );
    save_string( file, player->ai_fname );
}
static void load_player( FILE *file, Player *player )
{
    /* контроль, воздушный / морской транспорт */
    player->ctrl = load_int( file ) + 1/* совместимость со старыми сохраненными играми */;
    player->air_trsp_used = load_int( file );
    player->sea_trsp_used = load_int( file );
    player->cur_prestige = 0;
    if (store_version >= StorePurchaseData)
        player->cur_prestige = load_int( file );
    free( player->ai_fname );
    player->ai_fname = load_string( file );
}
/*
====================================================================
Сохраните индексы в сценах :: единицы наземного и воздушного отряда на этом тайле.
====================================================================
*/
static void save_map_tile_units( FILE *file, Map_Tile *tile )
{
    int i;
    int index;
    index = -1;
    /* земля */
    list_reset( units );
    if ( tile->g_unit )
        for ( i = 0; i < units->count; i++ )
            if ( tile->g_unit == list_next( units ) ) {
                index = i;
                break;
            }
    save_int( file, index );
    /* воздух */
    index = -1;
    list_reset( units );
    if ( tile->a_unit )
        for ( i = 0; i < units->count; i++ )
            if ( tile->a_unit == list_next( units ) ) {
                index = i;
                break;
            }
    save_int( file, index );
}
/* загрузить единицы плитки карты, предполагая, что для сценария :: единицы установлены правильные единицы */
static void load_map_tile_units( FILE *file, Unit **unit, Unit **air_unit )
{
    int index;
    index = load_int( file );
    if ( index == -1 ) *unit = 0;
    else
        *unit = list_get( units, index );
    index = load_int( file );
    if ( index == -1 ) *air_unit = 0;
    else
        *air_unit = list_get( units, index );
}
/*
====================================================================
Флаги сохранения карты: строки идентификатора нации и идентификатора игрока
====================================================================
*/
static void save_map_tile_flag( FILE *file, Map_Tile *tile )
{
    if ( tile->nation )
        save_string( file, tile->nation->id );
    else
        save_string( file, "none" );
    if ( tile->player )
        save_string( file, tile->player->id );
    else
        save_string( file, "none" );
}
static void load_map_tile_flag( FILE *file, Nation **nation, Player **player )
{
    char *str;
    str = load_string( file );
    *nation = nation_find( str );
    free( str );
    str = load_string( file );
    *player = player_get_by_id( str );
    free( str );
}
/* Загрузить / сохранить список передачи основного блока */
static void save_prev_scen_core_units( FILE *file )
{
	int num = 0;
	transferredUnitProp *prop;

	/* сохранить количество записей */
	if (prev_scen_core_units)
		num = prev_scen_core_units->count;
	save_int(file, num);
	if (num == 0)
		return; /* больше нет данных */

	/* сохранить имя файла сценария */
	save_string(file, prev_scen_fname);

	/* сохранить записи */
	list_reset(prev_scen_core_units);
	while ((prop = list_next(prev_scen_core_units))) {
		save_unit_lib_entry(file, &prop->prop);
		save_unit_lib_entry(file, &prop->trsp_prop);
		save_string(file, prop->prop_id);
		save_string(file, prop->trsp_prop_id);
		save_string(file, prop->name);
		save_string(file, prop->player_id);
		save_string(file, prop->nation_id);
		save_int(file, prop->str);
		save_int(file, prop->exp);
		save_string(file, prop->tag);
	}
}
static void load_prev_scen_core_units( FILE *file )
{
	int i, num;
	transferredUnitProp *prop;
	char *str;

	/* номер загрузки */
	num = load_int(file);
	if (num == 0) {
		if (prev_scen_core_units)
			list_clear(prev_scen_core_units);
		return; /* ничего не следует */
	}

	/* создать список, если еще не существует */
	if (!prev_scen_core_units)
		prev_scen_core_units = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
	else
		list_clear(prev_scen_core_units);

	/* прочитать имя файла сценария */
	if (prev_scen_fname)
		free(prev_scen_fname);
	prev_scen_fname = load_string(file);

	/* создавать и помещать записи в список */
	for (i = 0; i < num; i++) {
		prop = calloc( 1, sizeof( transferredUnitProp ) );

		load_unit_lib_entry(file, &prop->prop);
		load_unit_lib_entry(file, &prop->trsp_prop);

		str = load_string(file);
		snprintf( prop->prop_id, sizeof(prop->prop_id), "%s",str);
		free(str);
		str = load_string(file);
		snprintf( prop->trsp_prop_id, sizeof(prop->trsp_prop_id), "%s",str);
		free(str);
		str = load_string(file);
		snprintf( prop->name, sizeof(prop->name), "%s",str);
		free(str);
		str = load_string(file);
		snprintf( prop->player_id, sizeof(prop->player_id), "%s",str);
		free(str);
		str = load_string(file);
		snprintf( prop->nation_id, sizeof(prop->nation_id), "%s",str);
		free(str);
		prop->str = load_int(file);
		prop->exp = load_int(file);
		str = load_string(file);
		snprintf( prop->tag, sizeof(prop->tag), "%s",str);
		free(str);

		list_add(prev_scen_core_units, prop);
	}
}


/*
====================================================================
Публичные
====================================================================
*/

/*
====================================================================
Сохранить / загрузить игру в / из файла.
====================================================================
*/
// TODO (user#1#): Сохранить / загрузить
int slot_save( char *name, char *subdir )
{
    FILE *file = 0;
    char path[512];
    int i, j;
    /* ПРИМЕЧАНИЕ. Если вы измените макет сохранения, не забудьте
       для увеличения версии и добавления правильной дискриминации
       для загрузки
       макет:
        slot_name
        версия (начиная с версии StoreGlobalVersioning)
        кампания загружена
        название кампании (необязательно)
        идентификатор сценария кампании (необязательно)
        имя файла сценария
        туман войны
        поддержка
        погода
        развертывание
        покупка

        включить замены
        включить основную армию

        текущие обороты
        текущий player_id
        информация об игроке

        количество юнитов
        юниты

        количество юнитов повторного ввода
        усиливать юниты

        доступное количество единиц
        доступные юниты

        ширина карты
        высота карты
        единицы плитки карты
        флаги плитки карты
    */
    /* если мы когда-нибудь столкнемся с этим, у нас будут * большие * проблемы */
    //COMPILE_TIME_ASSERT(StoreHighestSupportedVersion <= StoreVersionLimit);
    /* получить имя файла */
    sprintf( path, "%s/%s/Save/%s/%s", get_gamedir(), config.mod_name, subdir, name );
    /* открыть файл */
    //fopen( path, "w" )	Создает файл для записи (по умолчанию файл открывается как текстовый).
    //fopen( path, "wb" )	Создает двоичный файл для записи.
    if ( ( file = fopen( path, "wb" ) ) == 0 ) {
        fprintf( stderr, tr("%s: not found\n"), path );
        return 0;
    }
    /* идентификация слота записи */
    slot_write_name( name, file );
    /* написать версию */
    save_int(file, StoreHighestSupportedVersion);
    /* если кампания настроена, следует некоторая информация о кампании */
    save_int( file, camp_loaded );
    if ( camp_loaded ) {
        save_string( file, camp_fname );
        save_string( file, camp_name );
        save_string( file, camp_cur_scen->id );
        save_prev_scen_core_units( file );
    }
    /*основные данные */
    /*имя сцерария */
    save_string( file, scen_info->fname );
    /*разрешить туман войны */
    save_int( file, config.fog_of_war );
    /*разрешить поддержку */
    save_int( file, config.supply );
    /*разрешить погоду */
    save_int( file, config.weather );
    /*разрешить развертывание */
    save_int( file, config.deploy_turn );
    /* покупка по престижу */
    save_int( file, config.purchase );
    /* включить слияние во время боя (0) или включить замены по престижности (1) */
    save_int( file, config.merge_replacements );
    /* включить основную армию, купленную игроком, пройти кампанию */
    save_int( file, config.use_core_units );
    /* ход */
    save_int( file, turn );
    /* получить индекс игрока в списке */
    save_int( file, player_get_index( cur_player ) );
    /* игроки */
    list_reset( players );
    for ( i = 0; i < players->count; i++ )
        save_player( file, list_next( players ) );
    /* юниты */
    list_reset( units );
    save_int( file, units->count );
    for ( i = 0; i < units->count; i++ )
        save_unit( file, list_next( units ) );
    /* подкрепление */
    list_reset( reinf );
    save_int( file, reinf->count );
    for ( i = 0; i < reinf->count; i++ )
        save_unit( file, list_next( reinf ) );
    list_reset( avail_units );
    save_int( file, avail_units->count );
    for ( i = 0; i < avail_units->count; i++ )
        save_unit( file, list_next( avail_units ) );
    /* карты */
    save_int( file, map_w );
    save_int( file, map_h );
    for ( i = 0; i < map_w; i++ )
        for ( j = 0; j < map_h; j++ )
            save_map_tile_units( file, map_tile( i, j ) );
    for ( i = 0; i < map_w; i++ )
        for ( j = 0; j < map_h; j++ )
            save_map_tile_flag( file, map_tile( i, j ) );
    fclose( file );
    return 1;
}
int slot_load( char *name )
{
    FILE *file = 0;
    char path[512], temp[256];
    int camp_saved;
    int i, j;
    char *scen_file_name = 0;
    int unit_count;
    char *str;

    store_version = StoreVersionLegacy;
    /* получить имя файла */
    sprintf( path, "%s/%s/Save/%s", get_gamedir(), config.mod_name, name );
    /* открыть файл */
    //fopen( path, "r" )    Открывает файл для чтения (по умолчанию файл открывается как текстовый).
    //fopen( path, "rb" )   Открывает двоичный файл для чтения.
//#ifdef WIN32
    if ( ( file = fopen( path, "rb" ) ) == 0 ) {
//#else
//    if ( ( file = fopen( path, "r" ) ) == 0 ) {
//#endif
        fprintf( stderr, tr("%s: not found\n"), path );
        return 0;
    }
    /* чтение идентификатора слота - ничего не изменит, но дескриптор файла нужно переместить */
    slot_read_name( temp, file );
    /* читать версию */
    fread( &store_version, sizeof( int ), 1, file );
    /* проверить порядок байтов */
    endian_need_swap = !!(store_version & 0xFFFF0000);
    if (endian_need_swap)
        store_version = SDL_Swap32(store_version);  //store_version = SDL_Swap32(store_version);

    /* отклонить игру, если она слишком новая */
    if (store_version > StoreHighestSupportedVersion) {
        fprintf(stderr, "%s: Needs version %d, we only provide %d\n", path, store_version, StoreHighestSupportedVersion);
        fclose(file);
        return 0;
    }
    /* если еще нет версии, это устаревшая версия */
    if (store_version < StoreVersionLegacy)
        camp_saved = store_version;
    else	/* в противном случае флаг кампании появляется после */
        camp_saved = load_int(file);
    /* если кампания настроена, следует некоторая информация о кампании */
    camp_delete();
    if ( camp_saved ) {
        /*перезагрузить кампанию и установить текущий идентификатор сценария */
        str = load_string( file );
        snprintf( path, 512, "%s", str );
        free( str );
        if ( store_version >= StoreCampaignEntryName )
        {
            str = load_string( file );
            camp_load( path, str );
        }
        else
            camp_load( path, "" );
        free( str );
        str = load_string( file );
        camp_set_cur( str );
        free( str );
        if (store_version >= StoreCoreTransferList)
            load_prev_scen_core_units( file );
        else if (prev_scen_core_units)
            list_clear(prev_scen_core_units);
    }
    /* загружаемый сейчас сценарий принадлежит к идентификатору сценария указанной выше кампании * /
    / * читать имя файла сценария */
    scen_file_name = load_string( file );
    if ( store_version < StoreNewFolderStructure )
        snprintf( scen_file_name, strlen( scen_file_name ), "%s", strrchr( scen_file_name, '/' ) );
    if ( !scen_load( scen_file_name ) ) //загрузка сценария
    {
        free( scen_file_name );
        fclose(file);
        return 0;
    }
    free( scen_file_name );
    /* основные данные */
    config.fog_of_war = load_int( file );
    config.supply = load_int( file );
    config.weather = load_int( file );
    if (store_version < StorePurchaseData) {
        /* поскольку нет надлежащей информации о престиже для уже сыгранных ходов и все
         * хранимые юниты имеют стоимость 0, что означает отсутствие выгоды за повреждение
         * или уничтожая юнитов, отключите покупку для старых сохраненных игр. к
         * предотвратить путаницу отображения примечания. :-)
         * развертывать оставлен без изменений и используется только так, как настроено командой
         * вариант линии. */
        if (config.purchase != NO_PURCHASE) {
            printf( "**********\n"
		"Note: Games saved before LGeneral version 1.2 do not provide proper prestige\n"
		"and purchase information. Therefore purchase option will be disabled. You can\n"
		"re-enable it in the settings menu when starting a new scenario.\n"
		"**********\n");
            config.purchase = NO_PURCHASE;
        }
    } else {
        config.deploy_turn = load_int( file );
        config.purchase = load_int( file );
    }
    if (store_version >= StoreCoreVersionData) {
        config.merge_replacements = load_int( file );
        config.use_core_units = load_int( file );
    }
    turn = load_int( file );
    cur_player = player_get_by_index( load_int( file ) );
    cur_weather = scen_get_weather();
    /* игроки */
    list_reset( players );
    for ( i = 0; i < players->count; i++ )
        load_player( file, list_next( players ) );
    /* материал юнитов */
    list_clear( units );
    unit_count = load_int( file );
    for ( i = 0; i < unit_count; i++ )
        list_add( units, load_unit( file ) );


    list_clear( reinf );
    unit_count = load_int( file );
    for ( i = 0; i < unit_count; i++ )
        list_add( reinf, load_unit( file ) );
    list_clear( avail_units );
    unit_count = load_int( file );
    for ( i = 0; i < unit_count; i++ )
        list_add( avail_units, load_unit( file ) );
    /* карты */
    map_w = load_int( file );
    map_h = load_int( file );
    for ( i = 0; i < map_w; i++ )
        for ( j = 0; j < map_h; j++ ) {
            load_map_tile_units( file, &map[i][j].g_unit, &map[i][j].a_unit );
            if ( map[i][j].g_unit )
                map[i][j].g_unit->terrain = terrain_type_find( map[i][j].terrain_id[0] );
            if ( map[i][j].a_unit )
                map[i][j].a_unit->terrain = terrain_type_find( map[i][j].terrain_id[0] );
        }
    for ( i = 0; i < map_w; i++ )
        for ( j = 0; j < map_h; j++ )
            load_map_tile_flag( file, &map[i][j].nation, &map[i][j].player );
    /* не может быть развёртывания (они не могут быть сохранены) */
    deploy_turn = 0;
    fclose( file );
    return 1;
}
