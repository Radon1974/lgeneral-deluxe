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

#include <SDL.h>
#include <stdlib.h>
#include "lgeneral.h"
#include "sdl.h"
#include "date.h"
#include "event.h"
#include "image.h"
#include "list.h"
#include "windows.h"
#include "unit.h"
#include "purchase_dlg.h"
#include "gui.h"
#include "localize.h"
#include "scenario.h"
#include "file.h"
#include "campaign.h"

extern Sdl sdl;
extern GUI *gui;
extern Nation *nations;
extern int nation_count;
extern int nation_flag_width, nation_flag_height;
extern Unit_Class *unit_classes;
extern int unit_class_count;
extern Player *cur_player;
extern List *unit_lib;
extern Scen_Info *scen_info;
extern SDL_Surface *nation_flags;
extern Trgt_Type *trgt_types;
extern int trgt_type_count;
extern Mov_Type *mov_types;
extern int mov_type_count;
extern List *reinf;
extern List *avail_units;
extern List *units;
extern int turn;
extern Config config;
extern enum CampaignState camp_loaded;
extern Unit *cur_unit;
extern int status;

/****** Частный *****/

/** Выносят флаг страны и название народа @data на поверхность @buffer. */
static void render_lbox_nation( void *data, SDL_Surface *buffer )
{
	Nation *n = (Nation*)data;
	char name[10];

	SDL_FillRect( buffer, 0, 0x0 );
	gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_CENTER;

	nation_draw_flag( n, buffer, 2, 0, NATION_DRAW_FLAG_NORMAL );
	snprintf(name,10,"%s",n->name); /* truncate name */
        write_text( gui->font_std, buffer, 4 + nation_flags->w, buffer->h/2, name, 255 );
}

/** Рендеринг имя класса юнита @data на поверхность @buffer. */
static void render_lbox_uclass( void *data, SDL_Surface *buffer )
{
	Unit_Class *c = (Unit_Class*)data;
	char name[13];

	SDL_FillRect( buffer, 0, 0x0 );
	gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_CENTER;
	snprintf(name,13,"%s",c->name); /* усечение имени */
        write_text( gui->font_std, buffer, 2, buffer->h/2, name, 255 );
}

/** Значок рендеринга и имя записи unit lib @data на поверхность @buffer. */
static void render_lbox_unit_lib_entry( void *data, SDL_Surface *buffer )
{
	Unit_Lib_Entry *e = (Unit_Lib_Entry*)data;
	char name[13];

	SDL_FillRect( buffer, 0, 0x0 );
	gui->font_std->align = ALIGN_X_CENTER | ALIGN_Y_BOTTOM;

        DEST( buffer, (buffer->w - e->icon_w)/2, (buffer->h - e->icon_h)/2,
							e->icon_w, e->icon_h );
        SOURCE( e->icon, 0, 0 );
        blit_surf();

	snprintf(name,13,"%s",e->name); /* усечение имени */
        write_text( gui->font_std, buffer, buffer->w / 2, buffer->h, name, 255 );
}

/** Значок рендеринга и название блока reinf @data на поверхность @buffer. */
static void render_lbox_reinf( void *data, SDL_Surface *buffer )
{
	Unit *u = (Unit*)data;

	render_lbox_unit_lib_entry(&u->prop, buffer);
}

/* Выберите те страны, у которых игрок может приобрести юниты.
 * Возврат в виде нового списка с указателями на глобальные страны (не будет удален
 * при удалении списка) */
List *get_purchase_nations( void )
{
	List *l = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
	int i;

	if (l == NULL) {
		fprintf( stderr, tr("Out of memory\n") );
		return NULL;
	}

	for (i = 0; i < cur_player->nation_count; i++)
		if (cur_player->nations[i]->no_purchase == 0)
			list_add( l, cur_player->nations[i] );
	return l;
}

/* Выберите те классы единиц измерения, из которых могут быть приобретены единицы измерения.
 * Возврат в виде нового списка с указателями на глобальные классы единиц измерения (не будет удален
 * при удалении списка) */
static List *get_purchase_unit_classes(void)
{
	List *l = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
	int i;

	if (l == NULL) {
		fprintf( stderr, tr("Out of memory\n") );
		return NULL;
	}

	for (i = 0; i < unit_class_count; i++)
		if (unit_classes[i].purchase == UC_PT_NORMAL)
			list_add( l, &unit_classes[i] );
	return l;
}

/** Возвратный указатель на класс наземного транспортера (приобретается для единиц с
 * флаг GROUND_TRSP_OK). */
static Unit_Class *get_purchase_trsp_class()
{
	int i;
	for (i = 0; i < unit_class_count; i++)
		if (unit_classes[i].purchase == UC_PT_TRSP)
			return &unit_classes[i];
	return NULL;
}

/** Выберите те записи из библиотеки юнитов, которые соответствуют определенным критериям:
 * @nationid: единица измерения принадлежит нации с этой строкой идентификатора (если есть NULL)
 * @uclassid: блок относится к классу с таким идентификатором строки (если значение null, любое)
 * @date: единица измерения все еще производится в это время (если NULL есть)
 * Стоимость должна быть > 0.
 *
 * Возврат в виде нового списка с указателями на записи unit lib (не будет удален
 * при удалении списка) */
List *get_purchasable_unit_lib_entries( const char *nationid,
				const char *uclassid, const Date *date )
{
	List *l = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
	Unit_Lib_Entry *e = NULL;

	if (l == NULL) {
		fprintf( stderr, tr("Out of memory\n") );
		return NULL;
	}

	list_reset(unit_lib);
	while ((e = list_next(unit_lib))) {
		if (e->cost <= 0)
			continue;
		if (nationid && (e->nation == -1 ||
				strcmp(nations[e->nation].id, nationid)))
			continue;
		if (uclassid && strcmp(unit_classes[e->class].id, uclassid))
			continue;
		if (date && !config.all_equipment) {
			if (e->start_year > date->year)
				continue;
			if (e->start_year == date->year && e->start_month >
								date->month)
				continue;
			if (e->last_year && e->last_year < date->year)
				continue;
		}
		list_add(l,e);
	}
	return l;
}

/** Получите все юниты из глобального списка reinf, которые принадлежат текущему игроку.
 * Такие юниты могут быть возвращены. Юниты в global avail_units не могут быть возвращены
 * так как уже прибыл и готов к развертыванию.
 *
 * Возврат в виде нового списка с указателями на глобальный список reinf (не будет удален
 * при удалении списка) */
static List *get_reinf_units( void )
{
	List *l = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
	Unit *u = NULL;

	if (l == NULL) {
		fprintf( stderr, tr("Out of memory\n") );
		return NULL;
	}

	list_reset(reinf);
	while ((u = list_next(reinf)))
		if (u->player == cur_player)
			list_add( l, u );
    if (config.purchase == INSTANT_PURCHASE)
    {
        list_reset(avail_units);
        while ((u = list_next(avail_units)))
            if (u->player == cur_player)
                list_add( l, u );
    }
	return l;
}

/** Визуализация информации о покупке для записи unit lib @entry на поверхность @buffer at
 * позиция @x, @y. */
static void render_unit_lib_entry_info( Unit_Lib_Entry *entry,
					SDL_Surface *buffer, int x, int y )
{
	char str[128];
	int i, start_y = y;

	gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;

	/* первая колонка: name, type, ammo, fuel, spot, mov, ini, range */
	write_line( buffer, gui->font_std, entry->name, x, &y );
	write_line( buffer, gui->font_std,
				unit_classes[entry->class].name, x, &y );
	y += 5;
	sprintf( str, tr("Cost:       %3i"), entry->cost );
	write_line( buffer, gui->font_std, str, x, &y );
	if ( entry->ammo == 0 )
		sprintf( str, tr("Ammo:         --") );
	else
		sprintf( str, tr("Ammo:        %2i"), entry->ammo );
	write_line( buffer, gui->font_std, str, x, &y );
	if ( entry->fuel == 0 )
		sprintf( str, tr("Fuel:        --") );
	else
		sprintf( str, tr("Fuel:        %2i"), entry->fuel );
	write_line( buffer, gui->font_std, str, x, &y );
	y += 5;
	sprintf( str, tr("Spotting:    %2i"), entry->spt );
	write_line( buffer, gui->font_std, str, x, &y );
	sprintf( str, tr("Movement:    %2i"), entry->mov );
	write_line( buffer, gui->font_std, str, x, &y );
	sprintf( str, tr("Initiative:  %2i"), entry->ini );
	write_line( buffer, gui->font_std, str, x, &y );
	sprintf( str, tr("Range:       %2i"), entry->rng );
	write_line( buffer, gui->font_std, str, x, &y );

	/* вторая колонка: move type, target type, attack, defense */
	y = start_y;
	x += 135;
	sprintf( str, tr("%s Movement"), mov_types[entry->mov_type].name );
	write_line( buffer, gui->font_std, str, x, &y );
	sprintf( str, tr("%s Target"), trgt_types[entry->trgt_type].name );
	write_line( buffer, gui->font_std, str, x, &y );
	y += 5;
	for ( i = 0; i < trgt_type_count; i++ ) {
		char valstr[8];
		int j;

		sprintf( str, tr("%s Attack:"), trgt_types[i].name );
		for (j = strlen(str); j < 14; j++)
			strcat( str, " " );
		if ( entry->atks[i] < 0 )
			snprintf( valstr, 8, "[%2i]", -entry->atks[i] );
		else
			snprintf( valstr, 8, "  %2i", entry->atks[i] );
		strcat(str, valstr);
		write_line( buffer, gui->font_std, str, x, &y );
	}
	y += 5;
	sprintf( str, tr("Ground Defense: %2i"), entry->def_grnd );
	write_line( buffer, gui->font_std, str, x, &y );
	sprintf( str, tr("Air Defense:    %2i"), entry->def_air );
	write_line( buffer, gui->font_std, str, x, &y );
	sprintf( str, tr("Close Defense:  %2i"), entry->def_cls );
	write_line( buffer, gui->font_std, str, x, &y );
}

/** Подсчитайте, сколько единиц игрок может приобрести: Unit limit - размещенные единицы -
 * заказать блоки - развертывания подразделений. Верните число или -1, если нет ограничения. */
int player_get_purchase_unit_limit( Player *player, int type )
{
    int limit;
    if (type == CORE)
        limit = player->core_limit;
    else
        limit = player->unit_limit;
	Unit *u = NULL;

	if (limit == 0)
		return -1;

    if ((type == AUXILIARY) && (camp_loaded != NO_CAMPAIGN) && config.use_core_units)
        limit = player->unit_limit - player->core_limit;
	/* единицы измерения, размещенные на карте */
	list_reset( units );
	while ((u = list_next( units )))
    {
        if (u->player == player && u->killed == 0 && u->core == type )
                limit--;
    }
	/* отряды заказаны, еще не прибыли */
	list_reset( reinf );
	while ((u = list_next( reinf )))
        if (u->player == player && u->killed == 0 && u->core == type )
			limit--;
	/* подразделения прибыли, еще не размещенные */
    list_reset( avail_units );
    while ((u = list_next( avail_units )))
        if (u->player == player && u->killed == 0 && u->core == type )
            limit--;

	return limit;
}

/** Проверить, является ли игрок @р можно купить блок записи, Либ @блок с
 * transporter @trsp (если не NULL) относительно доступного престижа и юнита
 * емкость. Верните 1, если покупка в порядке, 0 в противном случае.
 * Не проверяется, действительно ли @unit может иметь @trsp в качестве транспортера,
 * это должно быть утверждено перед вызовом этой функции. */
int player_can_purchase_unit( Player *p, Unit_Lib_Entry *unit,
							Unit_Lib_Entry *trsp)
{
	int cost = unit->cost + ((trsp)?trsp->cost:0);

    if (!config.use_core_units)
    {
        if (player_get_purchase_unit_limit(p,AUXILIARY) == 0)
            return 0;
    }
    else
    {
        if ( (player_get_purchase_unit_limit(p,AUXILIARY) == 0) && (camp_loaded == NO_CAMPAIGN) )
            return 0;
        else
            if (player_get_purchase_unit_limit(p,CORE) + player_get_purchase_unit_limit(p,AUXILIARY) == 0 &&
               (camp_loaded != NO_CAMPAIGN))
                return 0;
    }
	if (p->cur_prestige < cost)
		return 0;
	return 1;
}

/** Визуализация информации о выбранной единице измерения (единица измерения, транспортер, общая стоимость, ... )
 * и включить/отключить кнопку покупки в зависимости от того, достаточно ли престижа
 * и емкость. */
static void update_unit_purchase_info( PurchaseDlg *pdlg )
{
	int total_cost = 0;
	SDL_Surface *contents = pdlg->main_group->frame->contents;
	Unit_Lib_Entry *unit_entry = NULL, *trsp_entry = NULL;
	Unit *reinf_unit = NULL;

	SDL_FillRect( contents, 0, 0x0 );

	/* получить выбранные объекты */
	reinf_unit = lbox_get_selected_item( pdlg->reinf_lbox );
	if (reinf_unit) {
		unit_entry = &reinf_unit->prop;
		if (reinf_unit->trsp_prop.id)
			trsp_entry = &reinf_unit->trsp_prop;
	} else {
		unit_entry = lbox_get_selected_item( pdlg->unit_lbox );
		trsp_entry = lbox_get_selected_item( pdlg->trsp_lbox );
	}

	/* информация об юните */
	if (unit_entry) {
		render_unit_lib_entry_info( unit_entry, contents, 10, 10 ); //вывод информации о выбранном юните
		total_cost += unit_entry->cost;

		/* если транспортер не разрешен, очистите список транспортеров. если позволено
		 * но список пуст, заполните его. */
		if (reinf_unit)
			; /* unit/trsp уже очищен */
		else if(!unit_has_flag( unit_entry, "ground_trsp_ok" )) {
			lbox_clear_items(pdlg->trsp_lbox);
			trsp_entry = NULL;
		} else if (lbox_is_empty(pdlg->trsp_lbox) && pdlg->trsp_uclass)
			lbox_set_items(pdlg->trsp_lbox,
					get_purchasable_unit_lib_entries(
					pdlg->cur_nation->id,
					pdlg->trsp_uclass->id,
					(const Date*)&scen_info->start_date));
	}

	/* информация о транспортере */
	if (trsp_entry) {
		render_unit_lib_entry_info( trsp_entry, contents, 10, 155 ); //вывод информации о транспорте выбранного юнита
		total_cost += trsp_entry->cost;
	}

	/* показать стоимость, если есть выбор; отметить красным, если нет возможности покупки */
	if ( total_cost > 0 ) {
		char str[128];
		Font *font = gui->font_std;
		int y = group_get_height( pdlg->main_group ) - 10;

		snprintf( str, 128, tr("Total Cost: %d"), total_cost );
		if (!reinf_unit && !player_can_purchase_unit(cur_player,
						unit_entry, trsp_entry)) {
			font = gui->font_error;
			total_cost = 0; /* здесь указывается "не подлежит покупке". */
		}
		font->align = ALIGN_X_LEFT | ALIGN_Y_BOTTOM;
		write_text( font, contents, 10, y, str, 255 );

		if (total_cost > 0) {
			font->align = ALIGN_X_RIGHT | ALIGN_Y_BOTTOM;
			write_text( font, contents,
				group_get_width( pdlg->main_group ) - 62, y,
				reinf_unit?tr("REFUND?"):tr("REQUEST?"), 255 );
		}
	}

	/* если общая стоимость установлена, то возможна покупка */
	if (total_cost > 0)
		group_set_active( pdlg->main_group, ID_PURCHASE_OK, 1 );
	else
		group_set_active( pdlg->main_group, ID_PURCHASE_OK, 0 );

	/* нанесите содержимое */
	frame_apply( pdlg->main_group->frame ); //выведите все на экран информации юнита
}

/** Ограничение тока изменить @pdlg отдела добавив в этот список добавить и отобразить юниты. */
static void update_purchase_unit_limit( PurchaseDlg *pdlg, int add, int type )
{
	SDL_Surface *contents = pdlg->ulimit_frame->contents;
	char str[16];
	int y = 7;

    if (type == CORE)
    {
        if (pdlg->cur_core_unit_limit != -1)
        {
            pdlg->cur_core_unit_limit += add;
            if (pdlg->cur_core_unit_limit < 0)
                pdlg->cur_core_unit_limit = 0;
        }
    }
    else
    {
        if (pdlg->cur_unit_limit != -1)
        {
            pdlg->cur_unit_limit += add;
            if (pdlg->cur_unit_limit < 0)
                pdlg->cur_unit_limit = 0;
	    }
    }

	SDL_FillRect( contents, 0, 0x0 );
	gui->font_std->align = ALIGN_X_CENTER | ALIGN_Y_TOP;
    if (!config.use_core_units)
        write_line(contents, gui->font_std, tr("Available:"), contents->w/2, &y );
    else
    {
        write_line(contents, gui->font_std, tr("Core:"), contents->w/2, &y );
        if (pdlg->cur_core_unit_limit == -1)
            strcpy( str, tr("None") );
        else
            snprintf(str, 16, "%d %s", pdlg->cur_core_unit_limit,
                    (pdlg->cur_core_unit_limit == 1)?tr("Unit"):tr("Units"));
        write_line( contents, gui->font_std, str, contents->w/2, &y );
        write_line(contents, gui->font_std, tr("Auxiliary:"), contents->w/2, &y );
    }
	if (pdlg->cur_unit_limit == -1)
		strcpy( str, tr("None") );
	else
		snprintf(str, 16, "%d %s", pdlg->cur_unit_limit,
			(pdlg->cur_unit_limit==1)?tr("Unit"):tr("Units"));
	write_line( contents, gui->font_std, str, contents->w/2, &y );
	frame_apply( pdlg->ulimit_frame );  //выведите все на экран лимитов юнита
}

/** Найдите юнит @u в списке подкреплений. Если он найден, удалите его и верните деньги
 * престиж для игрока. */
static void player_refund_unit( Player *p, Unit *u )
{
	Unit *e;

	list_reset(reinf);
	while ((e = list_next(reinf))) {
		if (e != u)
			continue;
		p->cur_prestige += u->prop.cost;
		if (u->trsp_prop.id)
			p->cur_prestige += u->trsp_prop.cost;
		list_delete_current(reinf);
		return;
	}
    if (config.purchase == INSTANT_PURCHASE)
    {
        list_reset(avail_units);
        while ((e = list_next(avail_units))) {
            if (e != u)
                continue;
            p->cur_prestige += u->prop.cost;
            if (u->trsp_prop.id)
                p->cur_prestige += u->trsp_prop.cost;
            list_delete_current(avail_units);
            return;
        }
    }
}

/** Ручка нажмите на кнопку покупки: Покупка юнита в соответствии с настройками в
 * @pdlg и обновление информации и reinf. Не проверяется, действительно ли текущий игрок
* может приобрести это устройство. Это должно быть утверждено прежде чем называть это
 * функция. */
static void handle_purchase_button( PurchaseDlg *pdlg )
{
	Nation *nation = lbox_get_selected_item( pdlg->nation_lbox );
	Unit_Lib_Entry *unit_prop = lbox_get_selected_item( pdlg->unit_lbox );
	Unit_Lib_Entry *trsp_prop = lbox_get_selected_item( pdlg->trsp_lbox );
	Unit *reinf_unit = lbox_get_selected_item( pdlg->reinf_lbox );

        if (reinf_unit == NULL) {

            if (pdlg->cur_core_unit_limit > 0)  //если есть лимит в основные юниты, то купить в основной, иначе во вспомогательный
            {
                player_purchase_unit( cur_player, nation, CORE, unit_prop, trsp_prop ); //купить основной юнит
                update_purchase_unit_limit( pdlg, -1, CORE );
            }
            else
            {
                player_purchase_unit( cur_player, nation, AUXILIARY, unit_prop, trsp_prop );
                update_purchase_unit_limit( pdlg, -1, AUXILIARY ); //купить вспомогательный юнит
            }
        }
        else
        {
            player_refund_unit( cur_player, reinf_unit );
            update_purchase_unit_limit( pdlg, 1, reinf_unit->core );
        }
	lbox_set_items( pdlg->reinf_lbox, get_reinf_units() );
	update_unit_purchase_info( pdlg );
}

/** Покупка юнита для игрока @player. @nation - это нация юнита (должна совпадать
 * со списком игроков, но не проверено), @type является ОСНОВНЫМ или ВСПОМОГАТЕЛЬНЫМ,
 * @unit_prop и @trsp_prop-это записи * unit lib (trsp_prop может быть NULL
 * без транспортера). */
void player_purchase_unit( Player *player, Nation *nation, int type,
			Unit_Lib_Entry *unit_prop, Unit_Lib_Entry *trsp_prop )
{
	Unit *unit = NULL, unit_base;
	    /* информация о базовом юните сборки */
        memset( &unit_base, 0, sizeof( Unit ) );
        unit_base.nation = nation;
        unit_base.player = player;
        unit_base.max_str = 10;
        unit_base.orient = cur_player->orient;

        /* FIXME: нет глобального счетчика для единицы измерения числа, поэтому используйте простое имя */
      snprintf(unit_base.name, sizeof(unit_base.name), "%s", unit_prop->name);
        if (config.purchase != INSTANT_PURCHASE)
            unit_base.delay = turn + 1; /* доступен следующий поворот */
        else
            unit_base.delay = turn; /* доступно прямо сейчас */
        unit_base.str = 10;

        /* создаем юнита и добавляем в список reinf  */
        if ((unit = unit_create( unit_prop, trsp_prop, 0, &unit_base )) == NULL) {
            fprintf( stderr, tr("Out of memory\n") );
            return;
        }
        unit->core = type;

        if (config.purchase != INSTANT_PURCHASE)
            list_add( reinf, unit );
        else
            list_add( avail_units, unit );

        /* заплати за это */
        player->cur_prestige -= unit_prop->cost;
        if (trsp_prop)
            player->cur_prestige -= trsp_prop->cost;
}

/****** Публичный *****/

/** Создайте и верните указатель на диалог покупки. Используйте графику из темы
 * путь @theme_path. */
PurchaseDlg *purchase_dlg_create( char *theme_path )
{
	PurchaseDlg *pdlg = NULL;
	char path[MAX_PATH], transitionPath[MAX_PATH];
	int sx, sy;

	pdlg = calloc( 1, sizeof(PurchaseDlg) );
	if (pdlg == NULL)
		return NULL;

	/* создать основную группу (= main window) */
    search_file_name( path, 0, "confirm_buttons", theme_path, "", 'i' );
	pdlg->main_group = group_create( gui_create_frame( 300, 320 ), 160,
				load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ),
				20, 20, 2, ID_PURCHASE_OK,
				sdl.screen, 0, 0 );
	if (pdlg->main_group == NULL)
		goto failure;
	sx = group_get_width( pdlg->main_group ) - 60;
	sy = group_get_height( pdlg->main_group ) - 25;
	group_add_button( pdlg->main_group, ID_PURCHASE_OK, sx, sy, 0,
							tr("Ok"), 2 );
	group_add_button( pdlg->main_group, ID_PURCHASE_EXIT, sx + 30, sy, 0,
							tr("Exit"), 2 );

	/* создать информационный фрейм ограничения юнита */
    pdlg->ulimit_frame = frame_create( gui_create_frame( 112, 65 ), 160,
                     sdl.screen, 0, 0);
	if (pdlg->ulimit_frame == NULL)
		goto failure;

	/* создать список наций */
    search_file_name( path, 0, "scroll_buttons", theme_path, "", 'i' );
	pdlg->nation_lbox = lbox_create( gui_create_frame( 112, 74 ), 160, 6,
			load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
			3, 1, 100, 12, 1, 0x0000ff,
			render_lbox_nation, sdl.screen, 0, 0);
	if (pdlg->nation_lbox == NULL)
		goto failure;

	/* создать список классов */
    search_file_name( path, 0, "scroll_buttons", theme_path, "", 'i' );
	pdlg->uclass_lbox = lbox_create( gui_create_frame( 112, 166 ), 160, 6,
			load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
			10, 2, 100, 12, 1, 0x0000ff,
			render_lbox_uclass, sdl.screen, 0, 0);
	if (pdlg->uclass_lbox == NULL)
		goto failure;

	/* создать список юнитов */
    search_file_name( path, 0, "scroll_buttons", theme_path, "", 'i' );
	pdlg->unit_lbox = lbox_create( gui_create_frame( 112, 200 ), 160, 6,
			load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
			4, 3, 100, 40, 1, 0x0000ff,
			render_lbox_unit_lib_entry, sdl.screen, 0, 0);
	if (pdlg->unit_lbox == NULL)
		goto failure;

	/* создать список транспортеров */
    search_file_name( path, 0, "scroll_buttons", theme_path, "", 'i' );
	pdlg->trsp_lbox = lbox_create( gui_create_frame( 112, 120 ), 160, 6,
			load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
			2, 1, 100, 40, 1, 0x0000ff,
			render_lbox_unit_lib_entry, sdl.screen, 0, 0);
	if (pdlg->trsp_lbox == NULL)
		goto failure;

	/* создать список подкреплений */
    search_file_name( path, 0, "scroll_buttons", theme_path, "", 'i' );
	pdlg->reinf_lbox = lbox_create( gui_create_frame( 112, 280 ), 160, 6,
			load_surf( path, SDL_SWSURFACE, 0, 0, 0, 0 ), 24, 24,
			6, 3, 100, 40, 1, 0x0000ff,
			render_lbox_reinf, sdl.screen, 0, 0);
	if (pdlg->reinf_lbox == NULL)
		goto failure;

	return pdlg;
failure:
	fprintf(stderr, tr("Failed to create purchase dialogue\n"));
	purchase_dlg_delete(&pdlg);
	return NULL;
}

void purchase_dlg_delete( PurchaseDlg **pdlg )
{
	if (*pdlg) {
		PurchaseDlg *ptr = *pdlg;

		group_delete( &ptr->main_group );
		frame_delete( &ptr->ulimit_frame );
		lbox_delete( &ptr->nation_lbox );
		lbox_delete( &ptr->uclass_lbox );
		lbox_delete( &ptr->unit_lbox );
		lbox_delete( &ptr->trsp_lbox );
		lbox_delete( &ptr->reinf_lbox );
		free( ptr );
	}
	*pdlg = NULL;
}

/** Ширина возвращение диалога купли @pdlg путем суммирования всех компонентов. */
int purchase_dlg_get_width(PurchaseDlg *pdlg)
{
	return lbox_get_width( pdlg->nation_lbox ) +
		lbox_get_width( pdlg->unit_lbox ) +
		group_get_width( pdlg->main_group ) +
		lbox_get_width( pdlg->reinf_lbox );
}

/** Верните высоту диалога покупки @pdlg, сложив все компоненты. */
int purchase_dlg_get_height(PurchaseDlg *pdlg)
{
	return group_get_height( pdlg->main_group );
}

/** Переместите диалог покупки @pdlg в позицию @px, @py, переместив все отдельно
 * компоненты. */
void purchase_dlg_move( PurchaseDlg *pdlg, int px, int py)
{
	int sx = px, sy = py;
	int ulimit_offset = (group_get_height( pdlg->main_group ) -
			frame_get_height( pdlg->ulimit_frame ) -
			lbox_get_height( pdlg->nation_lbox ) -
			lbox_get_height( pdlg->uclass_lbox )) / 2;
	int reinf_offset = (group_get_height( pdlg->main_group ) -
			lbox_get_height( pdlg->reinf_lbox )) / 2;

	frame_move( pdlg->ulimit_frame, sx, sy + ulimit_offset);
	sy += frame_get_height( pdlg->ulimit_frame );
	lbox_move(pdlg->nation_lbox, sx, sy + ulimit_offset);
	sy += lbox_get_height( pdlg->nation_lbox );
	lbox_move(pdlg->uclass_lbox, sx, sy + ulimit_offset);
	sx += lbox_get_width( pdlg->nation_lbox );
	sy = py;
	lbox_move(pdlg->unit_lbox, sx, sy );
	sy += lbox_get_height( pdlg->unit_lbox );
	lbox_move(pdlg->trsp_lbox, sx, sy );
	sx += lbox_get_width( pdlg->unit_lbox );
	sy = py;
	group_move(pdlg->main_group, sx, sy);
	sx += group_get_width( pdlg->main_group );
	lbox_move(pdlg->reinf_lbox, sx, sy + reinf_offset );
}

/* Скрыть, нарисовать, нарисовать фон, получить фон для диалога покупки @pdlg,
применив действие ко всем отдельным компонентам. */
void purchase_dlg_hide( PurchaseDlg *pdlg, int value)
{
	group_hide(pdlg->main_group, value);
	frame_hide(pdlg->ulimit_frame, value);
	lbox_hide(pdlg->nation_lbox, value);
	lbox_hide(pdlg->uclass_lbox, value);
	lbox_hide(pdlg->unit_lbox, value);
	lbox_hide(pdlg->trsp_lbox, value);
	lbox_hide(pdlg->reinf_lbox, value);
}
void purchase_dlg_draw( PurchaseDlg *pdlg)
{
	group_draw(pdlg->main_group);
	frame_draw(pdlg->ulimit_frame);
	lbox_draw(pdlg->nation_lbox);
	lbox_draw(pdlg->uclass_lbox);
	lbox_draw(pdlg->unit_lbox);
	lbox_draw(pdlg->trsp_lbox);
	lbox_draw(pdlg->reinf_lbox);
}
void purchase_dlg_draw_bkgnd( PurchaseDlg *pdlg)
{
	group_draw_bkgnd(pdlg->main_group);
	frame_draw_bkgnd(pdlg->ulimit_frame);
	lbox_draw_bkgnd(pdlg->nation_lbox);
	lbox_draw_bkgnd(pdlg->uclass_lbox);
	lbox_draw_bkgnd(pdlg->unit_lbox);
	lbox_draw_bkgnd(pdlg->trsp_lbox);
	lbox_draw_bkgnd(pdlg->reinf_lbox);
}
void purchase_dlg_get_bkgnd( PurchaseDlg *pdlg)
{

	group_get_bkgnd(pdlg->main_group);
	frame_get_bkgnd(pdlg->ulimit_frame);
	lbox_get_bkgnd(pdlg->nation_lbox);
	lbox_get_bkgnd(pdlg->uclass_lbox);
	lbox_get_bkgnd(pdlg->unit_lbox);
	lbox_get_bkgnd(pdlg->trsp_lbox);
	lbox_get_bkgnd(pdlg->reinf_lbox);
}

/** Обработайте движение мыши для диалога покупки @pdlg, проверив все компоненты.
 * @cx, @cy-абсолютное положение мыши на экране. Возврат 1, если действие было
 * обрабатывается каким-то окном, в противном случае 0. */
int purchase_dlg_handle_motion( PurchaseDlg *pdlg, int cx, int cy)
{
	int ret = 1;
	void *item = NULL;

	if (!group_handle_motion(pdlg->main_group,cx,cy))
	if (!lbox_handle_motion(pdlg->nation_lbox,cx,cy,&item))
	if (!lbox_handle_motion(pdlg->uclass_lbox,cx,cy,&item))
	if (!lbox_handle_motion(pdlg->unit_lbox,cx,cy,&item))
	if (!lbox_handle_motion(pdlg->trsp_lbox,cx,cy,&item))
	if (!lbox_handle_motion(pdlg->reinf_lbox,cx,cy,&item))
		ret = 0;
	return ret;
}

/** Ручка мыши щелкает по экрану позиции @cx,@cy с помощью кнопки @bid.
 * Верните 1, если была нажата кнопка, которая нуждается в восходящей обработке (например, чтобы
* закрыть диалог), и установите @pbtn. В противном случае обработайте событие внутренне и
 * возвращает 0. */
int purchase_dlg_handle_button( PurchaseDlg *pdlg, int bid, int cx, int cy,
		Button **pbtn )
{
	void *item = NULL;


	if (group_handle_button(pdlg->main_group,bid,cx,cy,pbtn)) {
		/* поймать и обработать кнопку покупки внутри */
		if ((*pbtn)->id == ID_PURCHASE_OK) {
            if ( status == 15 ) handle_modify_button( pdlg ); //запуск процедуры покупки юнита
			else handle_purchase_button( pdlg ); //запуск процедуры покупки юнита

			return 0;
		}
		return 1;
	}
	if (lbox_handle_button(pdlg->nation_lbox,bid,cx,cy,pbtn,&item)) {
		if ( item && item != pdlg->cur_nation ) {
			pdlg->cur_nation = (Nation*)item;
			if ( status == 15 ) update_modify_units(pdlg);
			else update_purchasable_units(pdlg);
		}
		return 0;
	}
	if (lbox_handle_button(pdlg->uclass_lbox,bid,cx,cy,pbtn,&item)) {
		if ( item && item != pdlg->cur_uclass ) {
			pdlg->cur_uclass = (Unit_Class*)item;
			if ( status == 15 ) update_modify_units(pdlg);
			else update_purchasable_units(pdlg);
		}
		return 0;
	}
	if (lbox_handle_button(pdlg->unit_lbox,bid,cx,cy,pbtn,&item)) {
		if (item) {
			/* очистите выбор reinf, так как мы делимся информационным окном */
			lbox_clear_selected_item( pdlg->reinf_lbox );
			update_unit_purchase_info( pdlg );
		}
		return 0;
	}
	if (lbox_handle_button(pdlg->trsp_lbox,bid,cx,cy,pbtn,&item)) {
		if (item) {
			if (bid == BUTTON_RIGHT) /* отменить выбор записи */
				lbox_clear_selected_item( pdlg->trsp_lbox );
			/* очистите выбор reinf, так как мы делимся информационным окном */
			lbox_clear_selected_item( pdlg->reinf_lbox );
			update_unit_purchase_info( pdlg );
		}
		return 0;
	}
	if (lbox_handle_button(pdlg->reinf_lbox,bid,cx,cy,pbtn,&item)) {
		if (item) {
			/* очистите выбор юнита, так как мы делимся информационным окном */
			lbox_clear_selected_item( pdlg->unit_lbox );
			lbox_clear_items( pdlg->trsp_lbox );
			update_unit_purchase_info( pdlg );
		}
		return 0;
	}
	return 0;
}



/** Обновите списки (и очистите текущий выбор) для блока и транспортера
 * в соответствии с текущими настройками в @pdlg. */
void update_purchasable_units( PurchaseDlg *pdlg )
{
        lbox_set_items(pdlg->unit_lbox, get_purchasable_unit_lib_entries(
                    pdlg->cur_nation->id, pdlg->cur_uclass->id,
                    (const Date*)&scen_info->start_date));
        lbox_clear_items(pdlg->trsp_lbox);  //удалите старый список элементов
        update_unit_purchase_info(pdlg); /* очистить информацию */
}

/** Сброс настроек диалога покупки для глобальных @cur_player */
void purchase_dlg_reset( PurchaseDlg *pdlg )
{

	lbox_set_items( pdlg->nation_lbox, get_purchase_nations() );
	lbox_set_items( pdlg->uclass_lbox, get_purchase_unit_classes() );

	pdlg->cur_nation = lbox_select_first_item( pdlg->nation_lbox );
	if (pdlg->cur_nation == NULL) /* список пуст */
		pdlg->cur_nation = cur_player->nations[0];
	pdlg->cur_uclass = lbox_select_first_item( pdlg->uclass_lbox );
	if (pdlg->cur_uclass == NULL) /* список пуст */
		pdlg->cur_uclass = &unit_classes[0];
	pdlg->trsp_uclass = get_purchase_trsp_class();
    pdlg->cur_unit_limit = player_get_purchase_unit_limit( cur_player, AUXILIARY );
    if (config.use_core_units)
        pdlg->cur_core_unit_limit = player_get_purchase_unit_limit( cur_player, CORE );

	lbox_set_items( pdlg->reinf_lbox, get_reinf_units() );
	update_purchasable_units(pdlg);
    if (config.use_core_units)
        update_purchase_unit_limit(pdlg, 0, CORE);
    update_purchase_unit_limit(pdlg, 0, AUXILIARY);
}





/****** Модификация юнита *****/

/****** Частный *****/

/** Обновите списки (и очистите текущий выбор) для блока и транспортера
 * в соответствии с текущими настройками в @pdlg. */
void update_modify_units( PurchaseDlg *pdlg )
{
        if ( ( ( nation_get_index( pdlg->cur_nation )) == cur_unit->prop.nation ) && ( units_id_class( pdlg->cur_uclass->id ) == cur_unit->prop.class ) ) {
        lbox_set_items(pdlg->unit_lbox, get_purchasable_unit_lib_entries(
                    pdlg->cur_nation->id, pdlg->cur_uclass->id,
                    (const Date*)&scen_info->start_date));
        lbox_clear_items(pdlg->trsp_lbox);  //удалите старый список элементов
        update_unit_purchase_info(pdlg); /* очистить информацию */
        }
}

/** Покупка юнита для игрока @player. @nation - это нация юнита (должна совпадать
 * со списком игроков, но не проверено), @type является ОСНОВНЫМ или ВСПОМОГАТЕЛЬНЫМ,
 * @unit_prop и @trsp_prop-это записи * unit lib (trsp_prop может быть NULL
 * без транспортера). */
void player_modify_unit( Player *player, Nation *nation, int type,
			Unit_Lib_Entry *unit_prop, Unit_Lib_Entry *trsp_prop )
{
	//Unit *unit = NULL, unit_base;
	    /* информация о базовом юните сборки */
        //memset( &unit_base, 0, sizeof( Unit ) );
        //unit_base.nation = nation;
        //unit_base.player = player;
        //unit_base.max_str = 10;
        //unit_base.orient = cur_player->orient;

        //if ((unit = unit_create( unit_prop, trsp_prop, 0, &unit_base )) == NULL) {
        //    fprintf( stderr, tr("Out of memory\n") );
        //    return;
        //}

        strrem( cur_unit->name, cur_unit->prop.name );  //удаление части имени (оставить номер подразделения)
        /* заплати за это */
        player->cur_prestige = player->cur_prestige - ( unit_prop->cost - cur_unit->prop.cost );
        if (trsp_prop)
            player->cur_prestige = player->cur_prestige - ( trsp_prop->cost - cur_unit->trsp_prop.cost );

        memcpy( &cur_unit->prop, unit_prop, sizeof( Unit_Lib_Entry ) );
        strcat( cur_unit->name, cur_unit->prop.name );  //новое имя юнита
        if ( trsp_prop ) memcpy( &cur_unit->trsp_prop, trsp_prop, sizeof( Unit_Lib_Entry ) );
        //memcpy( &cur_unit->land_trsp_prop, 0, sizeof( Unit_Lib_Entry ) );




        //cur_unit->sel_prop = unit->sel_prop;
        //cur_unit->backup = unit->backup;
        //cur_unit->nation = unit->nation;
        //cur_unit->supply_level = unit->supply_level;






    //if ( prop == 0 ) return 0;

    /* неглубокая копия собственности */
    //memcpy( &unit->prop, prop, sizeof( Unit_Lib_Entry ) );
    //unit->sel_prop = &unit->prop;
    //unit->embark = EMBARK_NONE;
    /* назначить пропущенный транспортер без проверки */
    //if ( trsp_prop && !unit_has_flag( prop, "flying" ) && !unit_has_flag( prop, "swimming" ) ) {
    //    memcpy( &unit->trsp_prop, trsp_prop, sizeof( Unit_Lib_Entry ) );
        /* по умолчанию активен наземный транспортер море / воздух */
        //if ( unit_has_flag( trsp_prop, "swimming" ) ) {
            //unit->embark = EMBARK_SEA;
           // unit->sel_prop = &unit->trsp_prop;
           // if ( land_trsp_prop )
                //memcpy( &unit->land_trsp_prop, land_trsp_prop, sizeof( Unit_Lib_Entry ) );
        //}
        //if ( unit_has_flag( trsp_prop, "flying" ) ) {
            //unit->embark = EMBARK_AIR;
            //unit->sel_prop = &unit->trsp_prop;
        //}
    //}
    /* скопировать базовые значения */
    //unit->delay = base->delay;
    //unit->x = base->x; unit->y = base->y;
    //unit->str = base->str; unit->entr = base->entr;
    //unit->max_str = base->max_str;
//    unit->cur_str_repl = 0;
    //unit->player = base->player;
    //unit->nation = base->nation;
    //strcpy_lt( unit->name, base->name, 20 );
    //unit_add_exp( unit, base->exp_level * 100 );
    //unit->orient = base->orient;
    //unit_adjust_icon( unit );
    //unit->unused = 1;
    cur_unit->supply_level = 100;
    cur_unit->cur_ammo = cur_unit->prop.ammo;
    cur_unit->cur_fuel = cur_unit->prop.fuel;
    //unit->core = base->core;
    if ( cur_unit->cur_fuel == 0 && cur_unit->trsp_prop.id && cur_unit->trsp_prop.fuel > 0 )
        cur_unit->cur_fuel = cur_unit->trsp_prop.fuel;
    //strcpy_lt( unit->tag, base->tag, 31 );
    /* обновить свойства индикатора жизни */
    //update_bar( unit );
    /* выделить резервную память */
    //unit->backup = calloc( 1, sizeof( Unit ) );
    unit_set_as_used(cur_unit);
}

/** Ручка нажмите на кнопку покупки: Покупка юнита в соответствии с настройками в
 * @pdlg и обновление информации и reinf. Не проверяется, действительно ли текущий игрок
* может приобрести это устройство. Это должно быть утверждено прежде чем называть это
 * функция. */
void handle_modify_button( PurchaseDlg *pdlg )
{
	Nation *nation = lbox_get_selected_item( pdlg->nation_lbox );
	Unit_Lib_Entry *unit_prop = lbox_get_selected_item( pdlg->unit_lbox );
	Unit_Lib_Entry *trsp_prop = lbox_get_selected_item( pdlg->trsp_lbox );
	//Unit *reinf_unit = lbox_get_selected_item( pdlg->reinf_lbox );

    player_modify_unit( cur_player, nation, CORE, unit_prop, trsp_prop ); //модифицировать основной юнит

}

/* ID класса */
int units_id_class( char *uclass ) {
            int j;
            for ( j = 0; j < unit_class_count; j++ )
                if ( STRCMP( uclass, unit_classes[j].id ) ) {
                    return j;
                }
    return  -1;
}

/* Удаление части текста из строки */
void strrem( char* _pSourceStr, const char* _pDelStr )
{
char* pTmpStr;

pTmpStr = strstr( _pSourceStr, _pDelStr );

strcpy( pTmpStr, pTmpStr + strlen( _pDelStr ) );
};

/****** Публичный *****/

/** Сброс настроек диалога покупки для глобальных @cur_player */
void modify_dlg_reset( PurchaseDlg *pdlg )
{

	lbox_set_items( pdlg->nation_lbox, get_purchase_nations() );
	lbox_set_items( pdlg->uclass_lbox, get_purchase_unit_classes() );

	pdlg->cur_nation = lbox_select_first_item( pdlg->nation_lbox );
	if (pdlg->cur_nation == NULL) /* список пуст */
		pdlg->cur_nation = cur_player->nations[0];
	pdlg->cur_uclass = lbox_select_first_item( pdlg->uclass_lbox );
	if (pdlg->cur_uclass == NULL) /* список пуст */
		pdlg->cur_uclass = &unit_classes[0];
	pdlg->trsp_uclass = get_purchase_trsp_class();
    pdlg->cur_unit_limit = player_get_purchase_unit_limit( cur_player, AUXILIARY );
    if (config.use_core_units)
        pdlg->cur_core_unit_limit = player_get_purchase_unit_limit( cur_player, CORE );

	//lbox_set_items( pdlg->reinf_lbox, get_reinf_units() );
	update_purchasable_units(pdlg);
    if (config.use_core_units)
        update_purchase_unit_limit(pdlg, 0, CORE);
    update_purchase_unit_limit(pdlg, 0, AUXILIARY);

    lbox_clear_items ( pdlg->reinf_lbox );  //очистить юниты в окне покупок
    lbox_clear_items ( pdlg->unit_lbox );  //очистить юниты в окне юнитов
    lbox_clear_items ( pdlg->trsp_lbox );  //очистить юниты в окне транспорта юнитов
    //lbox_clear_selected_item ( pdlg->reinf_lbox );
    //lbox_clear_selected_item ( pdlg->unit_lbox );
    //PurchaseDlg *pdlg = NULL;
    //purchase_dlg_delete(&pdlg);
    //SDL_Surface *pdlg = NULL;
    //SDL_FreeSurface( pdlg->main_group->img );
    //group_delete( pdlg->main_group );


}


