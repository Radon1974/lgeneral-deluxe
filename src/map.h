/***************************************************************************
                          map.h  -  description
                             -------------------
    begin                : Sat Jan 20 2001
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

#ifndef __MAP_H
#define __MAP_H

#include "terrain.h"
#include "nation.h"
#include "player.h"
#include "unit.h"

#define MAX_LAYERS 5

enum { FAIR = 0, CLOUDS, RAIN, SNOW };

/*
====================================================================
Плитка карты
====================================================================
*/
typedef struct {
    char *name;             /* имя этого фрагмента карты */
    int terrain_id[MAX_LAYERS]; /* id свойств местности */
    int image_offset_x;     /* смещение изображения в prop->image */
    int image_offset_y;     /* смещение изображения в prop->image */
    int strat_image_offset; /* смещение в списке крошечных стратегических изображений местности */
    Nation *nation;         /* нация, владеющая этим флагом (NULL == нет нации) */
    Player *player;         /* вот */
    int obj;                /* военный объект? */
    int deploy_center;      /* развертывание разрешено? */
    Unit *g_unit;           /* указатель наземной / морской части */
    Unit *a_unit;           /* указатель воздушного блока */
    int layers[MAX_LAYERS]; /* слои этого фрагмента карты; слой 1 - дороги */
} Map_Tile;

/*
====================================================================
Для определения различных элементов карты (развертывание, обнаружение, блокировка ...)
маска карты используется, и это флаги для нее.
====================================================================
*/
enum {
    F_FOG =             ( 1L << 1 ),
    F_SPOT =            ( 1L << 2 ),
    F_IN_RANGE =        ( 1L << 3 ),
    F_MOUNT =           ( 1L << 4 ),
    F_SEA_EMBARK =      ( 1L << 5 ),
    F_AUX =             ( 1L << 6 ),
    F_INFL =            ( 1L << 7 ),
    F_INFL_AIR =        ( 1L << 8 ),
    F_VIS_INFL =        ( 1L << 9 ),
    F_VIS_INFL_AIR =    ( 1L << 10 ),
    F_BLOCKED =         ( 1L << 11 ),
    F_BACKUP =          ( 1L << 12 ),
    F_MERGE_UNIT =      ( 1L << 13),
    F_INVERSE_FOG =     ( 1L << 14 ), /* инверсия F_FOG */
    F_DEPLOY =          ( 1L << 15 ),
    F_CTRL_GRND =       ( 1L << 17 ),
    F_CTRL_AIR =        ( 1L << 18 ),
    F_CTRL_SEA =        ( 1L << 19 ),
    F_MOVE_COST =       ( 1L << 20 ),
    F_DANGER =          ( 1L << 21 ),
    F_SPLIT_UNIT =      ( 1L << 22 ),
    F_DISTANCE =        ( 1L << 23 )
};

/*
====================================================================
Map mask tile.
====================================================================
*/
typedef struct {
    int fog; /* если true, двигатель покрывает эту плитку туманом. если установлен ENGINE_MODIFY_FOG
                этот туман может меняться в зависимости от действия (дальность действия юнита, партнеров по слиянию
                и т.д */
    int spot; /* истина, если кто-либо из ваших юнитов наблюдает за этим тайлом карты; вы можете атаковать только юниты
    на плитке карты, которую вы заметили */
    /* используется для выбранного юнита */
    int in_range; /* это используется для поиска пути; это -1, если тайл не в диапазоне, иначе он установлен на
    оставшиеся движущиеся точки агрегата; влияние врага не учитывается */
    int distance; /* простое расстояние до текущего объекта; используется для маски опасности */
    int moveCost; /* общие затраты на достижение этой плитки */
    int blocked; /* юниты могут перемещаться туда вместе с союзным юнитом, но они не должны на этом останавливаться;
    поэтому разрешите переход к плитке, только если in_range и! заблокированы */
    int mount; /* истина, если юнит должен смонтировать, чтобы добраться до этой плитки */
    int sea_embark; /* возможен выход в море? */
    int infl; /* эта маска устанавливается в начале хода игрока; каждая плитка рядом с
    инфляция вражеского юнита увеличивается; если влияние равно 1, затраты на перемещение удваиваются; если влияние> = 2
    эта плитка непроходима (отряд останавливается на этой плитке); если юнит не видит плитку с infl> = 2 и
    пытается переместиться туда, он остановится на этой плитке; не зависит от движущихся точек юнита, проходящих через
    под влиянием тайла стоит все мов-поинты */
    int vis_infl; /* аналог infl, но только пятнистые единицы вносят вклад в эту маску; используется для установки
    маска in_range */
    int air_infl; /* аналог для летательных аппаратов */
    int vis_air_infl;
    int aux; /* используется для установки любого из верхних значений */
    int backup; /* используется для резервного копирования точечной маски для отмены перемещения юнита */
    Unit *merge_unit; /* если не NULL, это указатель на юнита, который вызвал map_get_merge_units ()
                         может сливаться с. вам нужно запомнить другой юнит, так как она не сохраняется здесь */
    Unit *split_unit; /* целевой отряд может передать силу */
    int  split_okay; /* юнит может передать новый подблок на этот тайл */
    int deploy; /* маска развертывания: 1: отряд может развернуть свои, 0 отряд не может их развернуть; настройка с помощью deploy.c */
    int danger; /* 1: отметьте эту плитку как опасную для входа */
    /* Маски ИИ */
    int ctrl_grnd; /* маска контролируемой зоны для игрока. собственные подразделения дают положительный бой
                      значение в движении + дальность атаки, в то время как очки врага вычитаются. финал
                      значение для каждой плитки относительно наивысшего абсолютного контрольного значения, таким образом
                      колеблется от -1000 до 1000 */
    int ctrl_air;
    int ctrl_sea; /* каждая рабочая область имеет свою маску управления */
} Mask_Tile;


/*
====================================================================
Загрузить карту.
====================================================================
*/
int map_load( char *fname );

/*
====================================================================
Удалить карту.
====================================================================
*/
void map_delete( );

/*
====================================================================
Получить плитку в x, y
====================================================================
*/
Map_Tile* map_tile( int x, int y );
Mask_Tile* map_mask_tile( int x, int y );

/*
====================================================================
Очистите переданные флаги маски карты.
====================================================================
*/
void map_clear_mask( int flags );

/*
====================================================================
Поменять местами юниты. Возвращает предыдущую единицу или 0, если нет.
====================================================================
*/
Unit *map_swap_unit( Unit *unit );

/*
====================================================================
Вставить, удалить указатель объекта с карты.
====================================================================
*/
void map_insert_unit( Unit *unit );
void map_remove_unit( Unit *unit );

/*
====================================================================
Получите соседние плитки по часовой стрелке с идентификатором от 0 до 5.
====================================================================
*/
Map_Tile* map_get_close_hex( int x, int y, int id );

/*
====================================================================
Добавить / установить засвет юнита во вспомогательную маску
====================================================================
*/
void map_add_unit_spot_mask( Unit *unit );
void map_get_unit_spot_mask( Unit *unit );

/*
====================================================================
Установите диапазон движения юнита на in_range / sea_embark / mount.
====================================================================
*/
void map_get_unit_move_mask( Unit *unit );
void map_clear_unit_move_mask();

/*
====================================================================
Воссоздает маску опасности для «юнита».
Для этого туман должен быть установлен на диапазон движения «юнит».
функция для правильной работы.
Стоимость движения маски должна быть установлена ​​как «юнит».
Возвращает 1, если установлена ​​хотя бы одна маска опасности для плитки, в противном случае - 0.
====================================================================
*/
int map_get_danger_mask( Unit *unit );

/*
====================================================================
Получите список путевых точек, по которым юнит движется к месту назначения.
Это включает проверку невидимого влияния вражеских юнитов (например,
Сюрприз-контакт).
====================================================================
*/
typedef struct {
    int x, y;
    int cost;
} Way_Point;



Way_Point* map_get_unit_way_points( Unit *unit, int x, int y, int *count, Unit **ambush_unit );

/*
====================================================================
Резервное копирование / восстановление точечной маски в / из резервной маски. Используется для отмены поворота.
====================================================================
*/
void map_backup_spot_mask();
void map_restore_spot_mask();

/*
====================================================================
Получить партнеров по слиянию юнита и установить маску слияния.
Максимум единиц MAP_MERGE_UNIT_LIMIT.
Все неиспользуемые записи в партнерах устанавливаются на 0.
====================================================================
*/
enum { MAP_MERGE_UNIT_LIMIT = 6 };
void map_get_merge_units( Unit *unit, Unit **partners, int *count );

/*
====================================================================
Проверьте, может ли юнит передать силу юниту (если не NULL) или создать
автономный модуль (если unit NULL) по координатам.
====================================================================
*/
int map_check_unit_split( Unit *unit, int str, int x, int y, Unit *dest );

/*
====================================================================
Получите разделенных партнеров юнита, предполагая, что юнит хочет дать силу 'str'
точки и установите маску «разделение». Максимум единиц MAP_SPLIT_UNIT_LIMIT.
Все неиспользованные записи в партнерах устанавливаются на 0. 'str' должно быть допустимым количеством,
здесь это не проверяется.
====================================================================
*/
enum { MAP_SPLIT_UNIT_LIMIT = 6 };
void map_get_split_units_and_hexes( Unit *unit, int str, Unit **partners, int *count );

/*
====================================================================
Получите список (vis_units) всех видимых единиц, проверив маску пятна.
====================================================================
*/
void map_get_vis_units( void );

/*
====================================================================
Нарисуйте на поверхности фрагмент карты местности. (затуманивается, если установлена ​​маска :: туман)
====================================================================
*/
void map_draw_terrain( SDL_Surface *surf, int map_x, int map_y, int x, int y );
/*
====================================================================
Нарисуйте блоки плитки. Если установлена ​​маска :: fog, юниты не отрисовываются.
Если 'ground' имеет значение True, наземный блок отображается как основной.
и воздушный блок нарисован маленьким (и наоборот).
Если установлено 'select', добавляется рамка выбора.
====================================================================
*/
void map_draw_units( SDL_Surface *surf, int map_x, int map_y, int x, int y, int ground, int select );
/*
====================================================================
Нарисуйте плитку опасности. Ожидает, что "surf" будет содержать полностью нарисованную плитку в
данная позиция, которая будет окрашена наложением опасности
поверхность местности.
====================================================================
*/
void map_apply_danger_to_tile( SDL_Surface *surf, int map_x, int map_y, int x, int y );
/*
====================================================================
Нарисуйте местность и юниты.
====================================================================
*/
void map_draw_tile( SDL_Surface *surf, int map_x, int map_y, int x, int y, int ground, int select );

/*
====================================================================
Установить / обновить маску пятна текущим игроком движка.
Обновление добавляет плитки, видимые юнитом.
====================================================================
*/
void map_set_spot_mask();
void map_update_spot_mask( Unit *unit, int *enemy_spotted );

/*
====================================================================
Установите маска :: туман (который является фактическим туманом двигателя) либо
точечная маска, маска in_range (закрывает sea_embark), маска слияния,
развернуть маску.
====================================================================
*/
void map_set_fog( int type );

/*
====================================================================
Установите туман на маску пятна игрока, используя маска :: aux (не маска :: место)
====================================================================
*/
void map_set_fog_by_player( Player *player );

/*
====================================================================
Убедитесь, что этот фрагмент карты виден движку (не покрыт
by mask :: fog или mask :: spot, так как модификация разрешена и может быть
туман другого игрока (например, один человек против процессора))
====================================================================
*/
#define MAP_CHECK_VIS( mapx, mapy ) ( ( !modify_fog && !mask[mapx][mapy].fog ) || ( modify_fog && mask[mapx][mapy].spot ) )

/*
====================================================================
Измените различные маски влияния.
====================================================================
*/
void map_add_unit_infl( Unit *unit );
void map_remove_unit_infl( Unit *unit );
void map_remove_vis_unit_infl( Unit *unit );
void map_set_infl_mask();
void map_set_vis_infl_mask();

/*
====================================================================
Проверить, может ли установка производить посадку в воздух / море / высадку в точке x, y.
Если 'init'! = 0, используются упрощенные правила для развертывания
====================================================================
*/
int map_check_unit_embark( Unit *unit, int x, int y, int type, int init );
int map_check_unit_debark( Unit *unit, int x, int y, int type, int init );

/*
====================================================================
Посадить / высадить юнит и вернуться, если был обнаружен враг.
Если «вражеское пятно» равно 0, не пересчитывать маску пятна.
Если координаты объекта или x и y выходят за границы, соответствующие
плиткой не манипулируют.
====================================================================
*/
void map_embark_unit( Unit *unit, int x, int y, int type, int *enemy_spotted );
void map_debark_unit( Unit *unit, int x, int y, int type, int *enemy_spotted );

/*
====================================================================
Установите маску развертывания для этого устройства. Если 'init', используйте начальное развертывание
маска (или по умолчанию). Если нет, установите действующие центры развертывания. В
Во втором прогоне удалите любую плитку, заблокированную собственным юнитом, если установлено значение «unit».
====================================================================
*/
void map_get_deploy_mask( Player *player, Unit *unit, int init );

/*
====================================================================
Отметьте, что это поле является полем развертывания для данного игрока.
====================================================================
*/
void map_set_deploy_field( int mx, int my, int player );

/*
====================================================================
Проверьте, является ли это поле полем развертывания для данного игрока.
====================================================================
*/
int map_is_deploy_field( int mx, int my, int player );

/*
====================================================================
Проверьте, можно ли развернуть модуль на mx, my или вернуть неразвертываемый модуль
там. Если установлен air_mode, сначала проверяется воздушный блок.
«игрок» - это индекс игрока.
====================================================================
*/
int map_check_deploy( Unit *unit, int mx, int my, int player, int init, int air_mode );
Unit* map_get_undeploy_unit( int x, int y, int air_region );

/*
====================================================================
Проверьте уровень поставки плитки (mx, my) в контексте «единицы».
(шестиугольные плитки с SUPPLY_GROUND имеют 100% запас)
====================================================================
*/
int map_get_unit_supply_level( int mx, int my, Unit *unit );
/*
====================================================================
Проверьте, является ли этот фрагмент карты точкой снабжения данного отряда.
====================================================================
*/
int map_is_allied_depot( Map_Tile *tile, Unit *unit );
/*
====================================================================
Проверяет, поставляется ли этот гексагон (mx, my) складом в
контекст «юнита».
====================================================================
*/
int map_supplied_by_depot( int mx, int my, Unit *unit );

/*
====================================================================
Получить зону высадки для юнита (все близкие гексы, которые свободны).
====================================================================
*/
void map_get_dropzone_mask( Unit *unit );

#endif
