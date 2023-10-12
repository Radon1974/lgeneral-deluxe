/***************************************************************************
                          engine.c  -  description
                             -------------------
    begin                : Sat Feb 3 2001
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

#ifdef USE_DL
  #include <dlfcn.h>
#endif
#include <math.h>
#include "lgeneral.h"
#include "date.h"
#include "event.h"
#include "windows.h"
#include "nation.h"
#include "unit.h"
#include "purchase_dlg.h"
#include "headquarters_dlg.h"
#include "message_dlg.h"
#include "gui.h"
#include "map.h"
#include "scenario.h"
#include "slot.h"
#include "action.h"
#include "strat_map.h"
#include "campaign.h"
#include "ai.h"
#include "engine.h"
#include "localize.h"
#include "file.h"


/*
====================================================================
Внешние
====================================================================
*/
extern Sdl sdl;
extern Config config;
extern int cur_weather; /* используется в unit.c для вычисления влияния погоды на юниты; устанавливается set_player () */
extern Nation *nations;
extern int nation_count;
extern int nation_flag_width, nation_flag_height;
extern int hex_w, hex_h;
extern int hex_x_offset, hex_y_offset;
extern Terrain_Icons *terrain_icons;
extern int map_w, map_h;
extern Weather_Type *weather_types;
extern List *players;
extern int turn;
extern List *units;
extern List *reinf;
extern List *avail_units;
extern Scen_Info *scen_info;
extern Map_Tile **map;
extern Mask_Tile **mask;
extern GUI *gui;
extern int camp_loaded;
extern Camp_Entry *camp_cur_scen;
extern Setup setup;
extern int  term_game, sdl_quit;
extern char *camp_first;
extern char scen_result[64];
extern char scen_message[128];
extern VCond *vconds;
extern int vcond_count;
extern int cheat_endscn;
extern List *prev_scen_core_units;
extern char *prev_scen_fname;

/*
====================================================================
Двигатель
====================================================================
*/
int modify_fog = 0;      /* если это False, туман, инициированный
                            map_set_fog () сохраняется на протяжении всего хода
                            иначе он обновляется маской движения и т. д. */
int cur_ctrl = 0;        /* текущий тип управления (равно player-> ctrl, если установлено иначе
                            это PLAYER_CTRL_NOBODY) */
Player *cur_player = 0;  /* указатель текущего игрока */
typedef struct {         /* резервное копирование */
    int used;
    Unit unit;          /* мелкая копия юнита */
    /* используется для сброса флага карты, если юнит захватил его */
    int flag_saved;     /* эти значения использовали? */
    Nation *dest_nation;
    Player *dest_player;
} Move_Backup;
Move_Backup move_backup = { 0 }; /* резервное копирование, чтобы отменить последний ход */
int fleeing_unit = 0;    /* если это правда, движение юнита не дублируется */
int air_mode = 0;        /* воздушные юниты являются первичными */
int end_scen = 0;        /* Истинно, если сценарий завершен или прерван */
List *left_deploy_units = 0; /* список с указателями единиц на avail_units всех
                                юнитов, которые еще не размещены */
Unit *deploy_unit = 0;   /* текущий отряд выбран в списке развертывания */
Unit *surrender_unit = 0;/* юнит, которая сдастся */
Unit *move_unit = 0;     /* в настоящее время движется юнит */
Unit *surp_unit = 0;     /* если задано cur_unit, у этого юнита есть сюрприз при движении */
Unit *cur_unit = 0;      /* выбранный в данный момент юнит (человеком) */
Unit *cur_target = 0;    /* цель cur_unit */
Unit *cur_atk = 0;       /* текущий атакующий - если не оборонительный огонь, это
                            идентично cur_unit */
Unit *cur_def = 0;       /* текущий защитник - идентичен cur_target
                            если не оборонительный огонь (то это cur_unit) */
List *df_units = 0;      /* это список оборонительных огневых подразделений, поддерживающих
                            cur_target. пока этот список не пустой cur_unit
                            становится cur_def, а cur_atk - текущая защита
                            Блок. если этот список пуст на последнем шаге
                            cur_unit и cur_target действительно сражаются
                            если атака не была остановлена */
int  defFire = 0;        /* бой поддерживает, поэтому поменяйте жертвы */
int merge_unit_count = 0;
Unit *merge_units[MAP_MERGE_UNIT_LIMIT];  /* список партнеров по слиянию для cur_unit */
int split_unit_count = 0;
Unit *split_units[MAP_SPLIT_UNIT_LIMIT]; /* список разделенных партнеров */
int cur_split_str = 0;
/* ДИСПЛЕЙ */
enum {
    SC_NONE = 0,
    SC_VERT,
    SC_HORI
};
int sc_type = 0, sc_diff = 0;  /* тип копии экрана. используется для ускорения полного обновления карты */
SDL_Surface *sc_buffer = 0;    /* буфер копирования экрана */
int *hex_mask = 0;             /* используется для определения шестнадцатеричного числа по указателю pos */
int map_x, map_y;              /* текущее положение на карте */
int map_sw, map_sh;            /* количество плиток, выводимых на экран */
int map_sx, map_sy;            /* позиция, где нарисовать первую плитку */
int draw_map = 0;              /* если этот флаг истинен, engine_update () вызывает engine_draw_map () */
enum {
    SCROLL_NONE = 0,
    SCROLL_LEFT,
    SCROLL_RIGHT,
    SCROLL_UP,
    SCROLL_DOWN
};
int scroll_hori = 0, scroll_vert = 0; /* направления прокрутки, если есть */
int blind_cpu_turn = 0;        /* если это правда, все движения скрыты */
Button *last_button = 0;       /* последняя нажатая кнопка. используется для сброса состояния выключения при отпускании кнопки */
/* РАЗНОЕ */
int old_mx = -1, old_my = -1;  /* последний фрагмент карты, на котором находился курсор */
int old_region = -1;           /* регион на фрагменте карты */
int scroll_block_keys = 0;     /* блокировка клавиш для прокрутки */
int scroll_block = 0;          /* прокрутка блока, если задана постоянная
                                  скорость прокрутки */
int scroll_time = 100;         /* одна прокрутка каждые миллисекунды scroll_time */
Delay scroll_delay;            /* используется для тайм-аута оставшихся миллисекунд */
char *slot_name;               /* название слота, в который сохраняется игра */
Delay blink_delay;             /* используется для мигания точек на стратегической карте */
/* ДЕЙСТВИЕ */
enum {
    STATUS_NONE = 0,           /* действия, которые разделены на разные фазы
                                  установить этот статус */
    STATUS_MOVE,               /* переместить юнит по "пути" */
    STATUS_ATTACK,             /* юнит атакует cur_target (включая защитный огонь) */
    STATUS_MERGE,              /* человек может сливаться с юнитами */
    STATUS_SPLIT,              /* человек хочет разделить юнит */
    STATUS_DEPLOY,             /* человек может развернуть юниты */
    STATUS_DROP,               /* выбрать зону сброса парашютистов */
    STATUS_INFO,               /* показать полную информацию об юните */
    STATUS_SCEN_INFO,          /* показать информацию о сценарии */
    STATUS_CONF,               /* запустить окно подтверждения */
    STATUS_UNIT_MENU,          /* кнопки управления юнитом */
    STATUS_GAME_MENU,          /* игровое меню */
    STATUS_DEPLOY_INFO,        /* полная информация об юните при развертывании */
    STATUS_STRAT_MAP,          /* показывая стратегическую карту */
    STATUS_RENAME,             /* переименовать блок */
    STATUS_SAVE_EDIT,          /* запуск сохранения редактирования */
    STATUS_SAVE_MENU,          /* запуск меню сохранения */
    STATUS_LOAD_MENU,          /* запуск меню загрузки */
    STATUS_MOD_SELECT,         /* меню выбора запущенного мода */
    STATUS_NEW_FOLDER,         /* создание нового имени папки */
    STATUS_TITLE,              /* показать фон */
    STATUS_TITLE_MENU,         /* запустить меню заголовков */
    STATUS_RUN_SCEN_DLG,       /* запустить диалог сценария */
    STATUS_RUN_CAMP_DLG,       /* вести диалог кампании */
    STATUS_RUN_SCEN_SETUP,     /* запустить настройку сценария */
    STATUS_RUN_CAMP_SETUP,     /* запустить настройку кампании */
    STATUS_RUN_MODULE_DLG,     /* выберите модуль ИИ */
    STATUS_CAMP_BRIEFING,      /* запустить информационный диалог кампании */
    STATUS_PURCHASE,           /* запустить диалог покупки единицы */
    STATUS_VMODE_DLG,          /* запустить выбор режима видео */
    STATUS_HEADQUARTERS        /* вести диалог в штаб-квартире */
};
int status;                    /* статусы, определенные в engine_tools.h */
enum {
    PHASE_NONE = 0,
    /* БОЙ */
    PHASE_INIT_ATK,             /* инициировать атаку крест */
    PHASE_SHOW_ATK_CROSS,       /* атакующий крест */
    PHASE_SHOW_DEF_CROSS,       /* защитник крест */
    PHASE_COMBAT,               /* вычислить и получить урон */
    PHASE_EVASION,              /* подлодка уклоняется */
    PHASE_RUGGED_DEF,           /* остановите двигатель на некоторое время и отобразите сообщение о защите */
    PHASE_PREP_EXPLOSIONS,      /* настроить взрывы */
    PHASE_SHOW_EXPLOSIONS,      /* анимировать оба взрыва */
    PHASE_FIGHT_MSG,			/* показать сообщения о статусе боя */
    PHASE_CHECK_RESULT,         /* очистите этот бой и инициируйте следующий, если таковой будет */
    PHASE_BROKEN_UP_MSG,        /* при необходимости отображать разорванное сообщение */
    PHASE_SURRENDER_MSG,        /* показать сообщение о сдаче */
    PHASE_END_COMBAT,           /* очистить статус и перерисовать */
    /* ДВИЖЕНИЕ */
    PHASE_INIT_MOVE,            /* начать движение */
    PHASE_START_SINGLE_MOVE,    /* начать движение к следующей путевой точке из текущей позиции */
    PHASE_RUN_SINGLE_MOVE,      /* выполнить одиночное движение и по завершении вызвать START_SINGLE_MOVEMENT */
    PHASE_CHECK_LAST_MOVE,      /* проверить последний единственный ход на неожиданный контакт, захват флага, конец сценария */
    PHASE_END_MOVE              /* завершить движение */
};
int phase;
Way_Point *way = 0;             /* путевые точки для движения */
int way_length = 0;
int way_pos = 0;
int dest_x, dest_y;             /* конечная точка пути */
Image *move_image = 0;          /* изображение, которое содержит графику движущегося объекта */
float move_vel = 0.3;           /* пикселей в миллисекунду */
Delay move_time;                /* время, которое занимает одно движение */
typedef struct {
    float x,y;
} Vector;
Vector unit_vector;             /* плавающая позиция анимации */
Vector move_vector;             /* вектор, по которому движется юнит */
int surp_contact = 0;           /* истина, если текущий бой является результатом неожиданного контакта */
int atk_result = 0;             /* результат атаки */
Delay msg_delay;                /* задержка разорванного сообщения */
int atk_took_damage = 0;
int def_took_damage = 0;        /* если True, при атаке отображается взрыв */
int atk_damage_delta;			/* дельта повреждений для атакующего юнита */
int atk_suppr_delta;			/* дельта подавления для атакующего юнита */
int def_damage_delta;			/* дельта повреждений обороняющегося юнита */
int def_suppr_delta;			/* дельта подавления для обороняющейся единицы */
static int has_danger_zone;		/* есть ли опасные зоны */
int deploy_turn;				/* 1, если это очередь развертывания */
static Action *top_committed_action;	/* самое главное действие, которое нельзя удалять */
static struct MessagePane *camp_pane; 	/* состояние панели сообщений кампании */
static char *last_debriefing;   /* текст последнего подведения итогов */
int log_x, log_y = 2;
char log_str[MAX_NAME];

/*
====================================================================
Местное
====================================================================
*/

/*
====================================================================
Перенаправлено
====================================================================
*/
static void engine_draw_map();
static void engine_update_info( int mx, int my, int region );
static void engine_set_status( int newstat );
static void engine_show_final_message( void );
static void engine_select_player( Player *player, int skip_unit_prep );
static int engine_capture_flag( Unit *unit );
static void engine_show_game_menu( int cx, int cy );
static void engine_handle_button( int id );

/*
====================================================================
Завершите сценарий и отобразите последнее сообщение.
====================================================================
*/
static void engine_finish_scenario()
{
    /* доработать ход ИИ, если есть */
    if ( cur_player && cur_player->ctrl == PLAYER_CTRL_CPU )
        (cur_player->ai_finalize)();
    /* продолжить кампанию, если таковая имеется */
    if (camp_loaded == FIRST_SCENARIO)
        camp_loaded = CONTINUE_CAMPAIGN;
    blind_cpu_turn = 0;
    engine_show_final_message();
    group_set_active( gui->base_menu, ID_MENU, 0 );
    draw_map = 1;
    image_hide( gui->cursors, 0 );
    gui_set_cursor( CURSOR_STD );
    engine_select_player( 0, 0 );
    turn = scen_info->turn_limit;
    engine_set_status( STATUS_NONE );
    phase = PHASE_NONE;
    if (camp_loaded == CONTINUE_CAMPAIGN)
		scen_save_core_units(); /* храните основные блоки для следующего сценария */
}

/*
====================================================================
Верните первого игрока-человека.
====================================================================
*/
static Player *engine_human_player(int *human_count)
{
    Player *human = 0;
    int count = 0;
    int i;
    for ( i = 0; i < players->count; i++ ) {
        Player *player = list_get( players, i );
        if ( player->ctrl == PLAYER_CTRL_HUMAN ) {
            if ( count == 0 )
                human = player;
            count++;
        }
    }
    if (human_count) *human_count = count;
    return human;
}

/*
====================================================================
Очистите опасную зону.
====================================================================
*/
static void engine_clear_danger_mask()
{
    if ( has_danger_zone ) {
        map_clear_mask( F_DANGER );
        has_danger_zone = 0;
    }
}

/*
====================================================================
Установить желаемый статус.
====================================================================
*/
static void engine_set_status( int newstat )
{
    if ( newstat == STATUS_NONE && setup.type == SETUP_RUN_TITLE )
    {
        status = STATUS_TITLE;
        /* повторно показать главное меню */
        if (!term_game) engine_show_game_menu(10,40);
    }
    /* вернуться из сценария на титульный экран */
    else if ( newstat == STATUS_TITLE )
    {
        status = STATUS_TITLE;
        if (!term_game) engine_show_game_menu(10,40);
    }
    else
        status = newstat;
}

/*
====================================================================
Нарисуйте обои и фон.
====================================================================
*/
static void engine_draw_bkgnd()
{
    int i, j;
    for ( j = 0; j < sdl.screen->h; j += gui->wallpaper->h )
        for ( i = 0; i < sdl.screen->w; i += gui->wallpaper->w ) {
            DEST( sdl.screen, i, j, gui->wallpaper->w, gui->wallpaper->h );
            SOURCE( gui->wallpaper, 0, 0 );
            blit_surf();
        }
    DEST( sdl.screen,
          ( sdl.screen->w - gui->bkgnd->w ) / 2,
          ( sdl.screen->h - gui->bkgnd->h ) / 2,
          gui->bkgnd->w, gui->bkgnd->h );
    SOURCE( gui->bkgnd, 0, 0 );
    blit_surf();
    log_x = sdl.screen->w - 2;
    log_y = 2;
    gui->font_error->align = ALIGN_X_RIGHT | ALIGN_Y_TOP;
    write_line( sdl.screen, gui->font_error, log_str, log_x, &log_y );
    blit_surf();
}

/*
====================================================================
Возвращает true, когда имели место события закрытия экрана состояния.
====================================================================
*/
static int engine_status_screen_dismissed()
{
    int dummy;
    return event_get_buttonup( &dummy, &dummy, &dummy )
            || event_check_key(SDLK_SPACE)
            || event_check_key(SDLK_RETURN)
            || event_check_key(SDLK_ESCAPE);
}

#if 0
/*
====================================================================
Отобразите большое окно сообщения (например, брифинг) и дождитесь щелчка.
====================================================================
*/
static void engine_show_message( char *msg )
{
    struct MessagePane *pane = gui_create_message_pane();
    gui_set_message_pane_text(pane, msg);
    engine_draw_bkgnd();
    gui_draw_message_pane(pane);
    /* ждать */
    SDL_PumpEvents(); event_clear();
    while ( !engine_status_screen_dismissed() ) { SDL_PumpEvents(); SDL_Delay( 20 ); }
    event_clear();
    gui_delete_message_pane(pane);
}
#endif

/*
====================================================================
Сохраните разбор последнего сценария или пустую строку.
====================================================================
*/
static void engine_store_debriefing(const char *result)
{
    const char *str = camp_get_description(result);
    free(last_debriefing); last_debriefing = 0;
    if (str) last_debriefing = strdup(str);
}

/*
====================================================================
Подготовьте дисплей для брифинга следующей кампании.
====================================================================
*/
static void engine_prep_camp_brief()
{
    gui_delete_message_pane(camp_pane);
    camp_pane = 0;
    engine_draw_bkgnd();

    camp_pane = gui_create_message_pane();
    /* составить текст сценария */
    {
        char *txt = alloca((camp_cur_scen->title ? strlen(camp_cur_scen->title) + 2 : 0)
                           + (last_debriefing ? strlen(last_debriefing) + 1 : 0)
                           + (camp_cur_scen->brief ? strlen(camp_cur_scen->brief) : 0) + 2 + 1);
        strcpy(txt, "");
        if (camp_cur_scen->title) {
            strcat(txt, camp_cur_scen->title);
            strcat(txt, "##");
        }
        if (last_debriefing) {
            strcat(txt, last_debriefing);
            strcat(txt, " ");
        }
        if (camp_cur_scen->brief) {
            strcat(txt, camp_cur_scen->brief);
            strcat(txt, "##");
        }
        gui_set_message_pane_text(camp_pane, txt);
    }

    /* укажите параметры или идентификатор по умолчанию */
    if (camp_cur_scen->scen) {
      gui_set_message_pane_default(camp_pane, "nextscen");
    } else {
      List *ids = camp_get_result_list();
      List *vals = list_create(LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK);
      char *result;

      list_reset(ids);
      while ((result = list_next(ids))) {
          const char *desc = camp_get_description(result);
          list_add(vals, desc ? (void *)desc : (void *)result);
      }

      /* отсутствие идентификаторов означает завершение состояния */
      if (ids->count == 0)
          gui_set_message_pane_default(camp_pane, " ");
      /* не предоставляйте явный выбор, если есть только один идентификатор */
      else if (ids->count == 1)
          gui_set_message_pane_default(camp_pane, list_first(ids));
      else
          gui_set_message_pane_options(camp_pane, ids, vals);

      list_delete(ids);
      list_delete(vals);
    }

    gui_draw_message_pane(camp_pane);
    engine_set_status(STATUS_CAMP_BRIEFING);
}

/*
====================================================================
Проверьте кнопки меню и включите / отключите их в зависимости от состояния двигателя.
====================================================================
*/
static void engine_check_menu_buttons()
{
    /* режим авиации */
    group_set_active( gui->base_menu, ID_AIR_MODE, 1 );
    /* меню */
    if ( cur_ctrl == PLAYER_CTRL_NOBODY )
        group_set_active( gui->base_menu, ID_MENU, 0 );
    else
        group_set_active( gui->base_menu, ID_MENU, 1 );
    /* информация */
    if ( status != STATUS_NONE )
        group_set_active( gui->base_menu, ID_SCEN_INFO, 0 );
    else
        group_set_active( gui->base_menu, ID_SCEN_INFO, 1 );
    /* покупка */
    if ( (config.purchase != NO_PURCHASE) && cur_ctrl != PLAYER_CTRL_NOBODY && (!deploy_turn && status == STATUS_NONE ) )
        group_set_active( gui->base_menu, ID_PURCHASE, 1 );
    else
        group_set_active( gui->base_menu, ID_PURCHASE, 0 );
    /* развернуть */
    if ( avail_units )
    {
        if ( ( avail_units->count > 0 || deploy_turn ) && status == STATUS_NONE )
            group_set_active( gui->base_menu, ID_DEPLOY, 1 );
        else
            group_set_active( gui->base_menu, ID_DEPLOY, 0 );
    }
    /* стратегическая карта */
    if ( status == STATUS_NONE )
        group_set_active( gui->base_menu, ID_STRAT_MAP, 1 );
    else
        group_set_active( gui->base_menu, ID_STRAT_MAP, 0 );
    /* конец поворота */
    if ( status != STATUS_NONE )
        group_set_active( gui->base_menu, ID_END_TURN, 0 );
    else
        group_set_active( gui->base_menu, ID_END_TURN, 1 );
    /* условия победы */
    if ( status != STATUS_NONE )
        group_set_active( gui->base_menu, ID_CONDITIONS, 0 );
    else
        group_set_active( gui->base_menu, ID_CONDITIONS, 1 );
}

/*
====================================================================
Проверьте кнопки юнита. (использовать текущую юнит)
====================================================================
*/
static void engine_check_unit_buttons()
{
    char str[MAX_LINE];
    if ( cur_unit == 0 ) return;
    /* переименовать */
    group_set_active( gui->unit_buttons, ID_RENAME, 1 );
    /* поставка */
    if ( unit_check_supply( cur_unit, UNIT_SUPPLY_ANYTHING, 0, 0 ) ) {
        group_set_active( gui->unit_buttons, ID_SUPPLY, 1 );
        /* показать уровень предложения */
        sprintf( str, tr("Supply Unit [s] (Rate:%3i%%)"), cur_unit->supply_level );
        strcpy( group_get_button( gui->unit_buttons, ID_SUPPLY )->tooltip_center, str );
    }
    else
        group_set_active( gui->unit_buttons, ID_SUPPLY, 0 );
    /* поглощать */
    if (config.merge_replacements == OPTION_MERGE)
    {
        if ( merge_unit_count > 0 )
            group_set_active( gui->unit_buttons, ID_MERGE, 1 );
        else
            group_set_active( gui->unit_buttons, ID_MERGE, 0 );
    }
    /* расщеплять */
    if (unit_get_split_strength(cur_unit)>0)
        group_set_active( gui->unit_buttons, ID_SPLIT, 1 );
    else
        group_set_active( gui->unit_buttons, ID_SPLIT, 0 );
    /* замены */
    if (config.merge_replacements == OPTION_REPLACEMENTS)
    {
        if (unit_check_replacements(cur_unit,REPLACEMENTS) > 0)
        {
            group_set_active( gui->unit_buttons, ID_REPLACEMENTS, 1 );
            /* показать замену и уровень поставки */
            unit_check_supply( cur_unit, UNIT_SUPPLY_ANYTHING, 0, 0 );
            sprintf( str, tr("Supply Rate:%3i%%"), cur_unit->supply_level );
            strcpy( group_get_button( gui->unit_buttons, ID_REPLACEMENTS )->tooltip_left, str );
            sprintf( str, tr("Replacements") );
            strcpy( group_get_button( gui->unit_buttons, ID_REPLACEMENTS )->tooltip_center, str );
            sprintf( str, tr("Str:%2i, Exp:%4i, Prst:%4i"),
                     cur_unit->cur_str_repl, -cur_unit->repl_exp_cost, -cur_unit->repl_prestige_cost );
            strcpy( group_get_button( gui->unit_buttons, ID_REPLACEMENTS )->tooltip_right, str );
        }
        else
            group_set_active( gui->unit_buttons, ID_REPLACEMENTS, 0 );
        /* элитные замены */
        if (unit_check_replacements(cur_unit,ELITE_REPLACEMENTS) > 0)
        {
            group_set_active( gui->unit_buttons, ID_ELITE_REPLACEMENTS, 1 );
            /* показать элитные замены и уровень предложения */
            sprintf( str, tr("Supply Rate:%3i%%"), cur_unit->supply_level );
            strcpy( group_get_button( gui->unit_buttons, ID_ELITE_REPLACEMENTS )->tooltip_left, str );
            sprintf( str, tr("Elite Replacements") );
            strcpy( group_get_button( gui->unit_buttons, ID_ELITE_REPLACEMENTS )->tooltip_center, str );
            sprintf( str, tr("Str:%2i, Prst:%4i"), cur_unit->cur_str_repl, -cur_unit->repl_prestige_cost );
            strcpy( group_get_button( gui->unit_buttons, ID_ELITE_REPLACEMENTS )->tooltip_right, str );
        }
        else
            group_set_active( gui->unit_buttons, ID_ELITE_REPLACEMENTS, 0 );
    }
    /* отменить */
    if ( move_backup.used )
        group_set_active( gui->unit_buttons, ID_UNDO, 1 );
    else
        group_set_active( gui->unit_buttons, ID_UNDO, 0 );
    /* посадка в воздух */
    if ( map_check_unit_embark( cur_unit, cur_unit->x, cur_unit->y, EMBARK_AIR, 0 ) ||
         map_check_unit_debark( cur_unit, cur_unit->x, cur_unit->y, EMBARK_AIR, 0 ) )
        group_set_active( gui->unit_buttons, ID_EMBARK_AIR, 1 );
    else
        group_set_active( gui->unit_buttons, ID_EMBARK_AIR, 0 );
    /* распустить */
    if (cur_unit->unused)
        group_set_active( gui->unit_buttons, ID_DISBAND, 1 );
    else
        group_set_active( gui->unit_buttons, ID_DISBAND, 0 );
}

/*
====================================================================
Показать / скрыть полную информацию об юните при развертывании
====================================================================
*/
static void engine_show_deploy_unit_info( Unit *unit )
{
    status = STATUS_DEPLOY_INFO;
    gui_show_full_info( gui->finfo, unit );
    group_set_active( gui->deploy_window, ID_DEPLOY_UP, 0 );
    group_set_active( gui->deploy_window, ID_DEPLOY_DOWN, 0 );
    group_set_active( gui->deploy_window, ID_APPLY_DEPLOY, 0 );
    group_set_active( gui->deploy_window, ID_CANCEL_DEPLOY, 0 );
}
static void engine_hide_deploy_unit_info()
{
    status = STATUS_DEPLOY;
    frame_hide( gui->finfo, 1 );
    old_mx = old_my = -1;
    group_set_active( gui->deploy_window, ID_DEPLOY_UP, 1 );
    group_set_active( gui->deploy_window, ID_DEPLOY_DOWN, 1 );
    group_set_active( gui->deploy_window, ID_APPLY_DEPLOY, 1 );
    group_set_active( gui->deploy_window, ID_CANCEL_DEPLOY, !deploy_turn );
}

/*
====================================================================
Показать / скрыть игровое меню.
====================================================================
*/
static void engine_show_game_menu( int cx, int cy )
{
    char str[MAX_NAME];
    if ( setup.type == SETUP_RUN_TITLE ) {
        status = STATUS_TITLE_MENU;
        if ( cy + gui->main_menu->frame->img->img->h >= sdl.screen->h )
            cy = sdl.screen->h - gui->main_menu->frame->img->img->h;
        group_move( gui->main_menu, cx, cy );
        group_hide( gui->main_menu, 0 );
        group_set_active( gui->main_menu, ID_SAVE, 0 );
        group_set_active( gui->main_menu, ID_RESTART, 0 );
        snprintf( str, MAX_NAME, tr("Mod: %s"), config.mod_name );
        label_write( gui->label_left, gui->font_std, str );
        label_write( gui->label_center, gui->font_std, "" );
        label_write( gui->label_right, gui->font_std, "" );
    }
    else {
        engine_check_menu_buttons();
        gui_show_menu( cx, cy );
        status = STATUS_GAME_MENU;
        group_set_active( gui->main_menu, ID_SAVE, 1 );
        group_set_active( gui->main_menu, ID_RESTART, 1 );
    }
    /* заблокировать кнопки конфигурации */
    group_lock_button( gui->opt_menu, ID_C_SUPPLY, config.supply );
    group_lock_button( gui->opt_menu, ID_C_WEATHER, config.weather );
    group_lock_button( gui->opt_menu, ID_C_GRID, config.grid );
    group_lock_button( gui->opt_menu, ID_C_SHOW_CPU, config.show_cpu_turn );
    group_lock_button( gui->opt_menu, ID_C_SHOW_STRENGTH, config.show_bar );
    group_lock_button( gui->opt_menu, ID_C_SOUND, config.sound_on );
}
static void engine_hide_game_menu()
{
    if ( setup.type == SETUP_RUN_TITLE ) {
        status = STATUS_TITLE;
    }
    else
        engine_set_status( STATUS_NONE );
    group_hide( gui->base_menu, 1 );
    group_hide( gui->main_menu, 1 );
    fdlg_hide( gui->save_menu, 1 );
    fdlg_hide( gui->load_menu, 1 );
    group_hide( gui->opt_menu, 1 );
    old_mx = old_my = -1;
}

/*
====================================================================
Показать / скрыть меню юнита.
====================================================================
*/
static void engine_show_unit_menu( int cx, int cy )
{
    engine_check_unit_buttons();
    gui_show_unit_buttons( cx, cy );
    status = STATUS_UNIT_MENU;
}
static void engine_hide_unit_menu()
{
    engine_set_status( STATUS_NONE );
    group_hide( gui->unit_buttons, 1 );
    group_hide( gui->split_menu, 1 );
    old_mx = old_my = -1;
}

/*
====================================================================
Откройте окно подтверждения. (если это возвращает ID_CANCEL
последнее сохраненное действие будет удалено)
====================================================================
*/
static void engine_confirm_action( const char *text )
{
    top_committed_action = 0;
    gui_show_confirm( text );
    status = STATUS_CONF;
}

/*
====================================================================
Откройте окно подтверждения. Если это возвращает ID_CANCEL
все действия, кроме last_persistent_action, являются
удалено. Если last_persistent_action не существует, все
действия удалены.
====================================================================
*/
static void engine_confirm_actions( Action *last_persistent_action, const char *text )
{
    top_committed_action = last_persistent_action;
    gui_show_confirm( text );
    status = STATUS_CONF;
}

/*
====================================================================
Отобразить окончательное сообщение сценария (вызывается, когда scene_check_result ()
возвращает True).
====================================================================
*/
static void engine_show_final_message()
{
    event_wait_until_no_input();
    SDL_FillRect( sdl.screen, 0, 0x0 );
    gui->font_turn_info->align = ALIGN_X_CENTER | ALIGN_Y_CENTER;
    write_text( gui->font_turn_info, sdl.screen, sdl.screen->w / 2, sdl.screen->h / 2, scen_get_result_message(), 255 );
    refresh_screen( 0, 0, 0, 0 );
    while ( !engine_status_screen_dismissed() ) {
        SDL_PumpEvents();
        SDL_Delay( 20 );
    }
    event_clear();
}

/*
====================================================================
Показать информацию о ходе (сделано перед ходом)
====================================================================
*/
static void engine_show_turn_info()
{
    int text_x, text_y;
    int time_factor;
    char text[400];
    FULL_DEST( sdl.screen );    //рисовать регион
    fill_surf( 0x0 );   //заполнить поверхность цветом
#if 0
#  define i time_factor
    gui->font_std->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    text_x = text_y = 0;
    write_line( sdl.screen, gui->font_std, "Charset Test (latin1): Flöße über Wasser. \241Señálalo!", text_x, &text_y );
    text[32] = 0;
    for (i = 0; i < 256; i++) {
        text[i % 32] = i ? i : 127;
        if ((i + 1) % 32 == 0)
            write_line( sdl.screen, gui->font_std, text, text_x, &text_y );
    }
#  undef i
#endif
    text_x = sdl.screen->w >> 1;
    text_y = ( sdl.screen->h - 4 * gui->font_turn_info->height ) >> 1;
    gui->font_turn_info->align = ALIGN_X_CENTER | ALIGN_Y_TOP;
    scen_get_date( text );
    write_text( gui->font_turn_info, sdl.screen, text_x, text_y, text, OPAQUE );
    text_y += gui->font_turn_info->height;
    sprintf( text, tr("Next Player: %s"), cur_player->name );
    write_text( gui->font_turn_info, sdl.screen, text_x, text_y, text, OPAQUE );
    text_y += gui->font_turn_info->height;

    if ( deploy_turn ) {
        if ( cur_player->ctrl == PLAYER_CTRL_HUMAN ) {
            text_y += gui->font_turn_info->height;
            write_text( gui->font_turn_info, sdl.screen, text_x, text_y, tr("Deploy your troops"), OPAQUE );
            text_y += gui->font_turn_info->height;
            refresh_screen( 0, 0, 0, 0 );
            SDL_PumpEvents(); event_clear();
            while ( !engine_status_screen_dismissed() )
                SDL_PumpEvents(); SDL_Delay( 20 );
            event_clear();
        }
        /* не показывать экран для игроков с компьютерным управлением */
        return;
    }

    if ( turn + 1 < scen_info->turn_limit ) {
        sprintf( text, tr("Remaining Turns: %i"), scen_info->turn_limit - turn );
        write_text( gui->font_turn_info, sdl.screen, text_x, text_y, text, OPAQUE );
        text_y += gui->font_turn_info->height;
    }
    sprintf( text, tr("Weather: %s (%s)"), weather_types[scen_get_weather()].name,
                                           weather_types[scen_get_weather()].ground_conditions );
    write_text( gui->font_turn_info, sdl.screen, text_x, text_y, text, OPAQUE );
    text_y += gui->font_turn_info->height;
    if ( turn + 1 < scen_info->turn_limit )
        sprintf( text, tr("Turn: %d"), turn + 1 );
    else
        sprintf( text, tr("Last Turn") );
    write_text( gui->font_turn_info, sdl.screen, text_x, text_y, text, OPAQUE );
    refresh_screen( 0, 0, 0, 0 );
    SDL_PumpEvents(); event_clear();
    time_factor = 3000/20;		/* ждать 3 sec */
    while ( !engine_status_screen_dismissed() && time_factor ) {
        SDL_PumpEvents(); SDL_Delay( 20 );
	if (cur_ctrl != PLAYER_CTRL_HUMAN) time_factor--;
    }
    event_clear();
}

/*
====================================================================
Данные резервной копии, которые будут восстановлены, когда перемещение объекта было отменено.
(флаг пункта назначения, маска пятна, позиция объекта)
Если x! = -1, флаг в x, y будет сохранен.
====================================================================
*/
static void engine_backup_move( Unit *unit, int x, int y )
{
    if ( move_backup.used == 0 ) {
        move_backup.used = 1;
        memcpy( &move_backup.unit, unit, sizeof( Unit ) );
        map_backup_spot_mask();
    }
    if ( x != -1 ) {
        move_backup.dest_nation = map[x][y].nation;
        move_backup.dest_player = map[x][y].player;
        move_backup.flag_saved = 1;
    }
    else
        move_backup.flag_saved = 0;
}
static void engine_undo_move( Unit *unit )
{
    int new_embark;
    if ( !move_backup.used ) return;
    map_remove_unit( unit );
    if ( move_backup.flag_saved ) {
        map[unit->x][unit->y].player = move_backup.dest_player;
        map[unit->x][unit->y].nation = move_backup.dest_nation;
        move_backup.flag_saved = 0;
    }
    /* получить материал перед восстановлением указателя */
    new_embark = unit->embark;
    /* восстановить */
    memcpy( unit, &move_backup.unit, sizeof( Unit ) );
    /* проверить счетчики высадки / высадки */
    if ( unit->embark == EMBARK_NONE ) {
        if ( new_embark == EMBARK_AIR )
            unit->player->air_trsp_used--;
        if ( new_embark == EMBARK_SEA )
            unit->player->sea_trsp_used--;
    }
    else
        if ( unit->embark == EMBARK_SEA && new_embark == EMBARK_NONE )
            unit->player->sea_trsp_used++;
        else
            if ( unit->embark == EMBARK_AIR && new_embark == EMBARK_NONE )
                unit->player->air_trsp_used++;
    unit_adjust_icon( unit ); /* отрегулируйте изображение, так как направление могло измениться */
    map_insert_unit( unit );
    map_restore_spot_mask();
    if ( modify_fog ) map_set_fog( F_SPOT );
    move_backup.used = 0;
}
static void engine_clear_backup()
{
    move_backup.used = 0;
    move_backup.flag_saved = 0;
}

/*
====================================================================
Удалите отряд с карты и из списка отрядов и очистите его влияние.
====================================================================
*/
static void engine_remove_unit( Unit *unit )
{
    if (unit->killed >= 3) return;

    /* проверьте, не является ли он врагом текущего игрока; если так, то влияние должно быть устранено */
    if ( !player_is_ally( cur_player, unit->player ) )
        map_remove_unit_infl( unit );
    map_remove_unit( unit );
    /* из списка модулей (если не основной модуль) */
    if (!unit->core)
        unit->killed = 3;
    else
        unit->killed = 2;
}

/*
====================================================================
Вернуть текущие единицы в avail_units в список повторных ссылок. Получите все в силе
подкрепления для текущего игрока от reinf и поместите их в
avail_units. Самолеты на первом месте.
====================================================================
*/
static void engine_update_avail_reinf_list()
{
    Unit *unit;
    int i;
    /* доступное подкрепление */
    if ( avail_units )
    {
        list_reset( avail_units );
        while ( avail_units->count > 0 )
            list_transfer( avail_units, reinf, list_first( avail_units ) );
        /* добавить все юниты из сценария :: reinf, задержка которого <= cur_turn */
        list_reset( reinf );
        for ( i = 0; i < reinf->count; i++ ) {
            unit = list_next( reinf );
            if ( unit_has_flag( unit->sel_prop, "flying" ) && unit->player == cur_player && unit->delay <= turn ) {
                list_transfer( reinf, avail_units, unit );
                /* индекс должен быть сброшен, если юнит был добавлен */
                i--;
            }
        }
    }
    if ( reinf )
    {
        list_reset( reinf );
        for ( i = 0; i < reinf->count; i++ ) {
            unit = list_next( reinf );
            if ( !unit_has_flag( unit->sel_prop, "flying" ) && unit->player == cur_player && unit->delay <= turn ) {
                list_transfer( reinf, avail_units, unit );
                /* индекс должен быть сброшен, если юнит был добавлен */
                i--;
            }
        }
    }
}

/*
====================================================================
Инициируйте игрока как текущего игрока и подготовьте свой ход.
Если 'skip_unit_prep' установлен, сцена_prep_unit () не вызывается.
====================================================================
*/
static void engine_select_player( Player *player, int skip_unit_prep )
{
    Player *human;
    int i, human_count, x, y;
    Unit *unit;

    cur_player = player;
    if ( player )
        cur_ctrl = player->ctrl;
    else
        cur_ctrl = PLAYER_CTRL_NOBODY;
    if ( !skip_unit_prep ) {
        /* обновить доступное подкрепление */
        engine_update_avail_reinf_list();   	//отсортировать юниты в avail_units
        /* подготовить юниты к развороту - топливо, точки перемещения, вход, погода и т. д. */
        if (units)
        {
            list_reset( units );
            for ( i = 0; i < units->count; i++ ) {
                unit = list_next( units );
                if ( unit->player == cur_player ) {
                    if ( turn == 0 )
                        scen_prep_unit( unit, SCEN_PREP_UNIT_FIRST );
                    else
                        scen_prep_unit( unit, SCEN_PREP_UNIT_NORMAL );
                }
            }
        }
    }
    /* установить туман */
    switch ( cur_ctrl ) {
        case PLAYER_CTRL_HUMAN:
            modify_fog = 1;
            map_set_spot_mask();
            map_set_fog( F_SPOT );
            break;
        case PLAYER_CTRL_NOBODY:
            for ( x = 0; x < map_w; x++ )
                for ( y = 0; y < map_h; y++ )
                    mask[x][y].spot = 1;
            map_set_fog( 0 );
            break;
        case PLAYER_CTRL_CPU:
            human = engine_human_player( &human_count );
            if ( human_count == 1 ) {
                modify_fog = 0;
                map_set_spot_mask();
                map_set_fog_by_player( human );
            }
            else {
                modify_fog = 1;
                map_set_spot_mask();
                map_set_fog( F_SPOT );
            }
            break;
    }
    /* обратный отсчет задержки развертывания центра (1==снова развернуть центр) */
    if ( !skip_unit_prep )
	    for (x=0;x<map_w;x++)
		for (y=0;y<map_h;y++)
		    if (map[x][y].deploy_center > 1 && map[x][y].player ==
								cur_player)
			map[x][y].deploy_center--;
    /* установить маску влияния */
    if ( cur_ctrl != PLAYER_CTRL_NOBODY )
        map_set_infl_mask();
    map_get_vis_units();
    if ( !skip_unit_prep) {

        /* диалоговое окно подготовка развертывания при развертывании-хода */
        if ( deploy_turn
             && cur_player && cur_player->ctrl == PLAYER_CTRL_HUMAN )
            engine_handle_button( ID_DEPLOY );	//обработайте кнопку, которая была нажата
        else
            group_hide( gui->deploy_window, 1 );

        /* уровни поставок */
        if ( units )
        {
            list_reset( units );
            while ( ( unit = list_next( units ) ) )
                if ( unit->player == cur_player )
                    scen_adjust_unit_supply_level( unit );
        }
        /* пометьте юнит как повторно развертываемую в ходе развертывания, когда она
* находится на поле развертывания и ей разрешено повторно развертываться.
         */
        if (deploy_turn)
        {
            map_get_deploy_mask(cur_player,0,1);
            list_reset( units );
            while ( ( unit = list_next( units ) ) ) {
                if ( unit->player == cur_player )
                if ( mask[unit->x][unit->y].deploy )
                if ( unit_supports_deploy(unit) ) {
                    unit->fresh_deploy = 1;
                    list_transfer( units, avail_units, unit );
                }
            }
        }
    }
    /* очистить выбор / действия */
    cur_unit = cur_target = cur_atk = cur_def = surp_unit = move_unit = deploy_unit = 0;
    merge_unit_count = 0;
    list_clear( df_units );
    actions_clear();
    scroll_block = 0;
}

/*
====================================================================
Начинайте ход следующего игрока. Поэтому выберите следующий игрок или используйте
'forced_player', если не NULL (тогда следующий-это тот, который стоит после
'forced_player').
Если задано значение 'skip_unit_prep', функция scen_prep_unit () не вызывается.
====================================================================
*/
static void engine_begin_turn( Player *forced_player, int skip_unit_prep )
{
    char text[400];
    int new_turn = 0;
    Player *player = 0;
    /* очистите различные вещи, которые все еще могут быть установлены с последнего хода */
    group_set_active( gui->confirm, ID_OK, 1 );
    engine_hide_unit_menu();    //скрыть меню юнита
    engine_hide_game_menu();    //скрыть игровое меню
    /* очистить отменить */
    engine_clear_backup();
    /* понятно, отвратительных кликов */
    if ( !deploy_turn && cur_ctrl == PLAYER_CTRL_HUMAN ) event_wait_until_no_input();   //если нет очереди развертывания и управляет игрок подождите, пока не будет нажата ни клавиша, ни кнопка
    /* получить игрока */
    if ( forced_player == 0 ) {
        /* следующий игрок и ход */
        player = players_get_next( &new_turn );
        if ( new_turn )  {  //если новый ход увеличить счетчик ходов
            turn++;
        }
        if ( turn == scen_info->turn_limit ) {  //если достигнут лимит ходов в сценарии
            /* использовать другой состоянии, в результате сценарий  */
            /* и взгляните в последний раз */
            if (camp_loaded == FIRST_SCENARIO)
                camp_loaded = CONTINUE_CAMPAIGN;
            scen_check_result( 1 );
            blind_cpu_turn = 0;
            engine_show_final_message();
            draw_map = 1;
            image_hide( gui->cursors, 0 );
            gui_set_cursor( CURSOR_STD );
            engine_select_player( 0, skip_unit_prep );
            engine_set_status( STATUS_NONE );
            phase = PHASE_NONE;
            if (camp_loaded == CONTINUE_CAMPAIGN)
                scen_save_core_units(); /* храните основные блоки для следующего сценария */
            return;
        }
        else {
            cur_weather = scen_get_weather();
            engine_select_player( player, skip_unit_prep ); //подготовьте ход игрока
        }
    }
    else {
        engine_select_player( forced_player, skip_unit_prep ); //подготовьте ход игрока
        players_set_current( player_get_index( forced_player ) );   //установить и получить игрока как текущий по индексу
    }

    /* Добавить престиж для регулярного начала хода (skip_unit_prep означает, что игра была
     * загружено). */
    if (!deploy_turn && !skip_unit_prep)    //если нет очереди развертывания и нет пропуска игрока
        cur_player->cur_prestige += cur_player->prestige_per_turn[turn];    //добавить престиж перед началом хода

    if ( scen_check_result(0) ) {   //проверьте, полностью ли выполнены условия победы
        engine_finish_scenario();   //завершите сценарий и отобразите последнее сообщение
        end_scen = 1;               //конец сценария
        return;
    }

    /* инициализация хода ИИ, если таковой имеется */
    if ( cur_player && cur_player->ctrl == PLAYER_CTRL_CPU )
        (cur_player->ai_init)();    //инициировать ход ИИ
    /* информация о ходе */
    engine_show_turn_info();    //показать информацию о ходе
    engine_set_status( deploy_turn ? STATUS_DEPLOY : STATUS_NONE ); //установить желаемый статус
    phase = PHASE_NONE; //конец фазы
    /* экран обновления */
    if ( cur_ctrl != PLAYER_CTRL_CPU || config.show_cpu_turn ) {    //если не ИИ или ...
        if ( cur_ctrl == PLAYER_CTRL_CPU )
            engine_update_info( 0, 0, 0 );  //обновите краткую информацию об объекте и информацию о фрагменте карты
        else {
            image_hide( gui->cursors, 0 );
        }
        engine_draw_map();     //обновить полную карту
        refresh_screen( 0, 0, 0, 0 );   //обновить прямоугольник (0,0,0,0) -> полноэкранный режим
        blind_cpu_turn = 0;    //если это правда, все движения скрыты
    }
    else {
        engine_update_info( 0, 0, 0 );  //обновите краткую информацию об объекте и информацию о фрагменте карты
        draw_map = 0;   //если этот флаг истинен, engine_update () вызывает engine_draw_map ()
        FULL_DEST( sdl.screen );    //рисовать регион
        fill_surf( 0x0 );   //заполнить поверхность цветом
        gui->font_turn_info->align = ALIGN_X_CENTER | ALIGN_Y_CENTER;
        sprintf( text, tr("CPU thinks...") );   //центральный процессор думает
        write_text( gui->font_turn_info, sdl.screen, sdl.screen->w >> 1, sdl.screen->h >> 1, text, OPAQUE );
        sprintf( text, tr("( Enable option 'Show Cpu Turn' if you want to see what it is doing. )") );
        write_text( gui->font_turn_info, sdl.screen, sdl.screen->w >> 1, ( sdl.screen->h >> 1 )+ 20, text, OPAQUE );
        refresh_screen( 0, 0, 0, 0 );   //обновить прямоугольник (0,0,0,0) -> полноэкранный режим
        blind_cpu_turn = 1;    //если это правда, все движения скрыты
    }
}

/*
====================================================================
Завершите ход текущего игрока без выбора следующего игрока. Здесь
происходит автосохранение, самолеты терпят крушение, юниты снабжаются.
====================================================================
*/
static void engine_end_turn()
{
    int i;
    char autosaveName[25];
    Unit *unit;
    /* завершите ход ИИ, если таковой имеется */
    if ( cur_player && cur_player->ctrl == PLAYER_CTRL_CPU )
        (cur_player->ai_finalize)();
    /* если turn == scen_info-> turn_limit это был последний ход */
    if ( turn == scen_info->turn_limit ) {
        end_scen = 1;
        return;
    }
    /* автосохранение игры для человека */
    if (!deploy_turn && cur_player && cur_player->ctrl == PLAYER_CTRL_HUMAN)
    {
        snprintf( autosaveName, 25, "Autosave");// (%s), %s, %s",  );
        slot_save( autosaveName, "" );
    }
    /* заправляйте и уничтожайте разбившиеся самолеты*/
    list_reset( units );
    while ( (unit=list_next(units)) )
    {
        if ( unit->player != cur_player ) continue;
        /* поставлять неиспользуемые наземные подразделения так же, как если бы это делалось вручную, и
           любой самолет с обновленным уровнем снабжения */
        if (config.supply && unit_check_fuel_usage( unit ))
        {
            if ( unit_has_flag( &unit->prop, "flying" ) )
            {
                /* освободите половину топлива даже если не двигаетесь */
                if (unit->cur_mov>0) /* FIXME: это пойдет не так, если юниты могут двигаться несколько раз */
                {
                    unit->cur_fuel -= unit_calc_fuel_usage(unit,0);
                    if (unit->cur_fuel<0) unit->cur_fuel = 0;
                }
                /* поставка? */
                scen_adjust_unit_supply_level( unit );
                if ( unit->supply_level > 0 )
                {
                    unit->unused = 1; /* требуется для заправки */
                    unit_supply( unit, UNIT_SUPPLY_ALL );
                }
                /* авария, если нет топлива */
                if (unit->cur_fuel == 0)
                    unit->killed = 1;
            }
            else if ( unit->unused && unit->supply_level > 0 )
                unit_supply( unit, UNIT_SUPPLY_ALL );
        }
    }
    /* удалить все юниты, которые были убиты в последний ход */
    list_reset( units );
    for ( i = 0; i < units->count; i++ )
    {
        unit = list_next(units);
        if ( unit->killed ) {
            engine_remove_unit( unit );
            if (unit->killed != 2)
            {
                list_delete_item( units, unit );
                i--; /* настроить индекс */
            }
        }
    }
}

/*
====================================================================
Получить положение карты / экрана из положения курсора / карты.
====================================================================
*/
static int engine_get_screen_pos( int mx, int my, int *sx, int *sy )
{
    int x = map_sx, y = map_sy;
    /* это начальная позиция, если x-pos первой плитки на экране не является нечетной */
    /* если он нечетный, мы должны добавить y_offset в исходную позицию */
    if ( ODD( map_x ) )
        y += hex_y_offset;
    /* уменьшить до видимых плиток карты */
    mx -= map_x;
    my -= map_y;
    /* проверить диапазон */
    if ( mx < 0 || my < 0) return 0;
    /* вычислить позицию */
    x += mx * hex_x_offset;
    y += my * hex_h;
    /* если x_pos первой плитки четное, мы должны добавить y_offset к нечетным плиткам на экране */
    if ( EVEN( map_x ) ) {
        if ( ODD( mx ) )
            y += hex_y_offset;
    }
    else {
        /* мы должны вычесть y_offset из четных плиток */
        if ( ODD( mx ) )
            y -= hex_y_offset;
    }
    /* проверить диапазон */
    if ( x >= gui->map_frame->w || y >= gui->map_frame->h ) return 0;
    /* назначать */
    *sx = x;
    *sy = y + 30;
    return 1;
}
enum {
    REGION_GROUND = 0,
    REGION_AIR
};
static int engine_get_map_pos( int sx, int sy, int *mx, int *my, int *region )
{
    int x = 0, y = 0;
    int screen_x, screen_y;
    int tile_x, tile_y;
    int total_y_offset;
    if ( status == STATUS_STRAT_MAP ) {
        /* стратегическая карта */
        if ( strat_map_get_pos( sx, sy, mx, my ) ) {
            if ( *mx < 0 || *my < 0 || *mx >= map_w || *my >= map_h )
                return 0;
            return 1;
        }
        return 0;
    }
    /* получить смещение карты на экране из положения мыши */
    x = ( sx - map_sx ) / hex_x_offset;
    /* Значение y вычисляется так же, как значение x, но может быть смещением engine :: y_offset */
    total_y_offset = 0;
    if ( EVEN( map_x ) && ODD( x ) )
        total_y_offset = hex_y_offset;
    /* если engine :: map_x нечетный, должно быть смещение engine :: y_offset для начала первого тайла */
    /* и все нечетные плитки получают смещение -engine :: y_offset, поэтому результат:
        odd: offset = 0 even: offset = engine :: y_offset лучше всего это нарисовать ;-D
    */
    if ( ODD( map_x ) && EVEN( x ) )
        total_y_offset = hex_y_offset;
    y = ( sy - 30 - total_y_offset - map_sy ) / hex_h;
    /* вычислить положение экрана */
    if ( !engine_get_screen_pos( x + map_x, y + map_y, &screen_x, &screen_y ) ) return 0;
    /* тестовая маска с  sx - screen_x, sy - screen_y */
    tile_x = sx - screen_x;
    tile_y = sy - screen_y;
    if ( !hex_mask[tile_y * hex_w + tile_x] ) {
        if ( EVEN( map_x ) ) {
            if ( tile_y < hex_y_offset && EVEN( x ) ) y--;
            if ( tile_y >= hex_y_offset && ODD( x ) ) y++;
        }
        else {
            if ( tile_y < hex_y_offset && ODD( x ) ) y--;
            if ( tile_y >= hex_y_offset && EVEN( x ) ) y++;
        }
        x--;
    }
    /* область */
    if ( tile_y < ( hex_h >> 1 ) )
        *region = REGION_AIR;
    else
        *region = REGION_GROUND;
    /* добавить смещение карты двигателя и назначить */
    x += map_x;
    y += map_y;
    *mx = x;
    *my = y;
    /* check range */
    if ( x < 0 || y < 0 || x >= map_w || y >= map_h ) return 0;
    /* хорошо, плитка существует */
    return 1;
}

/*
====================================================================
Переместитесь в это положение и установите «draw_map», если оно действительно перемещено.
====================================================================
*/
static void engine_goto_xy( int x, int y )
{
    int x_diff, y_diff;
    /* проверить диапазон */
    if ( x < 0 ) x = 0;
    if ( y < 0 ) y = 0;
    /* если отображается больше плиток, значит карта имеет (оставшееся черное пространство), изменение положения не допускается */
    if ( map_sw >= map_w ) x = 0;
    else
        if ( x > map_w - map_sw )
            x = map_w - map_sw;
    if ( map_sh >= map_h ) y = 0;
    else
        if ( y > map_h - map_sh )
            y = map_h - map_sh;
    /* проверьте, возможна ли копия экрана */
    x_diff = x - map_x;
    y_diff = y - map_y;
    /* if one diff is ==0 and one diff !=0 do it! */
    if ( x_diff == 0 && y_diff != 0 ) {
        sc_type = SC_VERT;
        sc_diff = y_diff;
    }
    else
        if ( x_diff != 0 && y_diff == 0 ) {
            sc_type = SC_HORI;
            sc_diff = x_diff;
        }
    /* на самом деле движется? */
    if ( x != map_x || y != map_y ) {
        map_x = x; map_y = y;
        draw_map = 1;
    }
}

/*
====================================================================
Проверьте, находится ли позиция мыши в области прокрутки или нажата клавиша
и возможна прокрутка.
Если 'by_wheel' истинно, scroll_hori / vert было с помощью
колесо мыши и проверку клавиш / мыши нужно пропустить.
====================================================================
*/
enum {
    SC_NORMAL = 0,
    SC_BY_WHEEL
};
static void engine_check_scroll(int by_wheel)
{
    int region;
    int tol = 3; /* граница, в которой прокрутка мышью */
    int mx, my, cx, cy;
    if ( scroll_block ) return;
    if ( setup.type == SETUP_RUN_TITLE ) return;
    if( !by_wheel ) {
        /* ключи */
        scroll_hori = scroll_vert = SCROLL_NONE;
        if ( !scroll_block_keys ) {
            if ( event_check_key( SDLK_UP ) && map_y > 0)
                scroll_vert = SCROLL_UP;
            else
                if ( event_check_key( SDLK_DOWN ) && map_y < map_h - map_sh )
                    scroll_vert = SCROLL_DOWN;
            if ( event_check_key( SDLK_LEFT ) && map_x > 0 )
                scroll_hori = SCROLL_LEFT;
            else
                if ( event_check_key( SDLK_RIGHT ) && map_x < map_w - map_sw )
                    scroll_hori = SCROLL_RIGHT;
        }
        if ( scroll_vert == SCROLL_NONE && scroll_hori == SCROLL_NONE ) {
            /* мышь */
            event_get_cursor_pos( &mx, &my );
            if ( my <= tol && map_y > 0 )
                scroll_vert = SCROLL_UP;
            else
                if ( mx >= sdl.screen->w - tol - 1 && map_x < map_w - map_sw )
                    scroll_hori = SCROLL_RIGHT;
                else
                    if ( my >= sdl.screen->h - tol - 1 && map_y < map_h - map_sh )
                        scroll_vert = SCROLL_DOWN;
                    else
                        if ( mx <= tol && map_x > 0 )
                            scroll_hori = SCROLL_LEFT;
        }
    }
    /* прокрутка */
    if ( !( scroll_vert == SCROLL_NONE && scroll_hori == SCROLL_NONE ) &&
          ( gui->message_dlg->main_group->frame->img->bkgnd->hide &&
            gui->save_menu->group->frame->img->bkgnd->hide && gui->load_menu->group->frame->img->bkgnd->hide ) )
    {
        if ( scroll_vert == SCROLL_UP )
            engine_goto_xy( map_x, map_y - 2 );
        else
            if ( scroll_hori == SCROLL_RIGHT )
                engine_goto_xy( map_x + 2, map_y );
            else
                if ( scroll_vert == SCROLL_DOWN )
                    engine_goto_xy( map_x, map_y + 2 );
                else
                    if ( scroll_hori == SCROLL_LEFT )
                        engine_goto_xy( map_x - 2, map_y );
        event_get_cursor_pos( &cx, &cy );
        if(engine_get_map_pos( cx, cy, &mx, &my, &region ))
            engine_update_info( mx, my, region );
        if ( !by_wheel )
            scroll_block = 1;
    }
}

/*
====================================================================
Обновить полную карту.
====================================================================
*/
static void engine_draw_map()
{
    int x, y, abs_y;
    int i, j;
    int start_map_x, start_map_y, end_map_x, end_map_y;
    int buffer_height, buffer_width, buffer_offset;
    int use_frame = ( cur_ctrl != PLAYER_CTRL_CPU );
    enum Stage { DrawTerrain, DrawUnits, DrawDangerZone } stage = DrawTerrain;
    enum Stage top_stage = has_danger_zone ? DrawDangerZone : DrawUnits;

    /* reset_timer(); */

    draw_map = 0;

    if ( status == STATUS_STRAT_MAP ) {
        sc_type = SC_NONE;
        strat_map_draw();
        return;
    }

    if ( status == STATUS_TITLE || status == STATUS_TITLE_MENU ) {
        sc_type = SC_NONE;
        engine_draw_bkgnd();
        return;
    }

    /* копия экрана? */
    start_map_x = map_x;
    start_map_y = map_y;
    end_map_x = map_x + map_sw;
    end_map_y = map_y + map_sh;
    if ( sc_type == SC_VERT ) {
        /* снять флаг */
        sc_type = SC_NONE;
        /* установить смещение и высоту буфера */
        buffer_offset = abs( sc_diff ) * hex_h;
        buffer_height = gui->map_frame->h - buffer_offset;
        /* спускаться */
        if ( sc_diff > 0 ) {
            /* скопировать экран в буфер */
            DEST( sc_buffer, 0, 0, gui->map_frame->w, buffer_height );
            SOURCE( gui->map_frame, 0, buffer_offset );
            blit_surf();
            /* скопировать буфер на новую позицию */
            DEST( gui->map_frame, 0, 0, gui->map_frame->w, buffer_height );
            SOURCE( sc_buffer, 0, 0 );
            blit_surf();
            /* установить диапазон цикла для перерисовки нижних линий */
            start_map_y += map_sh - sc_diff - 2;
        }
        /* подниматься */
        else {
            /* скопировать экран в буфер */
            DEST( sc_buffer, 0, 0, gui->map_frame->w, buffer_height );
            SOURCE( gui->map_frame, 0, 0 );
            blit_surf();
            /* скопировать буфер на новую позицию */
            DEST( gui->map_frame, 0, buffer_offset, gui->map_frame->w, buffer_height );
            SOURCE( sc_buffer, 0, 0 );
            blit_surf();
            /* установить диапазон цикла для перерисовки верхних строк */
            end_map_y = map_y + abs( sc_diff ) + 1;
        }
    }
    else
        if ( sc_type == SC_HORI ) {
            /* снять флаг */
            sc_type = SC_NONE;
            /* установить смещение и ширину буфера */
            buffer_offset = abs( sc_diff ) * hex_x_offset;
            buffer_width = gui->map_frame->w - buffer_offset;
            buffer_height = gui->map_frame->h;
            /* идет правильно */
            if ( sc_diff > 0 ) {
                /* скопировать экран в буфер */
                DEST( sc_buffer, 0, 0, buffer_width, buffer_height );
                SOURCE( gui->map_frame, buffer_offset, 0 );
                blit_surf();
                /* скопировать буфер на новую позицию */
                DEST( gui->map_frame, 0, 0, buffer_width, buffer_height );
                SOURCE( sc_buffer, 0, 0 );
                blit_surf();
                /* установить диапазон цикла для перерисовки правильных линий */
                start_map_x += map_sw - sc_diff - 2;
            }
            /* иду налево */
            else {
                /* скопировать экран в буфер */
                DEST( sc_buffer, 0, 0, buffer_width, buffer_height );
                SOURCE( gui->map_frame, 0, 0 );
                blit_surf();
                /* скопировать буфер на новую позицию */
                DEST( gui->map_frame, buffer_offset, 0, buffer_width, buffer_height );
                SOURCE( sc_buffer, 0, 0 );
                blit_surf();
                /* установить диапазон цикла для перерисовки правильных линий */
                end_map_x = map_x + abs( sc_diff ) + 1;
            }
        }
    for (; stage <= top_stage; stage++) {
        /* начальная позиция для рисования */
        x = map_sx + ( start_map_x - map_x ) * hex_x_offset;
        y = map_sy + ( start_map_y - map_y ) * hex_h;
        /* end_map_xy не должен превышать размер карты */
        if ( end_map_x >= map_w ) end_map_x = map_w;
        if ( end_map_y >= map_h ) end_map_y = map_h;
        /* цикл для рисования фрагмента карты */
        for ( j = start_map_y; j < end_map_y; j++ ) {
            for ( i = start_map_x; i < end_map_x; i++ ) {
                /* обновить каждый фрагмент карты */
                if ( i & 1 )
                    abs_y = y + hex_y_offset;
                else
                    abs_y = y;
                switch (stage) {
                    case DrawTerrain:
                        map_draw_terrain( gui->map_frame, i, j, x, abs_y );
                        break;
                    case DrawUnits:
                        if ( cur_unit && cur_unit->x == i && cur_unit->y == j && status != STATUS_MOVE && mask[i][j].spot )
                            map_draw_units( gui->map_frame, i, j, x, abs_y, !air_mode, use_frame );
                        else
                            map_draw_units( gui->map_frame, i, j, x, abs_y, !air_mode, 0 );
                        break;
                    case DrawDangerZone:
                        if ( mask[i][j].danger )
                            map_apply_danger_to_tile( gui->map_frame, i, j, x, abs_y );
                        break;
                }
                x += hex_x_offset;
            }
            y += hex_h;
            x = map_sx + ( start_map_x - map_x ) * hex_x_offset;
        }
    }
    /* printf( "time needed: %i ms\n", get_time() ); */
}

/*
====================================================================
Получите основной юнит на плитке.
====================================================================
*/
static Unit *engine_get_prim_unit( int x, int y, int region )
{
    if ( x < 0 || y < 0 || x >= map_w || y >= map_h ) return 0;
    if ( region == REGION_AIR ) {
        if ( map[x][y].a_unit )
            return map[x][y].a_unit;
        else
            return map[x][y].g_unit;
    }
    else {
        if ( map[x][y].g_unit )
            return map[x][y].g_unit;
        else
            return map[x][y].a_unit;
    }
}

/*
====================================================================
Проверьте, есть ли цель для текущего юнита на x, y.
====================================================================
*/
static Unit* engine_get_target( int x, int y, int region )
{
    Unit *unit;
    if ( x < 0 || y < 0 || x >= map_w || y >= map_h ) return 0;
    if ( !mask[x][y].spot ) return 0;
    if ( cur_unit == 0 ) return 0;
    if ( ( unit = engine_get_prim_unit( x, y, region ) ) )
        if ( unit_check_attack( cur_unit, unit, UNIT_ACTIVE_ATTACK ) )
            return unit;
    return 0;
/*    if ( region == REGION_AIR ) {
        if ( map[x][y].a_unit && unit_check_attack( cur_unit, map[x][y].a_unit, UNIT_ACTIVE_ATTACK ) )
            return map[x][y].a_unit;
        else
            if ( map[x][y].g_unit && unit_check_attack( cur_unit, map[x][y].g_unit, UNIT_ACTIVE_ATTACK ) )
                return map[x][y].g_unit;
            else
                return 0;
    }
    else {
        if ( map[x][y].g_unit && unit_check_attack( cur_unit, map[x][y].g_unit, UNIT_ACTIVE_ATTACK ) )
            return map[x][y].g_unit;
        else
            if ( map[x][y].a_unit && unit_check_attack( cur_unit, map[x][y].a_unit, UNIT_ACTIVE_ATTACK ) )
                return map[x][y].a_unit;
            else
                return 0;
    }*/
}

/*
====================================================================
Проверить, есть ли выбираемый юнит для текущего игрока на x, y
Текущий выбранный блок не считается доступным для выбора. (хотя
основной юнит на той же плитке может быть выбран, если он не
текущий юнит)
====================================================================
*/
static Unit* engine_get_select_unit( int x, int y, int region )
{
    if ( x < 0 || y < 0 || x >= map_w || y >= map_h ) return 0;
    if ( !mask[x][y].spot ) return 0;
    if ( region == REGION_AIR ) {
        if ( map[x][y].a_unit && map[x][y].a_unit->player == cur_player ) {
            if ( cur_unit == map[x][y].a_unit )
                return 0;
            else
                return map[x][y].a_unit;
        }
        else
            if ( map[x][y].g_unit && map[x][y].g_unit->player == cur_player )
                return map[x][y].g_unit;
            else
                return 0;
    }
    else {
        if ( map[x][y].g_unit && map[x][y].g_unit->player == cur_player ) {
            if ( cur_unit == map[x][y].g_unit )
                return 0;
            else
                return map[x][y].g_unit;
        }
        else
            if ( map[x][y].a_unit && map[x][y].a_unit->player == cur_player )
                return map[x][y].a_unit;
            else
                return 0;
    }
}

/*
====================================================================
Обновите краткую информацию об объекте и информацию о фрагменте карты, если фрагмент карты mx, my,
регион имеет фокус. Также обновите курсор.
====================================================================
*/
static void engine_update_info( int mx, int my, int region )
{
    Unit *unit1 = 0, *unit2 = 0, *unit;
    char str[MAX_LINE];
    int att_damage, def_damage;
    int moveCost = 0;
    /* нет информации, когда процессор работает */
    if ( cur_ctrl == PLAYER_CTRL_CPU ) {
        image_hide( gui->cursors, 1 );
        label_write( gui->label_left, gui->font_std, "" );
        label_write( gui->label_center, gui->font_status, "" );
        label_write( gui->label_right, gui->font_std, "" );
        frame_hide( gui->qinfo1, 1 );
        frame_hide( gui->qinfo2, 1 );
        return;
    }
	if ( cur_unit && cur_unit->cur_mov > 0 && mask[mx][my].in_range &&
         !mask[mx][my].blocked )
        moveCost = mask[mx][my].moveCost;
    /* ввел новую плитку, поэтому обновите информацию о местности */
    if (status == STATUS_PURCHASE) {
        snprintf( str, MAX_LINE, tr("Prestige: %d"), cur_player->cur_prestige );
        label_write( gui->label_left, gui->font_std, str );
    }
    else if (status==STATUS_DROP)
    {
        label_write( gui->label_left, gui->font_std,tr("Select Drop Zone")  );
    }
    else if (status==STATUS_SPLIT)
    {
        sprintf( str, tr("Split Up %d Strength"), cur_split_str );
        label_write( gui->label_left, gui->font_std, str );
    }
    else if (moveCost>0)
    {
        sprintf( str, GS_DISTANCE "%d", moveCost );
        label_write( gui->label_left, gui->font_status, str );
        sprintf( str, "%s (%i,%i) ", map[mx][my].name, mx, my );
        label_write( gui->label_center, gui->font_status, str );
        snprintf( str, MAX_NAME, tr("Weather: %s"), weather_types[cur_weather].name );
        label_write( gui->label_right, gui->font_std, str );
    }
    else
    {

        //snprintf( str, MAX_LINE, tr("Prestige: %d"), cur_player->cur_prestige );   /*FIXME Показать престиж вместо мода   */
        snprintf( str, MAX_NAME, tr("Mod: %s"), config.mod_name );
        label_write( gui->label_left, gui->font_std, str );
        sprintf( str, "%s (%i,%i)", map[mx][my].name, mx, my );
        label_write( gui->label_center, gui->font_status, str );
        snprintf( str, MAX_NAME, tr("Weather: %s"), weather_types[cur_weather].name );
        label_write( gui->label_right, gui->font_std, str );
    }
    /* DEBUG:
    sprintf( str, "(%d,%d), P: %d B: %d I: %d D: %d",mx,my,mask[mx][my].in_range-1,mask[mx][my].blocked,mask[mx][my].vis_infl,mask[mx][my].distance);
    label_write( gui->label, gui->font_std, str ); */
    /* update the unit info */
    if ( !mask[mx][my].spot ) {
        if ( cur_unit )
            gui_show_quick_info( gui->qinfo1, cur_unit );
        else
            frame_hide( gui->qinfo1, 1 );
        frame_hide( gui->qinfo2, 1 );
    }
    else {
        if ( cur_unit && ( mx != cur_unit->x || my != cur_unit->y ) ) {
            unit1 = cur_unit;
            unit2 = engine_get_prim_unit( mx, my, region );
        }
        else {
            if ( map[mx][my].a_unit && map[mx][my].g_unit ) {
                unit1 = map[mx][my].g_unit;
                unit2 = map[mx][my].a_unit;
            }
            else
                if ( map[mx][my].a_unit )
                    unit1 = map[mx][my].a_unit;
                else
                    if ( map[mx][my].g_unit )
                        unit1 = map[mx][my].g_unit;
        }
        if ( unit1 )
            gui_show_quick_info( gui->qinfo1, unit1 );
        else
            frame_hide( gui->qinfo1, 1 );
        if ( unit2 && status != STATUS_UNIT_MENU )
            gui_show_quick_info( gui->qinfo2, unit2 );
        else
            frame_hide( gui->qinfo2, 1 );
        /* показать ожидаемые потери? */
        if ( cur_unit && ( unit = engine_get_target( mx, my, region ) ) ) {
            unit_get_expected_losses( cur_unit, unit, &att_damage, &def_damage );
            gui_show_expected_losses( cur_unit, unit, att_damage, def_damage );
        }
/*        if ( unit1 && unit2 ) {
            if ( engine_get_target( mx, my, region ) )
            if ( unit1 == cur_unit && unit_check_attack( unit1, unit2, UNIT_ACTIVE_ATTACK ) ) {
                unit_get_expected_losses( unit1, unit2, map[unit2->x][unit2->y].terrain, &att_damage, &def_damage );
                gui_show_expected_losses( unit1, unit2, att_damage, def_damage );
            }
            else
            if ( unit2 == cur_unit && unit_check_attack( unit2, unit1, UNIT_ACTIVE_ATTACK ) ) {
                unit_get_expected_losses( unit2, unit1, map[unit1->x][unit1->y].terrain, &att_damage, &def_damage );
                gui_show_expected_losses( unit2, unit1, att_damage, def_damage );
            }
        }*/
    }
    if ( cur_player == 0 )
        gui_set_cursor( CURSOR_STD );
    else
    /* курсор */
    switch ( status ) {
        case STATUS_TITLE:
        case STATUS_TITLE_MENU:
        case STATUS_STRAT_MAP:
        case STATUS_GAME_MENU:
        case STATUS_UNIT_MENU:
            gui_set_cursor( CURSOR_STD );
            break;
        case STATUS_DROP:
            if ( mask[mx][my].deploy )
                gui_set_cursor( CURSOR_DEBARK );
            else
                gui_set_cursor( CURSOR_STD );
            break;
        case STATUS_MERGE:
            if ( mask[mx][my].merge_unit )
                gui_set_cursor( CURSOR_UNDEPLOY );
            else
                gui_set_cursor( CURSOR_STD );
            break;
        case STATUS_SPLIT:
            if (mask[mx][my].split_unit||mask[mx][my].split_okay)
                gui_set_cursor( CURSOR_DEPLOY );
            else
                gui_set_cursor( CURSOR_STD );
            break;
        case STATUS_DEPLOY:
            if (map_get_undeploy_unit(mx,my,region==REGION_AIR))
                gui_set_cursor( CURSOR_UNDEPLOY );
            else
                if (deploy_unit&&mask[mx][my].deploy)
                    gui_set_cursor( CURSOR_DEPLOY );
                else if (map_get_undeploy_unit(mx,my,region!=REGION_AIR))
                    gui_set_cursor( CURSOR_UNDEPLOY );
                else
                    gui_set_cursor( CURSOR_STD );
            break;
        default:
            if ( cur_unit ) {
                if ( cur_unit->x == mx && cur_unit->y == my && engine_get_prim_unit( mx, my, region ) == cur_unit )
                     gui_set_cursor( CURSOR_STD );
                else
                /* выбранный юнит */
                if ( engine_get_target( mx, my, region ) )
                    gui_set_cursor( CURSOR_ATTACK );
                else
                    if ( mask[mx][my].in_range && ( cur_unit->x != mx || cur_unit->y != my ) && !mask[mx][my].blocked ) {
                        if ( mask[mx][my].mount )
                            gui_set_cursor( CURSOR_MOUNT );
                        else
                            gui_set_cursor( CURSOR_MOVE );
                    }
                    else
                        if ( mask[mx][my].sea_embark ) {
                            if ( cur_unit->embark == EMBARK_SEA )
                                gui_set_cursor( CURSOR_DEBARK );
                            else
                                gui_set_cursor( CURSOR_EMBARK );
                        }
                        else
                            if ( engine_get_select_unit( mx, my, region ) )
                                gui_set_cursor( CURSOR_SELECT );
                            else
                                gui_set_cursor( CURSOR_STD );
            }
            else {
                /* юнит не выбран */
                if ( engine_get_select_unit( mx, my, region ) )
                    gui_set_cursor( CURSOR_SELECT );
                else
                    gui_set_cursor( CURSOR_STD );
            }
            break;
    }
    /* информация о новом юните */
    if ( status == STATUS_INFO || status == STATUS_DEPLOY_INFO ) {
        if ( engine_get_prim_unit( mx, my, region ) )
            if ( mask[mx][my].spot )
                gui_show_full_info( gui->finfo, engine_get_prim_unit( mx, my, region ) );
    }
}

/*
====================================================================
Скрыть все анимированные окна верхнего уровня.
====================================================================
*/
static void engine_begin_frame()
{
    if ( status == STATUS_ATTACK ) {
        anim_draw_bkgnd( terrain_icons->cross );
        anim_draw_bkgnd( terrain_icons->expl1 );
        anim_draw_bkgnd( terrain_icons->expl2 );
    }
    if ( status == STATUS_MOVE && move_image )
        image_draw_bkgnd( move_image );
    gui_draw_bkgnds();
}
/*
====================================================================
Обработайте все запрошенные обновления экрана, нарисуйте окна
и обновите экран.
====================================================================
*/
static void engine_end_frame()
{
    int full_refresh = 0;
//    if ( blind_cpu_turn ) return;
    if ( draw_map ) {
        engine_draw_map();
        full_refresh = 1;
    }
    if ( status == STATUS_ATTACK ) {
        anim_get_bkgnd( terrain_icons->cross );
        anim_get_bkgnd( terrain_icons->expl1 );
        anim_get_bkgnd( terrain_icons->expl2 );
    }
    if ( status == STATUS_MOVE && move_image ) /* при внезапной атаке этот образ еще не создан */
        image_get_bkgnd( move_image );
    gui_get_bkgnds();
    if ( status == STATUS_ATTACK ) {
        anim_draw( terrain_icons->cross );
        anim_draw( terrain_icons->expl1 );
        anim_draw( terrain_icons->expl2 );
    }
    if ( status == STATUS_MOVE && move_image ) {
        image_draw( move_image );
    }
    gui_draw();
    if ( full_refresh )
        refresh_screen( 0, 0, 0, 0 );
    else
        refresh_rects();
}

/*
====================================================================
Обработайте кнопку, которая была нажата.
====================================================================
*/
static void engine_handle_button( int id )
{
    char path[MAX_PATH];
    char str[MAX_LINE];
    int x, y, i, max;
    Unit *unit;
    Action *last_act;

    switch ( id ) {
        /* загрузить */
        case ID_LOAD_OK:
            fdlg_hide( gui->load_menu, 1 );
            engine_hide_game_menu();
            action_queue_load( gui->load_menu->current_name );
            sprintf( str, tr("Load Game '%s'"), gui->load_menu->current_name );
            engine_confirm_action( str );
            break;
        case ID_LOAD_CANCEL:
            fdlg_hide( gui->load_menu, 1 );
            engine_set_status( STATUS_NONE );
            break;
        /* сохранить */
        case ID_SAVE_OK:
            fdlg_hide( gui->save_menu, 1 );
            message_dlg_hide( gui->message_dlg, 1 );
            action_queue_overwrite( gui->save_menu->current_name );
            if ( strcmp ( gui->save_menu->current_name, tr( "<empty file>" ) ) != 0 )
                engine_confirm_action( tr("Overwrite saved game?") );
            break;
        case ID_SAVE_CANCEL:
            fdlg_hide( gui->save_menu, 1 );
            engine_set_status( STATUS_NONE );
            break;
        case ID_NEW_FOLDER:
            fdlg_hide( gui->save_menu, 1 );
            message_dlg_hide( gui->message_dlg, 1 );
            edit_show( gui->edit, tr( "new folder" ) );
            engine_set_status( STATUS_NEW_FOLDER );
            scroll_block_keys = 1;
            break;
        /* параметры */
        case ID_C_SOUND:
#ifdef WITH_SOUND
            config.sound_on = !config.sound_on;
            audio_enable( config.sound_on );
#endif
            break;
        case ID_C_SOUND_INC:
#ifdef WITH_SOUND
            config.sound_volume += 16;
            if ( config.sound_volume > 128 )
                config.sound_volume = 128;
            audio_set_volume( config.sound_volume );
#endif
            break;
        case ID_C_SOUND_DEC:
#ifdef WITH_SOUND
            config.sound_volume -= 16;
            if ( config.sound_volume < 0 )
                config.sound_volume = 0;
            audio_set_volume( config.sound_volume );
#endif
            break;
        case ID_C_SUPPLY:
            config.supply = !config.supply;
            break;
        case ID_C_WEATHER:
            config.weather = !config.weather;
            if ( status == STATUS_GAME_MENU ) {
                cur_weather = scen_get_weather();
                draw_map = 1;
            }
            break;
        case ID_C_GRID:
            config.grid = !config.grid;
            draw_map = 1;
            break;
        case ID_C_SHOW_STRENGTH:
            config.show_bar = !config.show_bar;
            draw_map = 1;
            break;
        case ID_C_SHOW_CPU:
            config.show_cpu_turn = !config.show_cpu_turn;
            break;
        case ID_C_VMODE:
            engine_hide_game_menu();
            gui_vmode_dlg_show();
            status = STATUS_VMODE_DLG;
            break;
        /* главное меню */
        case ID_MENU:
            x = gui->base_menu->frame->img->bkgnd->surf_rect.x + 30 - 1;
            y = gui->base_menu->frame->img->bkgnd->surf_rect.y;
            if ( y + gui->main_menu->frame->img->img->h >= sdl.screen->h )
                y = sdl.screen->h - gui->main_menu->frame->img->img->h;
            group_move( gui->main_menu, x, y );
            group_hide( gui->main_menu, 0 );
            break;
        case ID_OPTIONS:
            fdlg_hide( gui->load_menu, 1 );
            fdlg_hide( gui->save_menu, 1 );
            x = gui->main_menu->frame->img->bkgnd->surf_rect.x + 30 - 1;
            y = gui->main_menu->frame->img->bkgnd->surf_rect.y;
            if ( y + gui->opt_menu->frame->img->img->h >= sdl.screen->h )
                y = sdl.screen->h - gui->opt_menu->frame->img->img->h;
            group_move( gui->opt_menu, x, y );
            group_hide( gui->opt_menu, 0 );
            break;
        case ID_RESTART:
            engine_hide_game_menu();
            action_queue_restart();
            engine_confirm_action( tr("Do you really want to restart this scenario?") );
            break;
        case ID_SCEN:
            camp_loaded = NO_CAMPAIGN;
            config.use_core_units = 0;
            engine_hide_game_menu();
            sprintf( path, "%s/%s/Scenario", get_gamedir(), config.mod_name );
            fdlg_open( gui->scen_dlg, path, "" );
            group_set_active( gui->scen_dlg->group, ID_SCEN_SETUP, 0 );
            group_set_active( gui->scen_dlg->group, ID_SCEN_OK, 0 );
            group_set_active( gui->scen_dlg->group, ID_SCEN_CANCEL, 1 );
            status = STATUS_RUN_SCEN_DLG;
            break;
        case ID_CAMP:
            camp_loaded = FIRST_SCENARIO;
            engine_hide_game_menu();
            sprintf( path, "%s/%s/Scenario", get_gamedir(), config.mod_name );
            fdlg_open( gui->camp_dlg, path, "" );
            group_set_active( gui->camp_dlg->group, ID_CAMP_SETUP, 0 );
            group_set_active( gui->camp_dlg->group, ID_CAMP_OK, 0 );
            group_set_active( gui->camp_dlg->group, ID_CAMP_CANCEL, 1 );
            status = STATUS_RUN_CAMP_DLG;
            break;
        case ID_SAVE:
            engine_hide_game_menu();
            sprintf( path, "%s/%s/Save", get_gamedir(), config.mod_name );
            fdlg_open( gui->save_menu, path, "" );
            group_set_active( gui->save_menu->group, ID_SAVE_OK, 0 );
            group_set_active( gui->save_menu->group, ID_SAVE_CANCEL, 1 );
            engine_set_status( STATUS_SAVE_MENU );
            break;
        case ID_LOAD:
            engine_hide_game_menu();
            sprintf( path, "%s/%s/Save", get_gamedir(), config.mod_name );
            fdlg_open( gui->load_menu, path, "" );
            group_set_active( gui->load_menu->group, ID_LOAD_OK, 0 );
            group_set_active( gui->load_menu->group, ID_LOAD_CANCEL, 1 );
            engine_set_status( STATUS_LOAD_MENU );
            break;
        case ID_QUIT:
            engine_hide_game_menu();
            action_queue_quit();
            engine_confirm_action( tr("Do you really want to quit?") );
            break;
        case ID_AIR_MODE:
            engine_hide_game_menu();
            air_mode = !air_mode;
            draw_map = 1;
            break;
        case ID_END_TURN:
            engine_hide_game_menu();
            action_queue_end_turn();
            group_set_active( gui->base_menu, ID_END_TURN, 0 );
            engine_confirm_action( tr("Do you really want to end your turn?") );
            break;
        case ID_SCEN_INFO:
            engine_hide_game_menu();
            gui_show_scen_info();
            status = STATUS_SCEN_INFO;
            break;
        case ID_CONDITIONS:
            engine_hide_game_menu();
            gui_show_conds();
            status = STATUS_SCEN_INFO; /* is okay for the engine ;) */
            break;
        case ID_CANCEL:
            /* окно подтверждения запускается перед действием, поэтому, если отменить
               попадает во все действия, более поздние, чем top_committed_action
               будет удален. Если не указано, только самое верхнее действие
               удалено */
            while ( actions_top() != top_committed_action ) {
                action_remove_last();
                if ( !top_committed_action ) break;
            }
            /* провалиться */
        case ID_OK:
            engine_set_status( deploy_turn ? STATUS_DEPLOY : STATUS_NONE );
            group_hide( gui->confirm, 1 );
            old_mx = old_my = -1;
            draw_map = 1;
            break;
        case ID_SUPPLY:
            if (cur_unit==0) break;
            action_queue_supply( cur_unit );
            engine_select_unit( cur_unit );
            draw_map = 1;
            engine_hide_unit_menu();
            break;
        case ID_EMBARK_AIR:
            if ( cur_unit->embark == EMBARK_NONE ) {
                action_queue_embark_air( cur_unit, cur_unit->x, cur_unit->y );
                if ( cur_unit->trsp_prop.id )
                    engine_confirm_action( tr("Abandon the ground transporter?") );
                engine_backup_move( cur_unit, -1, -1 );
                engine_hide_unit_menu();
            }
            else
            {
                int drop = 0;
                if ( unit_has_flag( &cur_unit->prop, "parachute" ) )
                {
                    int x = cur_unit->x, y = cur_unit->y;
                    drop = 1;
                    if ( terrain_type_find( map[x][y].terrain_id[0] )->flags[cur_weather] & SUPPLY_AIR)
                    if (player_is_ally(map[x][y].player,cur_unit->player))
                    if (map[x][y].g_unit==0)
                        drop = 0;
                }
                if (!drop)
                {
                    action_queue_debark_air( cur_unit, cur_unit->x, cur_unit->y, 1 );
                    engine_backup_move( cur_unit, -1, -1 );
                    engine_hide_unit_menu();
                }
                else
                {
                    map_get_dropzone_mask(cur_unit);
                    map_set_fog( F_DEPLOY );
                    engine_hide_unit_menu();
                    status = STATUS_DROP;
                }
            }
            draw_map = 1;
            break;
        case ID_SPLIT:
            if (cur_unit==0) break;
            max = unit_get_split_strength(cur_unit);
            for (i=0;i<10;i++)
                group_set_active(gui->split_menu,ID_SPLIT_1+i,i<max);
            x = gui->unit_buttons->frame->img->bkgnd->surf_rect.x + 30 - 1;
            y = gui->unit_buttons->frame->img->bkgnd->surf_rect.y;
            if ( y + gui->split_menu->frame->img->img->h >= sdl.screen->h )
                y = sdl.screen->h - gui->split_menu->frame->img->img->h;
            group_move( gui->split_menu, x, y );
            group_hide( gui->split_menu, 0 );
            break;
        case ID_SPLIT_1:
        case ID_SPLIT_2:
        case ID_SPLIT_3:
        case ID_SPLIT_4:
        case ID_SPLIT_5:
        case ID_SPLIT_6:
        case ID_SPLIT_7:
        case ID_SPLIT_8:
        case ID_SPLIT_9:
        case ID_SPLIT_10:
            if (cur_unit==0) break;
            cur_split_str = id-ID_SPLIT_1+1;
            map_get_split_units_and_hexes(cur_unit,cur_split_str,split_units,&split_unit_count);
            map_set_fog( F_SPLIT_UNIT );
            engine_hide_unit_menu();
            status = STATUS_SPLIT;
            draw_map = 1;
            break;
        case ID_MERGE:
            if (cur_unit&&merge_unit_count>0)
            {
                map_set_fog( F_MERGE_UNIT );
                engine_hide_unit_menu();
                status = STATUS_MERGE;
                draw_map = 1;
            }
            break;
        case ID_REPLACEMENTS:
            if (cur_unit==0) break;
            engine_hide_unit_menu();
            action_queue_replace(cur_unit);
            draw_map = 1;
            break;
        case ID_ELITE_REPLACEMENTS:
            if (cur_unit==0) break;
            engine_hide_unit_menu();
            action_queue_elite_replace(cur_unit);
            draw_map = 1;
            break;
        case ID_DISBAND:
            if (cur_unit==0) break;
            engine_hide_unit_menu();
            action_queue_disband(cur_unit);
            engine_confirm_action( tr("Do you really want to disband this unit?") );
            break;
        case ID_UNDO:
            if (cur_unit==0) break;
            engine_undo_move( cur_unit );
            engine_select_unit( cur_unit );
            engine_focus( cur_unit->x, cur_unit->y, 0 );
            draw_map = 1;
            engine_hide_unit_menu();
            break;
        case ID_RENAME:
            if (cur_unit==0) break;
            engine_hide_unit_menu();
            status = STATUS_RENAME;
            edit_show( gui->edit, cur_unit->name );
            scroll_block_keys = 1;
            break;
        case ID_PURCHASE:
            engine_hide_game_menu();
            engine_select_unit( 0 );
            gui_show_purchase_window();
            status = STATUS_PURCHASE;
            draw_map = 1;
            break;
        case ID_PURCHASE_EXIT:
            purchase_dlg_hide( gui->purchase_dlg, 1 );
            engine_set_status( STATUS_NONE );
            break;
        case ID_DEPLOY:
            if ( avail_units->count > 0 || deploy_turn )
            {
                engine_hide_game_menu();
                engine_select_unit( 0 );
                gui_show_deploy_window();
                map_get_deploy_mask(cur_player,deploy_unit,deploy_turn);
                map_set_fog( F_DEPLOY );
                if ( deploy_turn && (camp_loaded != NO_CAMPAIGN) && STRCMP(camp_cur_scen->id, camp_first) &&
                    (config.use_core_units) )
                {
                    Unit *entry;
                    list_reset( units );
                    /* поиск основных юнитов */
                    while ( ( entry = list_next( units ) ) ) {
                        if (mask[entry->x][entry->y].deploy)
                            entry->core = CORE;
                    }
                }
                status = STATUS_DEPLOY;
                draw_map = 1;
            }
            break;
        case ID_STRAT_MAP:
            engine_hide_game_menu();
            status = STATUS_STRAT_MAP;
            strat_map_update_terrain_layer();
            strat_map_update_unit_layer();
            set_delay( &blink_delay, 500 );
            draw_map = 1;
            break;
        case ID_SCEN_CANCEL:
            fdlg_hide( gui->scen_dlg, 1 );
            engine_set_status( STATUS_NONE );
            break;
        case ID_SCEN_OK:
            fdlg_hide( gui->scen_dlg, 1 );
            group_set_hidden( gui->unit_buttons, ID_REPLACEMENTS, !config.merge_replacements );
            group_set_hidden( gui->unit_buttons, ID_ELITE_REPLACEMENTS, !config.merge_replacements );
            group_set_hidden( gui->unit_buttons, ID_MERGE, config.merge_replacements );
            group_set_hidden( gui->unit_buttons, ID_SPLIT, config.merge_replacements );
            engine_set_status( STATUS_NONE );
            action_queue_start_scen();
            break;
        case ID_CAMP_CANCEL:
            fdlg_hide( gui->camp_dlg, 1 );
            engine_set_status( STATUS_NONE );
            break;
        case ID_CAMP_OK:
            fdlg_hide( gui->camp_dlg, 1 );
            setup.type = SETUP_CAMP_BRIEFING;
            group_set_hidden( gui->unit_buttons, ID_REPLACEMENTS, !config.merge_replacements );
            group_set_hidden( gui->unit_buttons, ID_ELITE_REPLACEMENTS, !config.merge_replacements );
            group_set_hidden( gui->unit_buttons, ID_MERGE, config.merge_replacements );
            group_set_hidden( gui->unit_buttons, ID_SPLIT, config.merge_replacements );
            engine_set_status( STATUS_NONE );
            action_queue_start_camp();
            break;
        case ID_SCEN_SETUP:
            fdlg_hide( gui->scen_dlg, 1 );
            gui_open_scen_setup();
            status = STATUS_RUN_SCEN_SETUP;
            break;
        case ID_SCEN_SETUP_OK:
            fdlg_hide( gui->scen_dlg, 0 );
            sdlg_hide( gui->scen_setup, 1 );
            status = STATUS_RUN_SCEN_DLG;
            break;
        case ID_SCEN_SETUP_SUPPLY:
            config.supply = !config.supply;
            break;
        case ID_SCEN_SETUP_WEATHER:
            if (config.weather < 2)
                config.weather = config.weather + 1;
            else
                config.weather = 0;
            break;
        case ID_SCEN_SETUP_FOG:
            config.fog_of_war = !config.fog_of_war;
            break;
        case ID_SCEN_SETUP_DEPLOYTURN:
            config.deploy_turn = !config.deploy_turn;
            break;
        case ID_SCEN_SETUP_PURCHASE:
            if (config.purchase < 2)
                config.purchase = config.purchase + 1;
            else
                config.purchase = 0;
            break;
        case ID_SCEN_SETUP_MERGE_REPLACEMENTS:
            config.merge_replacements = !config.merge_replacements;
            break;
        case ID_SCEN_SETUP_CTRL:
            setup.ctrl[gui->scen_setup->sel_id] = !(setup.ctrl[gui->scen_setup->sel_id] - 1) + 1;
            gui_handle_player_select( gui->scen_setup->list->cur_item );
            break;
        case ID_SCEN_SETUP_MODULE:
            sdlg_hide( gui->scen_setup, 1 );
            group_set_active( gui->module_dlg->group, ID_MODULE_OK, 0 );
            group_set_active( gui->module_dlg->group, ID_MODULE_CANCEL, 1 );
            sprintf( path, "%s/Default/AI_modules", get_gamedir() );
            fdlg_open( gui->module_dlg, path, "" );
            status = STATUS_RUN_MODULE_DLG;
            break;
        case ID_CAMP_SETUP:
            fdlg_hide( gui->camp_dlg, 1 );
            gui_open_camp_setup();
            status = STATUS_RUN_CAMP_SETUP;
            break;
        case ID_CAMP_SETUP_OK:
            fdlg_hide( gui->camp_dlg, 0 );
            sdlg_hide( gui->camp_setup, 1 );
            config.purchase = config.campaign_purchase + 1;
            if (config.merge_replacements == OPTION_MERGE)
                group_set_active( gui->unit_buttons, ID_REPLACEMENTS, 0 );
            status = STATUS_RUN_CAMP_DLG;
            break;
        case ID_CAMP_SETUP_MERGE_REPLACEMENTS:
            config.merge_replacements = !config.merge_replacements;
            break;
        case ID_CAMP_SETUP_CORE:
            config.use_core_units = !config.use_core_units;
            break;
        case ID_CAMP_SETUP_PURCHASE:
            config.campaign_purchase = !config.campaign_purchase;
            break;
        case ID_MODULE_OK:
            if ( gui->module_dlg->lbox->cur_item ) {
                if ( gui->module_dlg->subdir[0] != 0 )
                    sprintf( path, "%s/%s", gui->module_dlg->subdir, (char*)gui->module_dlg->lbox->cur_item );
                else
                    sprintf( path, "%s", (char*)gui->module_dlg->lbox->cur_item );
                free( setup.modules[gui->scen_setup->sel_id] );
                setup.modules[gui->scen_setup->sel_id] = strdup( path );
                gui_handle_player_select( gui->scen_setup->list->cur_item );
            }
        case ID_MODULE_CANCEL:
            fdlg_hide( gui->module_dlg, 1 );
            sdlg_hide( gui->scen_setup, 0 );
            status = STATUS_RUN_SCEN_SETUP;
            break;
        case ID_DEPLOY_UP:
            gui_scroll_deploy_up();
            break;
        case ID_DEPLOY_DOWN:
            gui_scroll_deploy_down();
            break;
        case ID_APPLY_DEPLOY:
            /* передать все юниты с x != -1 */
            list_reset( avail_units );
            last_act = actions_top();
            while ( ( unit = list_next( avail_units ) ) )
                if ( unit->x != -1 )
                    action_queue_deploy( unit, unit->x, unit->y );
            if ( cur_ctrl == PLAYER_CTRL_HUMAN ) {
                if ( deploy_turn ) {
                    action_queue_end_turn();
                    engine_confirm_actions( last_act, tr("End deployment?") );
                } else {
                    action_queue_set_spot_mask();
                    action_queue_draw_map();
                }
            }
            if ( !deploy_turn ) {
                engine_set_status( STATUS_NONE );
                group_hide( gui->deploy_window, 1 );
            }
            break;
        case ID_CANCEL_DEPLOY:
            list_reset( avail_units );
            while ( ( unit = list_next( avail_units ) ) )
                if ( unit->x != -1 )
                    map_remove_unit( unit );
            draw_map = 1;
            engine_set_status( STATUS_NONE );
            group_hide( gui->deploy_window, 1 );
            map_set_fog( F_SPOT );
            break;
	case ID_VMODE_OK:
		i = select_dlg_get_selected_item_index( gui->vmode_dlg );
		if (i == -1)
			break;
		action_queue_set_vmode( sdl.vmodes[i].width,
					sdl.vmodes[i].height,
					sdl.vmodes[i].fullscreen);
		select_dlg_hide( gui->vmode_dlg, 1 );
		engine_set_status( STATUS_NONE );
		break;
	case ID_VMODE_CANCEL:
		select_dlg_hide( gui->vmode_dlg, 1 );
		engine_set_status( STATUS_NONE );
		break;
        case ID_MOD_SELECT:
            engine_hide_game_menu();
            sprintf( path, "%s", get_gamedir() );
            fdlg_open( gui->mod_select_dlg, path, "" );
            fdlg_hide( gui->mod_select_dlg, 0 );
            group_set_active( gui->mod_select_dlg->group, ID_MOD_SELECT_OK, 0 );
            group_set_active( gui->mod_select_dlg->group, ID_MOD_SELECT_CANCEL, 1 );
            engine_set_status( STATUS_MOD_SELECT );
            break;
        case ID_MOD_SELECT_OK:
            fdlg_hide( gui->mod_select_dlg, 1 );
            action_queue_change_mod();
            engine_confirm_action( tr("Loading new game folder will erase current game. Are you sure?") );
            break;
        case ID_MOD_SELECT_CANCEL:
            fdlg_hide( gui->mod_select_dlg, 1 );
            engine_set_status( STATUS_NONE );
            break;
        case ID_HEADQUARTERS:
            engine_hide_game_menu();
            engine_select_unit( 0 );
            gui_show_headquarters_window();
            status = STATUS_HEADQUARTERS;
            draw_map = 1;
            break;
        case ID_HEADQUARTERS_CLOSE:
            headquarters_dlg_hide( gui->headquarters_dlg, 1 );
            engine_set_status( STATUS_NONE );
            break;
    }
}

/*
====================================================================
Разворачивает данный юнит. Не удаляет с карты.
====================================================================
*/
static void engine_undeploy_unit( Unit *unit )
{
    int mx = unit->x, my = unit->y;
    unit->x = -1; unit->y = -1;

    /* высадить юниты, если они загружены */
    if ( map_check_unit_debark( unit, mx, my, EMBARK_AIR, 1 ) )
        map_debark_unit( unit, -1, -1, EMBARK_AIR, 0 );
    if ( map_check_unit_debark( unit, mx, my, EMBARK_SEA, 1 ) )
        map_debark_unit( unit, -1, -1, EMBARK_SEA, 0 );

    gui_add_deploy_unit( unit );
}

/*
====================================================================
Получайте действия от входных событий или ЦП и ставьте их в очередь.
====================================================================
*/
static void engine_check_events(int *reinit)
{
    int region;
    int hide_edit = 0;
    SDL_Event event;
    Unit *unit;
    int cx, cy, button = 0;
    int mx, my, keypressed = 0;
    SDL_PumpEvents(); /* собирать события в очереди */
    if ( sdl_quit ) term_game = 1; /* завершить игру оконным менеджером */
    if ( status == STATUS_MOVE || status == STATUS_ATTACK )
        return;
    if ( cur_ctrl == PLAYER_CTRL_CPU ) {
        if ( actions_count() == 0 )
            (cur_player->ai_run)();
    }
    else {
        if ( event_get_motion( &cx, &cy ) ) {   //проверьте, произошло ли событие движения с момента последнего вызова
            if ( setup.type == SETUP_RUN_TITLE )    //показать титульный экран и запустить меню
                ;
            else if (status == STATUS_CAMP_BRIEFING) {  //запустить информационный диалог кампании
                gui_handle_message_pane_event(camp_pane, cx, cy, BUTTON_NONE, 0);
            } else {
                if ( status == STATUS_DEPLOY || status == STATUS_DEPLOY_INFO ) {    //человек может развернуть юниты или полная информация об юните при развертывании
                    gui_handle_deploy_motion( cx, cy, &unit );
                    if ( unit ) {
                        if ( status == STATUS_DEPLOY_INFO )
                            gui_show_full_info( gui->finfo, unit );
                        /* отображать информацию об этом юните */
                        gui_show_quick_info( gui->qinfo1, unit );
                        frame_hide( gui->qinfo2, 1 );
                    }
                }
                if ( engine_get_map_pos( cx, cy, &mx, &my, &region ) ) {
                    /* движение мыши */
                    if ( mx != old_mx || my != old_my || region != old_region ) {
                        old_mx = mx; old_my = my, old_region = region;
                        engine_update_info( mx, my, region );   //обновите краткую информацию об объекте и информацию о фрагменте карты
                    }
                }
            }
            gui_handle_motion( cx, cy );    //обрабатывать события меню
        }
        if ( event_get_buttondown( &button, &cx, &cy ) ) {  //проверьте, была ли нажата кнопка, и верните ее значение
            /* щелкнуть мышью */
            if ( gui_handle_button( button, cx, cy, &last_button ) ) {  //обрабатывать события меню (по кнопке)
                engine_handle_button( last_button->id );    //Обработайте кнопку, которая была нажата
#ifdef WITH_SOUND
                wav_play( gui->wav_click ); //выдать звук нажатой кнопки
#endif
            }
            else {
                switch ( status ) {
                    case STATUS_TITLE:  //показать фон
                        /* меню заголовков */
                        if ( button == BUTTON_RIGHT )
                            engine_show_game_menu( cx, cy );
                        break;
                    case STATUS_TITLE_MENU: //запустить меню заголовков
                        if ( button == BUTTON_RIGHT )
                            engine_hide_game_menu();
                        break;
                    case STATUS_SAVE_MENU:  //запуск меню сохранения
                        if ( button == BUTTON_RIGHT )
                            engine_show_game_menu( cx, cy );
                        break;
                    case STATUS_CAMP_BRIEFING:  //запустить информационный диалог кампании
                        gui_handle_message_pane_event(camp_pane, cx, cy, button, 1);
                        break;
                    default:
                        if ( setup.type == SETUP_RUN_TITLE ) break; //показать титульный экран и запустить меню
                        /* проверка колеса мыши */
                        if ( status == STATUS_NONE ) {  //действия, которые разделены на разные фазы
                            if( button == WHEEL_UP ) {
                                scroll_vert = SCROLL_UP;
                                engine_check_scroll( SC_BY_WHEEL );
                            }
                            else if( button == WHEEL_DOWN ) {
                                scroll_vert = SCROLL_DOWN;
                                engine_check_scroll( SC_BY_WHEEL );
                            }
                        }
                        /* выберите юнит из окна развертывания */
                        if ( status == STATUS_DEPLOY) {     //человек может развернуть юниты
                            if ( gui_handle_deploy_click(button, cx, cy) ) {
                                if ( button == BUTTON_RIGHT ) {
                                    engine_show_deploy_unit_info( deploy_unit );
                                }
                                map_get_deploy_mask(cur_player,deploy_unit,deploy_turn);
                                map_set_fog( F_DEPLOY );
                                draw_map = 1;
                                break;
                            }
                        }
                        /* выбор только с карты */
                        if ( !engine_get_map_pos( cx, cy, &mx, &my, &region ) ) break;
                        switch ( status ) {
                            case STATUS_STRAT_MAP:  //показывая стратегическую карту
                                /* переключиться со стратегической карты на тактическую */
                                if ( button == BUTTON_LEFT )
                                    engine_focus( mx, my, 0 );
                                engine_set_status( STATUS_NONE );
                                old_mx = old_my = -1;
                                /* перед обновлением карты очистите экран */
                                SDL_FillRect( sdl.screen, 0, 0x0 );
                                draw_map = 1;
                                break;
                            case STATUS_GAME_MENU:  //игровое меню
                                if ( button == BUTTON_RIGHT )
                                    engine_hide_game_menu();
                                break;
                            case STATUS_DEPLOY_INFO:    //полная информация об юните при развертывании
                                engine_hide_deploy_unit_info();
                                break;
                            case STATUS_DEPLOY:     //человек может развернуть юниты
                                /* развернуть / свернуть */
                                if ( button == BUTTON_LEFT ) {  //левая кнопка мыши
                                    /* развернуть, но проверить, явно ли мы находимся на неразвертываемом модуле */
                                    if ((unit=map_get_undeploy_unit(mx,my,region==REGION_AIR)))
                                    {}
                                    else if ( deploy_unit && mask[mx][my].deploy ) {
                                        Unit *old_unit;
                                        /* подниматься в воздух, если совершается посадка, и явно находиться в воздушном районе в очереди на развертывание */
                                        if ( deploy_turn &&
                                             map_check_unit_embark( deploy_unit, mx, my, EMBARK_AIR, 1 ) &&
                                             (region==REGION_AIR||map[mx][my].g_unit) )
                                            map_embark_unit( deploy_unit, -1, -1, EMBARK_AIR, 0 );
                                        /* погрузиться в морской транспорт при выходе в океан */
                                        else if ( map_check_unit_embark( deploy_unit, mx, my, EMBARK_SEA, 1 ) )
                                            map_embark_unit( deploy_unit, -1, -1, EMBARK_SEA, 0 );
                                        deploy_unit->x = mx; deploy_unit->y = my;
                                        deploy_unit->fresh_deploy = 1;
                                        /* поменять местами юниты, если они уже есть */
                                        old_unit = map_swap_unit( deploy_unit );
                                        gui_remove_deploy_unit( deploy_unit );
                                        if ( old_unit ) engine_undeploy_unit( old_unit );
                                        if ( deploy_unit || deploy_turn ) {
                                            map_get_deploy_mask(cur_player,deploy_unit,deploy_turn);
                                            map_set_fog( F_DEPLOY );
                                        }
                                        else {
                                            map_clear_mask( F_DEPLOY );
                                            map_set_fog( F_DEPLOY );
                                        }
                                        engine_update_info(mx,my,region);
                                        draw_map = 1;
                                    }
                                    /* развернуть юнит из другого региона? */
                                    else
                                        unit=map_get_undeploy_unit(mx,my,region!=REGION_AIR);
                                    if (unit) { /* развернуть? */
                                        map_remove_unit( unit );
                                        engine_undeploy_unit( unit );
                                        map_get_deploy_mask(cur_player,deploy_unit,deploy_turn);
                                        map_set_fog( F_DEPLOY );
                                        engine_update_info(mx,my,region);
                                        draw_map = 1;
                                    }
                                    /* проверьте кнопку end_deploy */
//                                    if (deploy_turn)
//                                        group_set_active(gui->deploy_window,ID_APPLY_DEPLOY,(left_deploy_units->count>0)?0:1);
                                }
                                else if ( button == BUTTON_RIGHT )  //правая кнопка мыши
                                {
                                    if ( mask[mx][my].spot && ( unit = engine_get_prim_unit( mx, my, region ) ) ) {
                                       /* информация */
                                       engine_show_deploy_unit_info( unit );
                                    }
                                }
                                break;
                            case STATUS_DROP:   //выбрать зону сброса парашютистов
                                if ( button == BUTTON_RIGHT ) {  //правая кнопка мыши
                                    /* очистить статус */
                                    engine_set_status( STATUS_NONE );
                                    gui_set_cursor(CURSOR_STD);
                                    map_set_fog( F_IN_RANGE );
                                    mask[cur_unit->x][cur_unit->y].fog = 0;
                                    draw_map = 1;
                                }
                                else
                                    if ( button == BUTTON_LEFT )  //левая кнопка мыши
                                        if ( mask[mx][my].deploy ) {
                                            action_queue_debark_air( cur_unit, mx, my, 0 );
                                            map_set_fog( F_SPOT );
                                            engine_set_status( STATUS_NONE );
                                            gui_set_cursor(CURSOR_STD);
                                            draw_map = 1;
                                        }
                                break;
                            case STATUS_MERGE:  //человек может сливаться с юнитами
                                if ( button == BUTTON_RIGHT ) {  //правая кнопка мыши
                                    /* очистить статус */
                                    engine_set_status( STATUS_NONE );
                                    gui_set_cursor(CURSOR_STD);
                                    map_set_fog( F_IN_RANGE );
                                    mask[cur_unit->x][cur_unit->y].fog = 0;
                                    draw_map = 1;
                                }
                                else
                                    if ( button == BUTTON_LEFT )  //левая кнопка мыши
                                        if ( mask[mx][my].merge_unit ) {
                                            action_queue_merge( cur_unit, mask[mx][my].merge_unit );
                                            map_set_fog( F_SPOT );
                                            engine_set_status( STATUS_NONE );
                                            gui_set_cursor(CURSOR_STD);
                                            draw_map = 1;
                                        }
                                break;
                            case STATUS_SPLIT:  //человек хочет разделить юнит
                                if ( button == BUTTON_RIGHT ) {  //правая кнопка мыши
                                    /* очистить статус */
                                    engine_set_status( STATUS_NONE );
                                    gui_set_cursor(CURSOR_STD);
                                    map_set_fog( F_IN_RANGE );
                                    mask[cur_unit->x][cur_unit->y].fog = 0;
                                    draw_map = 1;
                                }
                                else
                                    if ( button == BUTTON_LEFT )  //левая кнопка мыши
                                    {
                                        if ( mask[mx][my].split_unit||mask[mx][my].split_okay ) {
                                            action_queue_split( cur_unit, cur_split_str, mx, my, mask[mx][my].split_unit );
                                            cur_split_str = 0;
                                            map_set_fog( F_SPOT );
                                            engine_set_status( STATUS_NONE );
                                            gui_set_cursor(CURSOR_STD);
                                            draw_map = 1;
                                        }
                                    }
                                break;
                            case STATUS_UNIT_MENU:
                                if ( button == BUTTON_RIGHT )  //правая кнопка мыши
                                    engine_hide_unit_menu();
                                break;
                            case STATUS_SCEN_INFO:  //показать информацию о сценарии
                                engine_set_status( STATUS_NONE );
                                frame_hide( gui->sinfo, 1 );
                                old_mx = old_my = -1;
                                break;
                            case STATUS_INFO:   //показать полную информацию об юните
                                engine_set_status( STATUS_NONE );
                                frame_hide( gui->finfo, 1 );
                                old_mx = old_my = -1;
                                break;
                            case STATUS_NONE:   //действия, которые разделены на разные фазы
                                switch ( button ) {
                                    case BUTTON_LEFT:  //левая кнопка мыши
                                        if ( cur_unit ) {
                                            /* обрабатывать текущий блок */
                                            if ( cur_unit->x == mx && cur_unit->y == my && engine_get_prim_unit( mx, my, region ) == cur_unit )
                                                engine_show_unit_menu( cx, cy );
                                            else
                                            if ( ( unit = engine_get_target( mx, my, region ) ) ) {
                                                action_queue_attack( cur_unit, unit );
                                                frame_hide( gui->qinfo1, 1 );
                                                frame_hide( gui->qinfo2, 1 );
                                            }
                                            else
                                                if ( mask[mx][my].in_range && !mask[mx][my].blocked ) {
                                                    action_queue_move( cur_unit, mx, my );
                                                    frame_hide( gui->qinfo1, 1 );
                                                    frame_hide( gui->qinfo2, 1 );
                                                }
                                                else
                                                    if ( mask[mx][my].sea_embark ) {
                                                        if ( cur_unit->embark == EMBARK_NONE )
                                                            action_queue_embark_sea( cur_unit, mx, my );
                                                        else
                                                            action_queue_debark_sea( cur_unit, mx, my );
                                                        engine_backup_move( cur_unit, mx, my );
                                                        draw_map = 1;
                                                    }
                                                    else
                                                        if ( ( unit = engine_get_select_unit( mx, my, region ) ) && cur_unit != unit ) {
                                                            /* сначала захватите флаг для человеческого юнита */
                                                            if ( cur_ctrl == PLAYER_CTRL_HUMAN ) {
                                                                if ( engine_capture_flag( cur_unit ) ) {
                                                                    /* ПРОВЕРИТЬ, ЗАВЕРШЕН СЦЕНАРИЙ */
                                                                    if ( scen_check_result( 0 ) )  {
                                                                        engine_finish_scenario();
                                                                        break;
                                                                    }
                                                                }
                                                            }
                                                            engine_select_unit( unit );
                                                            engine_clear_backup();
                                                            engine_update_info( mx, my, region );
                                                            draw_map = 1;
                                                        }
                                        }
                                        else
                                            if ( ( unit = engine_get_select_unit( mx, my, region ) ) && cur_unit != unit ) {
                                                /* выбрать юнит */
                                                engine_select_unit( unit );
                                                engine_update_info( mx, my, region );
                                                draw_map = 1;
#ifdef WITH_SOUND
                                                wav_play( terrain_icons->wav_select );
#endif
                                            }
                                        break;
                                    case BUTTON_RIGHT:  //правая кнопка мыши
                                        if ( cur_unit == 0 ) {
                                            if ( mask[mx][my].spot && ( unit = engine_get_prim_unit( mx, my, region ) ) ) {
                                                /* показать информацию об юните */
                                                gui_show_full_info( gui->finfo, unit );
                                                status = STATUS_INFO;
                                                gui_set_cursor( CURSOR_STD );
                                            }
                                            else {
                                                /* показать меню */
                                                engine_show_game_menu( cx, cy );
                                            }
                                        }
                                        else
                                            if ( cur_unit ) {
                                                /* обрабатывать текущий юнит */
                                                if ( cur_unit->x == mx && cur_unit->y == my && engine_get_prim_unit( mx, my, region ) == cur_unit )
                                                    engine_show_unit_menu( cx, cy );
                                                else {
                                                    /* сначала захватите флаг для человеческого юнита */
                                                    if ( cur_ctrl == PLAYER_CTRL_HUMAN ) {
                                                        if ( engine_capture_flag( cur_unit ) ) {
                                                            /* ПРОВЕРИТЬ, ЗАВЕРШЕН СЦЕНАРИЙ */
                                                            if ( scen_check_result( 0 ) )  {
                                                                engine_finish_scenario();
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    /* юнит выпуска */
                                                    engine_select_unit( 0 );
                                                    engine_update_info( mx, my, region );
                                                    draw_map = 1;
                                                }
                                            }
                                        break;
                                }
                                break;
                        }
                    }
            }
        }
        else
            if ( event_get_buttonup( &button, &cx, &cy ) ) {    //проверьте, была ли нажата кнопка, и верните ее значение и текущий положение мыши
                if (status == STATUS_CAMP_BRIEFING) {
                     const char *result;
                     gui_handle_message_pane_event(camp_pane, cx, cy, button, 0);

                     /* проверка на отбор / отказ */
                     result = gui_get_message_pane_selection(camp_pane);
                     if (result && strcmp(result, "nextscen") == 0) {
                         /* стартовый сценарий */
                         sprintf( setup.fname, "%s", camp_cur_scen->scen );
                         setup.type = SETUP_DEFAULT_SCEN;
                         end_scen = 1;
                         *reinit = 1;
                     } else if (result) {
                         /* загрузить следующую запись кампании */
                         setup.type = SETUP_CAMP_BRIEFING;
                         engine_store_debriefing(0);
                         if ( !camp_set_next( result ) )
                             setup.type = SETUP_RUN_TITLE;
                         end_scen = 1;
                         *reinit = 1;
                     }
                     return;
                }
                /* если была нажата кнопка отпустить */
                if ( last_button ) {
                    if ( !last_button->lock ) {
                        last_button->down = 0;
                        if ( last_button->active )
                            last_button->button_rect.x = 0;
                    }
                    if ( last_button->button_rect.x == 0 )
                        if ( button_focus( last_button, cx, cy ) )
                            last_button->button_rect.x = last_button->surf_rect.w;
                    last_button = 0;
                }
            }
        if ( SDL_PollEvent( &event ) )
        {
            if ( event.type == SDL_KEYDOWN )
            {
                /* разрешить зеркальное отображение где угодно */
                if (event.key.keysym.sym==SDLK_TAB)
                {
                    gui_mirror_asymm_windows();
                    keypressed = 1;
                }
                if (event.key.keysym.sym==SDLK_BACKQUOTE)
                {
                    if ( !gui->message_dlg->edit_box->label->frame->img->bkgnd->hide )
                    {
                        message_dlg_reset( gui->message_dlg );
                        if ( status == STATUS_NONE )
                        {
                            old_mx = old_my = -1;
                            scroll_block_keys = 0;
                        }
                    }
                    message_dlg_hide(gui->message_dlg, !gui->message_dlg->edit_box->label->frame->img->bkgnd->hide);
                    keypressed = 1;
                }
                if ( !gui->message_dlg->edit_box->label->frame->img->bkgnd->hide ) {
                    if ( event.key.keysym.sym == SDLK_RETURN ) {    //Enter клавиша
                        /* проверка различных чит-кодов */
                        if ( strspn( gui->message_dlg->edit_box->text, "/prestige" ) == 9  && cur_player != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_player->cur_prestige += atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d PRESTIGE activated", atoi( temp ) );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/core" ) == 5  && cur_player != 0 && config.use_core_units )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_player->core_limit += atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d CORE UNITS activated", atoi( temp ) );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/aux" ) == 4  && cur_player != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_player->unit_limit += atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d AUXILIARY UNITS activated", atoi( temp ) );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/sea" ) == 4  && cur_player != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_player->sea_trsp_count += atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d SEA TRANSPORTS activated", atoi( temp ) );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/sea" ) == 4  && cur_player != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_player->sea_trsp_count += atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d SEA TRANSPORTS activated", atoi( temp ) );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/exp" ) == 4  && cur_unit != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_unit->exp = 0;
                                unit_add_exp( cur_unit, atoi( temp ) );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d UNIT EXPERIENCE activated", atoi( temp ) );
                                engine_draw_map();
                                gui_show_quick_info( gui->qinfo1, cur_unit );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/str" ) == 4  && cur_unit != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_unit->str = atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d UNIT STRENGTH activated", atoi( temp ) );
                                engine_draw_map();
                                gui_show_quick_info( gui->qinfo1, cur_unit );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/ent" ) == 4  && cur_unit != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_unit->entr = atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d UNIT ENTRENCHMENT activated", atoi( temp ) );
                                engine_draw_map();
                                gui_show_quick_info( gui->qinfo1, cur_unit );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/fuel" ) == 5  && cur_unit != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_unit->cur_fuel = atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d UNIT FUEL activated", atoi( temp ) );
                                engine_draw_map();
                                gui_show_quick_info( gui->qinfo1, cur_unit );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/ammo" ) == 5  && cur_unit != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                cur_unit->cur_ammo = atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d UNIT AMMO activated", atoi( temp ) );
                                engine_draw_map();
                                gui_show_quick_info( gui->qinfo1, cur_unit );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/turns" ) == 6  && scen_info != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                turn += atoi( temp );
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %d SCENARIO TURN COUNT activated", atoi( temp ) );
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/weather" ) == 8  && scen_info != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                if ( atoi( temp ) >= 0 && atoi( temp ) <= 3 )
                                {
                                    cur_weather = atoi( temp ) + 4 * (int)( cur_weather / 4 );
                                    sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %s WEATHER activated", weather_types[cur_weather].name );
                                }
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/ground" ) == 7  && scen_info != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                if ( atoi( temp ) >= 0 && atoi( temp ) <= 2 )
                                {
                                    cur_weather = ( cur_weather % 4 ) + 4 * atoi( temp );
                                    sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %s GROUND CONDITIONS activated", weather_types[cur_weather].ground_conditions );
                                    draw_map = 1;
                                }
                            }
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/fog of war" ) == 11  && scen_info != 0 )
                        {
                            config.fog_of_war = !config.fog_of_war;
                            if ( config.fog_of_war )
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - FOG OF WAR activated" );
                            else
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - FOG OF WAR deactivated" );
                            map_set_spot_mask();
                            map_set_fog_by_player( cur_player );
                            draw_map = 1;
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/all eqp" ) == 8  && scen_info != 0 )
                        {
                            config.all_equipment = !config.all_equipment;
                            if ( config.all_equipment )
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - ALL EQUIPMENT activated" );
                            else
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - ALL EQUIPMENT deactivated" );
                            purchase_dlg_hide( gui->purchase_dlg, 1 );
                            engine_set_status( STATUS_NONE );
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/no zoc" ) == 7  && scen_info != 0 )
                        {
                            config.zones_of_control = !config.zones_of_control;
                            if ( config.zones_of_control )
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - ZONES OF CONTROL activated" );
                            else
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - ZONES OF CONTROL deactivated" );
                            cur_unit = 0;
                            map_set_fog_by_player( cur_player );
                            draw_map = 1;
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/uber units" ) == 11  && scen_info != 0 )
                        {
                            cur_player->uber_units = !cur_player->uber_units;
                            if ( cur_player->uber_units )
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - UBER UNITS activated" );
                            else
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - UBER UNITS deactivated" );
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/turbo units" ) == 12  && scen_info != 0 )
                        {
                            config.turbo_units = !config.turbo_units;
                            if ( config.turbo_units ) {
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - TURBO UNITS activated" );
                                list_reset( units );
                                while ( ( unit = list_next( units ) ) ) {
                                    unit->cur_mov_saved = unit->cur_mov;
                                    unit->cur_mov = 50;
                                }
                            }
                            else {
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - TURBO UNITS deactivated" );
                                list_reset( units );
                                while ( ( unit = list_next( units ) ) ) {
                                    unit->cur_mov = unit->cur_mov_saved;
                                }
                            }
                            cur_unit = 0;
                            map_set_fog_by_player( cur_player );
                            draw_map = 1;
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/force retreat" ) == 14  && scen_info != 0 )
                        {
                            cur_player->force_retreat = !cur_player->force_retreat;
                            if ( cur_player->force_retreat )
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - FORCE RETREAT activated" );
                            else
                                sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - FORCE RETREAT deactivated" );
                        }
                        else if ( strspn( gui->message_dlg->edit_box->text, "/endscn" ) == 7  && scen_info != 0 )
                        {
                            char *temp;
                            temp = strrchr( gui->message_dlg->edit_box->text, ' ' );
                            if (temp != NULL)
                            {
                                temp++;
                                if ( atoi( temp ) >= 0 && atoi( temp ) < vcond_count )
                                {
                                    cheat_endscn = 1;
                                    strcpy( scen_result, vconds[atoi( temp )].result );
                                    strcpy( scen_message, vconds[atoi( temp )].message );
                                    sprintf( gui->message_dlg->edit_box->text, "CHEAT CODE - %s END SCENARIO activated", scen_result );

                                }
                            }
                        }
                        message_dlg_add_text( gui->message_dlg );
                        message_dlg_reset( gui->message_dlg );
                    }
                    if ( event.key.keysym.sym == SDLK_ESCAPE )
                    {
                        hide_edit = 1;
                    }
                    if ( hide_edit ) {
                        message_dlg_reset( gui->message_dlg );
                        engine_set_status( STATUS_NONE );
                        message_dlg_hide( gui->message_dlg, 1 );
                        old_mx = old_my = -1;
                        scroll_block_keys = 0;
                    }
                    else
                    {
                        if ( event.key.keysym.sym != SDLK_BACKQUOTE )   //не нажата тильда
                        {
                            edit_handle_key( gui->message_dlg->edit_box, event.key.keysym.sym,
                                             event.key.keysym.mod, event.key.keysym.unicode );
#ifdef WITH_SOUND
                            wav_play( gui->wav_edit );
#endif
                        }
                    }
                } else if ( status == STATUS_NONE || status == STATUS_INFO || status == STATUS_UNIT_MENU ||
                     status == STATUS_HEADQUARTERS ) {  //вести диалог в штаб-квартире
                    int shiftPressed = event.key.keysym.mod&KMOD_LSHIFT||event.key.keysym.mod&KMOD_RSHIFT;
                    if ( (status != STATUS_UNIT_MENU) && (status != STATUS_HEADQUARTERS) &&
                         (event.key.keysym.sym == SDLK_n || //клавиша n
                         (event.key.keysym.sym == SDLK_f&&!shiftPressed) || //клавиша f
                         (event.key.keysym.sym == SDLK_m&&!shiftPressed) ) ) {  //клавиша m
                        int stype = (event.key.keysym.sym==SDLK_n)?0:(event.key.keysym.sym==SDLK_f)?1:2;
                        /* выберите следующий отряд, у которого есть любое движение
                           или нападение осталось */
                        list_reset( units );
                        if ( cur_unit != 0 )
                            while ( ( unit = list_next( units ) ) )
                                if ( cur_unit == unit )
                                    break;
                        /* получить следующий юнит */
                        while ( ( unit = list_next( units ) ) ) {
                            if ( unit->killed ) continue;
                            if ( unit->is_guarding ) continue;
                            if ( unit->player == cur_player )
                                if ( ((stype==0||stype==2)&&unit->cur_mov > 0) ||
                                     ((stype==0||stype==1)&&unit->cur_atk_count > 0) )
                                    break;
                        }
                        if ( unit == 0 ) {
                            /* искать снова с начала списка */
                            list_reset( units );
                            while ( ( unit = list_next( units ) ) ) {
                                if ( unit->killed ) continue;
                                if ( unit->is_guarding ) continue;
                                if ( unit->player == cur_player )
                                    if ( ((stype==0||stype==2)&&unit->cur_mov > 0) ||
                                         ((stype==0||stype==1)&&unit->cur_atk_count > 0) )
                                        break;
                            }
                        }
                        if ( unit ) {
                            engine_select_unit( unit );
                            engine_focus( cur_unit->x, cur_unit->y, 0 );
                            draw_map = 1;
                            if ( status == STATUS_INFO )
                                gui_show_full_info( gui->finfo, unit );
                            gui_show_quick_info( gui->qinfo1, cur_unit );
                            frame_hide( gui->qinfo2, 1 );
                        }
                        keypressed = 1;
                    } else
                    if ( (status != STATUS_UNIT_MENU) && (status != STATUS_HEADQUARTERS) &&
                         (event.key.keysym.sym == SDLK_p ||
                         (event.key.keysym.sym == SDLK_f&&shiftPressed) ||
                         (event.key.keysym.sym == SDLK_m&&shiftPressed) ) ) {
                        int stype = (event.key.keysym.sym==SDLK_p)?0:(event.key.keysym.sym==SDLK_f)?1:2;
                        /* выберите предыдущий отряд, у которого есть движение
                           или нападение осталось */
                        list_reset( units );
                        if ( cur_unit != 0 )
                            while ( ( unit = list_next( units ) ) )
                                if ( cur_unit == unit )
                                    break;
                        /* получить предыдущий блок */
                        while ( ( unit = list_prev( units ) ) ) {
                            if ( unit->killed ) continue;
                            if ( unit->is_guarding ) continue;
                            if ( unit->player == cur_player )
                                if ( ((stype==0||stype==2)&&unit->cur_mov > 0) ||
                                     ((stype==0||stype==1)&&unit->cur_atk_count > 0) )
                                    break;
                        }
                        if ( unit == 0 ) {
                            /* искать снова с конца списка */
                            units->cur_entry = &units->tail;
                            while ( ( unit = list_prev( units ) ) ) {
                                if ( unit->killed ) continue;
                                if ( unit->is_guarding ) continue;
                                if ( unit->player == cur_player )
                                    if ( ((stype==0||stype==2)&&unit->cur_mov > 0) ||
                                         ((stype==0||stype==1)&&unit->cur_atk_count > 0) )
                                        break;
                            }
                        }
                        if ( unit ) {
                            engine_select_unit( unit );
                            engine_focus( cur_unit->x, cur_unit->y, 0 );
                            draw_map = 1;
                            if ( status == STATUS_INFO )
                                gui_show_full_info( gui->finfo, unit );
                            gui_show_quick_info( gui->qinfo1, cur_unit );
                            frame_hide( gui->qinfo2, 1 );
                        }
                        keypressed = 1;
                    } else {
                        Uint8 *keystate; int numkeys;
                        keypressed = 1;
                        switch (event.key.keysym.sym)
                        {
                            case SDLK_t: if (status!=STATUS_UNIT_MENU) engine_handle_button(ID_AIR_MODE); break;
                            case SDLK_o: if (status!=STATUS_UNIT_MENU) engine_handle_button(ID_STRAT_MAP); break;
                            case SDLK_d: if (status!=STATUS_UNIT_MENU) engine_handle_button(ID_DEPLOY); break;
                            case SDLK_e: if (status!=STATUS_UNIT_MENU) engine_handle_button(ID_END_TURN); break;
                            case SDLK_v: if (status!=STATUS_UNIT_MENU) engine_handle_button(ID_C_VMODE); break;
                            case SDLK_g: if (status!=STATUS_UNIT_MENU) engine_handle_button(ID_C_GRID); break;
                            case SDLK_PERIOD: if (status!=STATUS_UNIT_MENU) engine_handle_button(ID_C_SHOW_STRENGTH); break;
                            case SDLK_i: if (status!=STATUS_UNIT_MENU) engine_handle_button(ID_SCEN_INFO); break;

                            case SDLK_u: engine_handle_button(ID_UNDO); break;
                            case SDLK_s: engine_handle_button(ID_SUPPLY); break;
                            case SDLK_j: engine_handle_button(ID_MERGE); break;
                            case SDLK_h: engine_handle_button(ID_HEADQUARTERS); break;
                            case SDLK_1:
                            case SDLK_2:
                            case SDLK_3:
                            case SDLK_4:
                            case SDLK_5:
                            case SDLK_6:
                            case SDLK_7:
                            case SDLK_8:
                            case SDLK_9:
                                if ( config.merge_replacements == OPTION_MERGE )
                                {
                                    keystate = SDL_GetKeyState(&numkeys);
                                    if (keystate[SDLK_x])
                                        engine_handle_button(ID_SPLIT_1+event.key.keysym.sym-SDLK_1);
                                    break;
                                }
                            case SDLK_MINUS:
                                if (cur_unit) cur_unit->is_guarding = !cur_unit->is_guarding;
                                draw_map = 1;
                                break;
                            default: keypressed = 0; break;
                        }
                    }
                } else if ( status == STATUS_CONF ) {   //запустить окно подтверждения
                    if ( event.key.keysym.sym == SDLK_RETURN )
                        engine_handle_button(ID_OK);
                    else if ( event.key.keysym.sym == SDLK_ESCAPE )
                        engine_handle_button(ID_CANCEL);
                } else if ( status == STATUS_RENAME || status == STATUS_SAVE_EDIT || status == STATUS_NEW_FOLDER ) {
                    if ( event.key.keysym.sym == SDLK_RETURN ) {
                        /* применять */
                        switch ( status ) {
                            case STATUS_RENAME: //переименовать блок
                                strcpy_lt( cur_unit->name, gui->edit->text, 20 );
                                hide_edit = 1;
                                break;
                            case STATUS_SAVE_EDIT:  //запуск сохранения редактирования
                                if ( dir_check( gui->save_menu->lbox->items, gui->edit->text ) != -1 )
                                    engine_confirm_action( tr("Overwrite saved game?") );
                                slot_save( gui->edit->text, gui->save_menu->subdir );
                                hide_edit = 1;
                                break;
                            case STATUS_NEW_FOLDER: //создание нового имени папки
                                dir_create( gui->edit->text, gui->save_menu->subdir );
                                char path[MAX_PATH];
                                sprintf( path, "%s/%s/Save", config.dir_name, config.mod_name );
                                fdlg_open( gui->save_menu, path, gui->save_menu->subdir );
                                group_set_active( gui->save_menu->group, ID_SAVE_OK, 0 );
                                group_set_active( gui->save_menu->group, ID_SAVE_CANCEL, 1 );
                                group_set_active( gui->save_menu->group, ID_NEW_FOLDER, 1 );
                                hide_edit = 1;
                                break;
                        }
                        keypressed = 1;
                    }
                    else
                        if ( event.key.keysym.sym == SDLK_ESCAPE )  //Escape клавиша
                        {
                            hide_edit = 1;
                            if ( status == STATUS_NEW_FOLDER )
                            {
                                char path[MAX_PATH];
                                sprintf( path, "%s/%s/Save", config.dir_name, config.mod_name );
                                fdlg_open( gui->save_menu, path, gui->save_menu->subdir );
                                group_set_active( gui->save_menu->group, ID_SAVE_OK, 0 );
                                group_set_active( gui->save_menu->group, ID_SAVE_CANCEL, 1 );
                                group_set_active( gui->save_menu->group, ID_NEW_FOLDER, 1 );
                            }
                        }
                    if ( hide_edit ) {
                        engine_set_status( STATUS_NONE );
                        edit_hide( gui->edit, 1 );
                        old_mx = old_my = -1;
                        scroll_block_keys = 0;
                    }
                    else {
                        edit_handle_key( gui->edit, event.key.keysym.sym, event.key.keysym.mod, event.key.keysym.unicode );
#ifdef WITH_SOUND
                        wav_play( gui->wav_edit );
#endif
                    }
                }
#ifdef WITH_SOUND
                if (keypressed)
                    wav_play( gui->wav_click );
#endif
            }
        }
        /* прокрутка */
        engine_check_scroll( SC_NORMAL );
    }
}

/*
====================================================================
Получите следующих бойцов, предполагая, что cur_unit атакует cur_target.
Установите cur_atk и cur_def и верните True, если есть еще
комбатанты.
====================================================================
*/
static int engine_get_next_combatants()
{
    int fight = 0;
    char str[MAX_LINE];
    /* проверьте, есть ли опорные блоки; если да, начни бой
       между атакующим и этими юнитами */
    if ( df_units->count > 0 ) {
        cur_atk = list_first( df_units );
        cur_def = cur_unit;
        fight = 1;
        defFire = 1;
        /* установить сообщение, если увидено */
        if ( !blind_cpu_turn ) {
            if ( unit_has_flag( cur_atk->sel_prop, "artillery" ) )
                sprintf( str, tr("Defensive Fire") );
            else
                if ( unit_has_flag( cur_atk->sel_prop, "air_defense" ) )
                    sprintf( str, tr("Air-Defense") );
                else
                    sprintf( str, tr("Interceptors") );
            label_write( gui->label_center, gui->font_error, str );
        }
    }
    else {
        /* четкая информация */
        if ( blind_cpu_turn )
        {
            label_hide( gui->label_left, 1 );
            label_hide( gui->label_center, 1 );
            label_hide( gui->label_right, 1 );
        }
        /* нормальная атака */
        cur_atk = cur_unit;
        cur_def = cur_target;
        fight = 1;
        defFire = 0;
    }
    return fight;
}

/*
====================================================================
Юнит полностью подавлен, поэтому проверьте, не
  ничего не делает
  пытается перейти на соседнюю плитку
  сдается, потому что не может уйти
====================================================================
*/
enum {
    UNIT_STAYS = 0,
    UNIT_FLEES,
    UNIT_SURRENDERS
};
static void engine_handle_suppr( Unit *unit, int *type, int *x, int *y )
{
    int i, nx, ny;
    *type = UNIT_STAYS;
    if ( unit->sel_prop->mov == 0 ) return;
    /* 80% шанс, что отряд хочет бежать */
    if ( DICE(10) <= 8 || ( cur_player->force_retreat && unit->player != cur_player ) ) {
        unit->cur_mov = 1;
        map_get_unit_move_mask( unit );
        /* получить лучший близкий гекс. если нет: сдаться */
        for ( i = 0; i < 6; i++ )
            if ( get_close_hex_pos( unit->x, unit->y, i, &nx, &ny ) )
                if ( mask[nx][ny].in_range && !mask[nx][ny].blocked ) {
                    *type = UNIT_FLEES;
                    *x = nx; *y = ny;
                    return;
                }
        /* сдаваться! */
        *type = UNIT_SURRENDERS;
    }
}

/*
====================================================================
Проверить, остается ли отряд над вражеским флагом, и захватить
Это. Верните True, если флаг был захвачен.
====================================================================
*/
static int engine_capture_flag( Unit *unit ) {
    if ( !unit_has_flag( unit->sel_prop, "flying" ) )
        if ( !unit_has_flag( unit->sel_prop, "swimming" ) )
            if ( map[unit->x][unit->y].nation != 0 )
                if ( !player_is_ally( map[unit->x][unit->y].player, unit->player ) ) {
                    /* захватить */
                    map[unit->x][unit->y].nation = unit->nation;
                    map[unit->x][unit->y].player = unit->player;
                    /* завоеванный флаг снова получает способность развертывания через несколько ходов */
                    map[unit->x][unit->y].deploy_center = 3;
                    /* обычный флаг дает 40, победная цель 80,
                     * нет потери престижа для другого игрока */
                    unit->player->cur_prestige += 40;
                    if (map[unit->x][unit->y].obj)
                            unit->player->cur_prestige += 40;
                    return 1;
                }
    return 0;
}

/*
====================================================================
Обработайте следующее действие и выполнение его.
====================================================================
*/
static void engine_handle_next_action( int *reinit )
{
    Action *action = 0;
    int enemy_spotted = 0;
    int depth, flags, i, j;
    char name[MAX_NAME];
    /* очередь действий блокировки? */
    if ( status == STATUS_CONF || status == STATUS_ATTACK || status == STATUS_MOVE )
        return;
    /* получить действие */
    if ( ( action = actions_dequeue() ) == 0 )
        return;
    /* справиться */
    switch ( action->type ) {
        case ACTION_START_SCEN: //запуск сценария
            camp_delete();
            setup.type = SETUP_INIT_SCEN;   //используйте информацию о настройке, чтобы перезаписать настройки сценария по умолчанию
            *reinit = 1;
            end_scen = 1;
            break;
        case ACTION_START_CAMP: //запуск компании
            setup.type = SETUP_INIT_CAMP;   //загрузить всю кампанию и использовать значения по умолчанию для  сценарии
            *reinit = 1;
            end_scen = 1;
            break;
        case ACTION_OVERWRITE:  //перезапись игры
            status = STATUS_SAVE_EDIT;
            if (gui->save_menu->current_name)
            {
                char time[MAX_NAME];
                currentDateTime( time );
                snprintf( name, MAX_NAME, "(%s) %s, turn %i", time, scen_info->name, turn + 1 );
            }
            else
            {
                snprintf( name, MAX_NAME, "%s", gui->save_menu->current_name );
            }
            edit_show( gui->edit, name );
            scroll_block_keys = 1;
            break;
        case ACTION_LOAD:   //загрузить игру
            setup.type = SETUP_LOAD_GAME;
            strcpy( setup.fname, gui->load_menu->current_name );
            *reinit = 1;
            end_scen = 1;
            break;
        case ACTION_RESTART:    //рестарт
            strcpy( setup.fname, scen_info->fname );
            setup.type = SETUP_INIT_SCEN;
            *reinit = 1;
            end_scen = 1;
            if (config.use_core_units && (camp_loaded != NO_CAMPAIGN) && !STRCMP(camp_cur_scen->id, camp_first))
                camp_loaded = RESTART_CAMPAIGN;
            break;
        case ACTION_QUIT:   //выйти из игры
            engine_set_status( STATUS_NONE );
            end_scen = 1;
            break;
        case ACTION_CHANGE_MOD: //сменить мод
            snprintf( config.mod_name, MAX_NAME, "%s", gui->mod_select_dlg->current_name );
            frame_hide( gui->qinfo1, 1 );
            frame_hide( gui->qinfo2, 1 );
            scen_delete();
            players_delete();
            map_delete();
            cur_player = 0;
            setup.type = SETUP_RUN_TITLE;
            engine_set_status( STATUS_TITLE );
            break;
        case ACTION_SET_VMODE:  //сменить разрешение
            flags = SDL_SWSURFACE;
            if ( action->full ) flags |= SDL_FULLSCREEN;
            depth = SDL_VideoModeOK( action->w, action->h, 32, flags );
            if ( depth == 0 ) {
                fprintf( stderr, tr("Video Mode: %ix%i, Fullscreen: %i not available\n"),
                         action->w, action->h, action->full );
            }
            else {
                /* видеомод */
                SDL_SetVideoMode( action->w, action->h, depth, flags );
                printf(tr("Applied video mode %dx%dx%d %s\n"),
                                        action->w, action->h, depth,
                                        (flags&SDL_FULLSCREEN)?tr("Fullscreen"):
                                            tr("Window"));
                /* отрегулировать окна */
                gui_adjust();
                if ( setup.type != SETUP_RUN_TITLE ) {
                    /* сбросить размер карты движка (количество плиток на экране) */
                    for ( i = map_sx, map_sw = 0; i < gui->map_frame->w; i += hex_x_offset )
                        map_sw++;
                    for ( j = map_sy, map_sh = 0; j < gui->map_frame->h; j += hex_h )
                        map_sh++;
                    /* сбросить позицию карты, если необходимо */
                    if ( map_x + map_sw >= map_w )
                        map_x = map_w - map_sw;
                    if ( map_y + map_sh >= map_h )
                        map_y = map_h - map_sh;
                    if ( map_x < 0 ) map_x = 0;
                    if ( map_y < 0 ) map_y = 0;
                    /* воссоздать стратегическую карту */
                    strat_map_delete();
                    strat_map_create();
                    /* воссоздать экранный буфер */
                    free_surf( &sc_buffer );
                    sc_buffer = create_surf( sdl.screen->w, sdl.screen->h, SDL_SWSURFACE );
                }
                /* перерисовать карту */
                draw_map = 1;
                /* установить конфигурацию */
                config.width = action->w;
                config.height = action->h;
                config.fullscreen = flags & SDL_FULLSCREEN;
            }
            break;
        case ACTION_SET_SPOT_MASK:  //установка спот маски
            map_set_spot_mask();
            map_set_fog( F_SPOT );
            break;
        case ACTION_DRAW_MAP:   //прорисовка карты
            draw_map = 1;
            break;
        case ACTION_DEPLOY: //расстановка войск
            action->unit->delay = 0;
            action->unit->fresh_deploy = 0;
            action->unit->x = action->x;
            action->unit->y = action->y;
            list_transfer( avail_units, units, action->unit );
            if ( cur_ctrl == PLAYER_CTRL_CPU ) /* для игрока-человека он уже вставлен */
                map_insert_unit( action->unit );
            scen_prep_unit( action->unit, SCEN_PREP_UNIT_DEPLOY );
            break;
        case ACTION_EMBARK_SEA: //высадка на сушу из морского транспорта
            if ( map_check_unit_embark( action->unit, action->x, action->y, EMBARK_SEA, 0 ) ) {
                map_embark_unit( action->unit, action->x, action->y, EMBARK_SEA, &enemy_spotted );
                if ( enemy_spotted ) engine_clear_backup();
                if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                    engine_select_unit( action->unit );
            }
            break;
        case ACTION_DEBARK_SEA: //погрузка в морской транспорт
            if ( map_check_unit_debark( action->unit, action->x, action->y, EMBARK_SEA, 0 ) ) {
                map_debark_unit( action->unit, action->x, action->y, EMBARK_SEA, &enemy_spotted );
                if ( enemy_spotted ) engine_clear_backup();
                if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                    engine_select_unit( action->unit );
                /* ПРОВЕРИТЬ, ЗАВЕРШЕН СЦЕНАРИЙ (путем захвата последнего флага) */
                if ( scen_check_result( 0 ) )  {
                    engine_finish_scenario();
                    break;
                }
            }
            break;
        case ACTION_MERGE:  //объединение юнита
            if ( unit_check_merge( action->unit, action->target ) ) {
                unit_merge( action->unit, action->target );
                engine_remove_unit( action->target );
                map_get_vis_units();
                if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                    engine_select_unit( action->unit );
            }
            break;
        case ACTION_SPLIT:  //разделение юнита
            if ( map_check_unit_split(action->unit,action->str,action->x,action->y,action->target) )
            {
                if (action->target)
                {
                    /* сделать обратное слияние */
                    int old_str = action->unit->str;
                    action->unit->str = action->str;
                    unit_merge(action->target,action->unit);
                    action->unit->str = old_str-action->str;
                    unit_set_as_used(action->unit);
                    unit_update_bar(action->unit);
                }
                else
                {
                    /* добавить новый юнит */
                    int dummy;
                    Unit *newUnit = unit_duplicate( action->unit, 1 );
                    newUnit->str = action->str;
                    newUnit->x = action->x; newUnit->y = action->y;
                    newUnit->terrain = terrain_type_find( map[newUnit->x][newUnit->y].terrain_id[0] );
                    action->unit->str -= action->str;
                    list_add(units,newUnit); map_insert_unit(newUnit);
                    unit_set_as_used(action->unit); unit_set_as_used(newUnit);
                    unit_update_bar(action->unit); unit_update_bar(newUnit);
                    map_update_spot_mask(newUnit,&dummy); map_set_fog(F_SPOT);
                }
            }
            break;
        case ACTION_REPLACE:    //пополнение юнита
            if ( unit_check_replacements( action->unit,REPLACEMENTS ) ) {
                unit_replace( action->unit,REPLACEMENTS );
                if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                    engine_select_unit( action->unit );
            }
            break;
        case ACTION_ELITE_REPLACE:  //элитное пополнение юнита
            if ( unit_check_replacements( action->unit,ELITE_REPLACEMENTS ) ) {
                unit_replace( action->unit,ELITE_REPLACEMENTS );
                if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                    engine_select_unit( action->unit );
            }
            break;
        case ACTION_DISBAND:    //расформировать юнита
            engine_remove_unit( action->unit );
            map_get_vis_units();
            if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                engine_select_unit(0);
            break;
        case ACTION_EMBARK_AIR: //погрузить в транспортный самолет
            if ( map_check_unit_embark( action->unit, action->x, action->y, EMBARK_AIR, 0 ) ) {
                map_embark_unit( action->unit, action->x, action->y, EMBARK_AIR, &enemy_spotted );
                if ( enemy_spotted ) engine_clear_backup();
                if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                    engine_select_unit( action->unit );
            }
            break;
        case ACTION_DEBARK_AIR: //выгрузить из транспортного самолета
            if ( map_check_unit_debark( action->unit, action->x, action->y, EMBARK_AIR, 0 ) ) {
                if (action->id==1) /* нормальная посадка */
                    map_debark_unit( action->unit, action->x, action->y, EMBARK_AIR, &enemy_spotted );
                else
                    map_debark_unit( action->unit, action->x, action->y, DROP_BY_PARACHUTE, &enemy_spotted );
                if ( enemy_spotted ) engine_clear_backup();
                if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                    engine_select_unit( action->unit );
                /* ПРОВЕРИТЬ, ЗАВЕРШЕН СЦЕНАРИЙ (путем захвата последнего флага) */
                if ( scen_check_result( 0 ) )  {
                    engine_finish_scenario();
                    break;
                }
            }
            break;
        case ACTION_SUPPLY: //пополнение топливом и боеприпасами
            if ( unit_check_supply( action->unit, UNIT_SUPPLY_ANYTHING, 0, 0 ) )
                unit_supply( action->unit, UNIT_SUPPLY_ALL );
            break;
        case ACTION_END_TURN:   //конец хода
            engine_end_turn();
            if (!end_scen)
                engine_begin_turn( 0, 0 );
            break;
        case ACTION_MOVE:   //передвижение юнита
            cur_unit = action->unit;
            move_unit = action->unit;
            if ( move_unit->cur_mov == 0 ) {
                fprintf( stderr, tr("%s has no move points remaining\n"), move_unit->name );
                break;
            }
            dest_x = action->x;
            dest_y = action->y;
            status = STATUS_MOVE;
            phase = PHASE_INIT_MOVE;
            engine_clear_danger_mask();
            if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                image_hide( gui->cursors, 1 );
            break;
        case ACTION_ATTACK: //атака юнита
            if ( !unit_check_attack( action->unit, action->target, UNIT_ACTIVE_ATTACK ) ) {
                fprintf( stderr, tr("'%s' (%i,%i) can not attack '%s' (%i,%i)\n"),
                         action->unit->name, action->unit->x, action->unit->y,
                         action->target->name, action->target->x, action->target->y );
                break;
            }
            if ( !mask[action->target->x][action->target->y].spot ) {
                fprintf( stderr, tr("'%s' may not attack unit '%s' (not visible)\n"), action->unit->name, action->target->name );
                break;
            }
            cur_unit = action->unit;
            cur_target = action->target;
            unit_get_df_units( cur_unit, cur_target, units, df_units );
            if ( engine_get_next_combatants() ) {
                status = STATUS_ATTACK;
                phase = PHASE_INIT_ATK;
                engine_clear_danger_mask();
                if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                    image_hide( gui->cursors, 1 );
            }
            break;
    }
    free( action );
}

/*
====================================================================
Обновите движок, если действие в настоящее время обрабатывается.
====================================================================
*/
static void engine_update( int ms )
{
    int cx, cy;
    int old_atk_str, old_def_str;
    int old_atk_suppr, old_def_suppr;
    int old_atk_turn_suppr, old_def_turn_suppr;
    int reset = 0;
    int broken_up = 0;
    int was_final_fight = 0;
    int type = UNIT_STAYS, dx, dy;
    int start_x, start_y, end_x, end_y;
    float len;
    int i;
    int enemy_spotted = 0;
    int surrender = 0;
    /* проверить статус и фазу */
    switch ( status ) {
        case STATUS_MOVE:
            switch ( phase ) {
                case PHASE_INIT_MOVE:
                    /* получить маску движения */
                    map_get_unit_move_mask( move_unit );
                    /* проверьте, находится ли плитка в пределах досягаемости */
                    if ( mask[dest_x][dest_y].in_range == 0 ) {
                        fprintf( stderr, tr("%i,%i out of reach for '%s'\n"), dest_x, dest_y, move_unit->name );
                        phase = PHASE_END_MOVE;
                        break;
                    }
                    if ( mask[dest_x][dest_y].blocked ) {
                        fprintf( stderr, tr("%i,%i is blocked ('%s' wants to move there)\n"), dest_x, dest_y, move_unit->name );
                        phase = PHASE_END_MOVE;
                        break;
                    }
                    way = map_get_unit_way_points( move_unit, dest_x, dest_y, &way_length, &surp_unit );
                    if ( way == 0 ) {
                        fprintf( stderr, tr("There is no way for unit '%s' to move to %i,%i\n"),
                                 move_unit->name, dest_x, dest_y );
                        phase = PHASE_END_MOVE;
                        break;
                    }
                    /* убрать влияние юнита */
                    if ( !player_is_ally( move_unit->player, cur_player ) )
                        map_remove_unit_infl( move_unit );
                    /* сделайте резервную копию отряда, но только если это не убегающий отряд! */
                    if ( fleeing_unit )
                        fleeing_unit = 0;
                    else
                        engine_backup_move( move_unit, dest_x, dest_y );
                    /* если наземному транспортеру необходимо установить юнит */
                    if ( mask[dest_x][dest_y].mount ) unit_mount( move_unit );
                    /* начать с первой точки пути */
                    way_pos = 0;
                    /* юниты используются */
                    move_unit->unused = 0;
                    /* артиллерия теряет способность атаковать */
                    if ( unit_has_flag( move_unit->sel_prop, "attack_first" ) )
                        move_unit->cur_atk_count = 0;
                    /* уменьшить движущиеся точки */
/*                    if ( ( move_unit->sel_prop->flags & RECON ) && surp_unit == 0 )
                        move_unit->cur_mov = mask[dest_x][dest_y].in_range - 1;
                    else*/
                        move_unit->cur_mov = 0;
                    if ( move_unit->cur_mov < 0 )
                        move_unit->cur_mov = 0;
                    /* нет окопов */
                    move_unit->entr = 0;
                    /* создать образ */
                    if ( !blind_cpu_turn ) {
                        move_image = image_create( create_surf( move_unit->sel_prop->icon->w,
                                                                move_unit->sel_prop->icon->h, SDL_SWSURFACE ),
                                                   move_unit->sel_prop->icon_w, move_unit->sel_prop->icon_h,
                                                   sdl.screen, 0, 0 );
                        image_set_region( move_image, move_unit->icon_offset, 0,
                                          move_unit->sel_prop->icon_w, move_unit->sel_prop->icon_h );
                        SDL_FillRect( move_image->img, 0, move_unit->sel_prop->icon->format->colorkey );
                        SDL_SetColorKey( move_image->img, SDL_SRCCOLORKEY, move_unit->sel_prop->icon->format->colorkey );
                        FULL_DEST( move_image->img );
                        FULL_SOURCE( move_unit->sel_prop->icon );
                        blit_surf();
                        if ( mask[move_unit->x][move_unit->y].fog )
                            image_hide( move_image, 1 );
                    }
                    /* удалить юнит с карты */
                    map_remove_unit( move_unit );
                    if ( !blind_cpu_turn ) {
                        engine_get_screen_pos( move_unit->x, move_unit->y, &start_x, &start_y );
                        start_x += ( ( hex_w - move_unit->sel_prop->icon_w ) >> 1 );
                        start_y += ( ( hex_h - move_unit->sel_prop->icon_h ) >> 1 );
                        image_move( move_image, start_x, start_y );
                        draw_map = 1;
                    }
                    /* анимированные */
                    phase = PHASE_START_SINGLE_MOVE;
                    /* играть звук */
#ifdef WITH_SOUND
                    if ( !mask[move_unit->x][move_unit->y].fog )
                        wav_play( move_unit->sel_prop->wav_move );
#endif
                    /* поскольку он движется, он больше не считается охранником */
                    move_unit->is_guarding = 0;
                    break;
                case PHASE_START_SINGLE_MOVE:
                    /* получить следующую точку старта */
                    if ( blind_cpu_turn ) {
                        way_pos = way_length - 1;
                        /* юнит быстрого перемещения */
                        for ( i = 1; i < way_length; i++ ) {
                            move_unit->x = way[i].x; move_unit->y = way[i].y;
                            map_update_spot_mask( move_unit, &enemy_spotted );
                        }
                    }
                    else
                        if ( !modify_fog ) {
                            i = way_pos;
                            while ( i + 1 < way_length && mask[way[i].x][way[i].y].fog && mask[way[i + 1].x][way[i + 1].y].fog ) {
                                i++;
                                /* юнит быстрого перемещения */
                                move_unit->x = way[i].x; move_unit->y = way[i].y;
                                map_update_spot_mask( move_unit, &enemy_spotted );
                            }
                            way_pos = i;
                        }
                    /* сфокусировать текущую точку пути */
                    if ( way_pos < way_length - 1 )
                        if ( !blind_cpu_turn && ( MAP_CHECK_VIS( way[way_pos].x, way[way_pos].y ) || MAP_CHECK_VIS( way[way_pos + 1].x, way[way_pos + 1].y ) ) ) {
                            if ( engine_focus( way[way_pos].x, way[way_pos].y, 1 ) ) {
                                engine_get_screen_pos( way[way_pos].x, way[way_pos].y, &start_x, &start_y );
                                start_x += ( ( hex_w - move_unit->sel_prop->icon_w ) >> 1 );
                                start_y += ( ( hex_h - move_unit->sel_prop->icon_h ) >> 1 );
                                image_move( move_image, start_x, start_y );
                            }
                        }
                    /* юниты смотрят направление */
                    if ( way_pos + 1 < way_length )
                        unit_adjust_orient( move_unit, way[way_pos + 1].x, way[way_pos + 1].y );
                    if ( !blind_cpu_turn )
                        image_set_region( move_image, move_unit->icon_offset, 0,
                                          move_unit->sel_prop->icon_w, move_unit->sel_prop->icon_h );
                    /* позиция юнита */
                    move_unit->x = way[way_pos].x; move_unit->y = way[way_pos].y;
                    /* обновление обнаружения */
                    map_update_spot_mask( move_unit, &enemy_spotted );
                    if ( modify_fog ) map_set_fog( F_SPOT );
                    /*FIXME если вы заметили врага, нельзя отменять ход
					if ( enemy_spotted ) {
                        engine_clear_backup();
                    }*/
                    /* определить следующий шаг */
                    if ( way_pos == way_length - 1 )
                        phase = PHASE_CHECK_LAST_MOVE;
                    else {
                        /* анимированные? */
                        if ( MAP_CHECK_VIS( way[way_pos].x, way[way_pos].y ) || MAP_CHECK_VIS( way[way_pos + 1].x, way[way_pos + 1].y ) ) {
                            engine_get_screen_pos( way[way_pos].x, way[way_pos].y, &start_x, &start_y );
                            start_x += ( ( hex_w - move_unit->sel_prop->icon_w ) >> 1 );
                            start_y += ( ( hex_h - move_unit->sel_prop->icon_h ) >> 1 );
                            engine_get_screen_pos( way[way_pos + 1].x, way[way_pos + 1].y, &end_x, &end_y );
                            end_x += ( ( hex_w - move_unit->sel_prop->icon_w ) >> 1 );
                            end_y += ( ( hex_h - move_unit->sel_prop->icon_h ) >> 1 );
                            if ( unit_has_flag( move_unit->sel_prop, "flying" ) )
                            {
                                start_y -= 10;
                                end_y -= 10;
                            }
                            unit_vector.x = start_x; unit_vector.y = start_y;
                            move_vector.x = end_x - start_x; move_vector.y = end_y - start_y;
                            len = sqrt( move_vector.x * move_vector.x + move_vector.y * move_vector.y );
                            move_vector.x /= len; move_vector.y /= len;
                            image_move( move_image, (int)unit_vector.x, (int)unit_vector.y );
                            set_delay( &move_time, ((int)( len / move_vel ))/config.anim_speed );
                        }
                        else
                            set_delay( &move_time, 0 );
                        phase = PHASE_RUN_SINGLE_MOVE;
                        image_hide( move_image, 0 );
                    }
                    break;
                case PHASE_RUN_SINGLE_MOVE:
                    if ( timed_out( &move_time, ms ) ) {
                        /* следующая точка пути */
                        way_pos++;
                        /* следующее движение */
                        phase = PHASE_START_SINGLE_MOVE;
                    }
                    else {
                        unit_vector.x += move_vector.x * move_vel * ms;
                        unit_vector.y += move_vector.y * move_vel * ms;
                        image_move( move_image, (int)unit_vector.x, (int)unit_vector.y );
                    }
                    break;
                case PHASE_CHECK_LAST_MOVE:
                    /* вставить юнит */
                    map_insert_unit( move_unit );
                    /* флаг захвата, если он есть */
                    /* ПРИМЕЧАНИЕ: делайте это только для ИИ. Для игрока-человека это будет
                     * выполняется после отмены выбора текущего устройства, чтобы он напоминал
                     * оригинальное поведение Panzer General
                     */
                    if ( cur_ctrl == PLAYER_CTRL_CPU ) {
                        if ( engine_capture_flag( move_unit ) ) {
                            /* ПРОВЕРЬТЕ, ЗАКОНЧЕН ЛИ СЦЕНАРИЙ */
                            if ( scen_check_result( 0 ) )  {
                                engine_finish_scenario();
                                break;
                            }
                        }
                    }
                    /* добавить влияние */
                    if ( !player_is_ally( move_unit->player, cur_player ) )
                        map_add_unit_infl( move_unit );
                    /* обновите список видимых юнитов */
                    map_get_vis_units();
                    map_set_vis_infl_mask();
                    /* следующий этап */
                    phase = PHASE_END_MOVE;
                    break;
                case PHASE_END_MOVE:
                    /* затухающий звук */
#ifdef WITH_SOUND
                    audio_fade_out( 0, 500 ); /* перемещение звукового канала */
#endif
                    /* уменьшите топливо для шестигранных плиток way_pos движения */
                    if ( unit_check_fuel_usage( move_unit ) && config.supply ) {
                        move_unit->cur_fuel -= unit_calc_fuel_usage( move_unit, way[way_pos].cost );
                        if ( move_unit->cur_fuel < 0 )
                            move_unit->cur_fuel = 0;
                    }
                    /* очистить изображение буфера перемещения */
                    if ( !blind_cpu_turn )
                        image_delete( &move_image );
                    /* запуск неожиданного контакта */
                    if ( surp_unit ) {
                        cur_unit = move_unit;
                        cur_target = surp_unit;
                        surp_contact = 1;
                        surp_unit = 0;
                        if ( engine_get_next_combatants() ) {
                            status = STATUS_ATTACK;
                            phase = PHASE_INIT_ATK;
                            if ( !blind_cpu_turn ) {
                                image_hide( gui->cursors, 1 );
                                draw_map = 1;
                            }
                        }
                        break;
                    }
                    /* выберите подразделение -- cur_unit может отличаться от move_unit! */
                    if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                        engine_select_unit( cur_unit );
                    /* статус */
                    engine_set_status( STATUS_NONE );
                    phase = PHASE_NONE;
                    /* разрешить ввод нового человека / процессора */
                    if ( !blind_cpu_turn ) {
                        if ( cur_ctrl == PLAYER_CTRL_HUMAN ) {
                            gui_set_cursor( CURSOR_STD );
                            image_hide( gui->cursors, 0 );
                            old_mx = old_my = -1;
                        }
                        draw_map = 1;
                    }
                    break;
            }
            break;
        case STATUS_ATTACK:
            switch ( phase ) {
                case PHASE_INIT_ATK:
#ifdef DEBUG_ATTACK
                    printf( "\n" );
#endif
                    label_write( gui->label_left, gui->font_std, "" );
                    label_write( gui->label_center, gui->font_std, "" );
                    label_write( gui->label_right, gui->font_std, "" );
                    if ( !blind_cpu_turn ) {
                        if ( MAP_CHECK_VIS( cur_atk->x, cur_atk->y ) ) {
                            /*показать крест атакующего */
                            engine_focus( cur_atk->x, cur_atk->y, 1 );
                            engine_get_screen_pos( cur_atk->x, cur_atk->y, &cx, &cy );
                            anim_move( terrain_icons->cross, cx, cy );
                            anim_play( terrain_icons->cross, 0 );
                        }
                        phase = PHASE_SHOW_ATK_CROSS;
                    }
                    else
                        phase = PHASE_COMBAT;
                    /* оба отряда в бою больше не просто охраняют */
                    cur_atk->is_guarding = 0;
                    cur_def->is_guarding = 0;
                    break;
                case PHASE_SHOW_ATK_CROSS:
                    if ( !terrain_icons->cross->playing ) {
                        if ( MAP_CHECK_VIS( cur_def->x, cur_def->y ) ) {
                            /* показать крест защитника */
                            engine_focus( cur_def->x, cur_def->y, 1 );
                            engine_get_screen_pos( cur_def->x, cur_def->y, &cx, &cy );
                            anim_move( terrain_icons->cross, cx, cy );
                            anim_play( terrain_icons->cross, 0 );
                        }
                        phase = PHASE_SHOW_DEF_CROSS;
                    }
                    break;
                case PHASE_SHOW_DEF_CROSS:
                    if ( !terrain_icons->cross->playing )
                        phase = PHASE_COMBAT;
                    break;
                case PHASE_COMBAT:
                    /* резервное копирование старой силы, чтобы увидеть, кому нужно и взрыв */
                    old_atk_str = cur_atk->str; old_def_str = cur_def->str;
                    /* резервное копирование старого подавления для расчета дельты */
                    old_atk_suppr = cur_atk->suppr; old_def_suppr = cur_def->suppr;
                    old_atk_turn_suppr = cur_atk->turn_suppr;
                    old_def_turn_suppr = cur_def->turn_suppr;
                    /* получить урон */
                    if ( surp_contact )
                        atk_result = unit_surprise_attack( cur_atk, cur_def );
                    else {
                        if ( df_units->count > 0 )
                            atk_result = unit_normal_attack( cur_atk, cur_def, UNIT_DEFENSIVE_ATTACK );
                        else
                            atk_result = unit_normal_attack( cur_atk, cur_def, UNIT_ACTIVE_ATTACK );
                    }
                    /* вычислить дельты */
                    atk_damage_delta = old_atk_str - cur_atk->str;
		            def_damage_delta = old_def_str - cur_def->str;
                    atk_suppr_delta = cur_atk->suppr - old_atk_suppr;
		            def_suppr_delta = cur_def->suppr - old_def_suppr;
                    atk_suppr_delta += cur_atk->turn_suppr - old_atk_turn_suppr;
                    def_suppr_delta += cur_def->turn_suppr - old_def_turn_suppr;
                    if ( blind_cpu_turn )
                        phase = PHASE_CHECK_RESULT;
                    else {
                        /* если усиленная защита добавит паузу */
                        if ( atk_result & AR_RUGGED_DEFENSE ) {
                            phase = PHASE_RUGGED_DEF;
                            if ( unit_has_flag( cur_def->sel_prop, "flying" ) )
                                label_write( gui->label_center, gui->font_error, tr("Out Of The Sun!") );
                            else
                                if ( unit_has_flag( cur_def->sel_prop, "swimming" ) )
                                    label_write( gui->label_center, gui->font_error, tr("Surprise Contact!") );
                                else
                                    label_write( gui->label_center, gui->font_error, tr("Rugged Defense!") );
                            reset_delay( &msg_delay );
                        }
                        else if (atk_result & AR_EVADED) {
                            /* если подлодка уклонилась, дайте информацию */
                            label_write( gui->label_center, gui->font_error, tr("Submarine evades!") );
                            reset_delay( &msg_delay );
                            phase = PHASE_EVASION;
                        }
                        else
                            phase = PHASE_PREP_EXPLOSIONS;
                    }
                    break;
                case PHASE_EVASION:
                    if ( timed_out( &msg_delay, ms ) ) {
                        phase = PHASE_PREP_EXPLOSIONS;
                    }
                    break;
                case PHASE_RUGGED_DEF:
                    if ( timed_out( &msg_delay, ms ) ) {
                        phase = PHASE_PREP_EXPLOSIONS;
                    }
                    break;
                case PHASE_PREP_EXPLOSIONS:
                    engine_focus( cur_def->x, cur_def->y, 1 );
                    if (defFire)
                        gui_show_actual_losses( cur_def, cur_atk, def_suppr_delta, def_damage_delta,
                                                atk_suppr_delta, atk_damage_delta );
                    else
                    {
                        if ( unit_has_flag( cur_atk->sel_prop, "turn_suppr" ) )
                            gui_show_actual_losses( cur_atk, cur_def, atk_suppr_delta, atk_damage_delta,
                                                    def_suppr_delta, def_damage_delta );
                        else
                            gui_show_actual_losses( cur_atk, cur_def, 0, atk_damage_delta,
                                                    0, def_damage_delta );
                    }
                    /* взрыва атакующего */
                    if ( atk_damage_delta ) {
                        engine_get_screen_pos( cur_atk->x, cur_atk->y, &cx, &cy );
			            if (!cur_atk->str) map_remove_unit( cur_atk );
                        map_draw_tile( gui->map_frame, cur_atk->x, cur_atk->y, cx, cy, !air_mode, 0 );
                        anim_move( terrain_icons->expl1, cx, cy );
                        anim_play( terrain_icons->expl1, 0 );
                    }
                    /* защитник взрыв */
                    if ( def_damage_delta ) {
                        engine_get_screen_pos( cur_def->x, cur_def->y, &cx, &cy );
                        if (!cur_def->str) map_remove_unit( cur_def );
			            map_draw_tile( gui->map_frame, cur_def->x, cur_def->y, cx, cy, !air_mode, 0 );
                        anim_move( terrain_icons->expl2, cx, cy );
                        anim_play( terrain_icons->expl2, 0 );
                    }
                    phase = PHASE_SHOW_EXPLOSIONS;
                    /* играть звук */
#ifdef WITH_SOUND
                    if ( def_damage_delta || atk_damage_delta )
                        wav_play( terrain_icons->wav_expl );
#endif
                    break;
                case PHASE_SHOW_EXPLOSIONS:
		            if ( !terrain_icons->expl1->playing && !terrain_icons->expl2->playing ) {
                        phase = PHASE_FIGHT_MSG;
                        reset_delay( &msg_delay );
			            anim_hide( terrain_icons->expl1, 1 );
			            anim_hide( terrain_icons->expl2, 1 );
		            }
                    break;
		        case PHASE_FIGHT_MSG:
                    if ( timed_out( &msg_delay, ms ) ) {
                        phase = PHASE_CHECK_RESULT;
                    }
                    break;
                case PHASE_CHECK_RESULT:
                    surp_contact = 0;
                    /* проверить результат атаки */
                    if ( atk_result & AR_UNIT_KILLED ) {
                        scen_inc_casualties_for_unit( cur_atk );
                        engine_remove_unit( cur_atk );
                        cur_atk = 0;
                    }
                    if ( atk_result & AR_TARGET_KILLED ) {
                        scen_inc_casualties_for_unit( cur_def );
                        engine_remove_unit( cur_def );
                        cur_def = 0;
                    }
                    /* ПРОВЕРИТЬ, ЗАВЕРШЕН СЦЕНАРИЙ ИЗ-ЗА UNITS_KILLED ИЛИ UNITS_SAVED */
                    if ( scen_check_result( 0 ) )  {
                        engine_finish_scenario();
                        break;
                    }
                    reset = 1;
                    if ( df_units->count > 0 ) {
                        if ( atk_result & AR_TARGET_SUPPRESSED || atk_result & AR_TARGET_KILLED ) {
                            list_clear( df_units );
                            if ( atk_result & AR_TARGET_KILLED )
                                cur_unit = 0;
                            else {
                                /* подавленный блок теряет свои действия */
                                cur_unit->cur_mov = 0;
                                cur_unit->cur_atk_count = 0;
                                cur_unit->unused = 0;
                                broken_up = 1;
                            }
                        }
                        else {
                            reset = 0;
                            list_delete_pos( df_units, 0 );
                        }
                    }
                    else
                        was_final_fight = 1;
                    if ( !reset ) {
                        /* продолжать бои */
                        if ( engine_get_next_combatants() ) {
                            status = STATUS_ATTACK;
                            phase = PHASE_INIT_ATK;
                        }
                        else
                            fprintf( stderr, tr("Deadlock! No remaining combatants but supposed to continue fighting? How is this supposed to work????\n") );
                    }
                    else {
                        /* четкое подавление оборонительного огня */
                        if ( cur_atk ) {
                            cur_atk->suppr = 0;
                            cur_atk->unused = 0;
                        }
                        if ( cur_def )
                            cur_def->suppr = 0;
                        /* если это был последний бой между выбранным отрядом и выбранной целью
                           проверьте, полностью ли подавлено одно из этих подразделений, и сдастся
                           или убегает */
                        if ( was_final_fight ) {
                            engine_clear_backup(); /* нельзя отменить после атаки */
                            if ( cur_atk != 0 && cur_def != 0 ) {
                                if ( atk_result & AR_UNIT_ATTACK_BROKEN_UP ) {
                                    /* отряд разогнал атаку */
                                    broken_up = 1;
                                }
                                else
                                /* полное подавление может привести только к бегству или
                                   сдаться, если: оба подразделения являются наземными / военно-морскими подразделениями в
                                   ближний бой: юнит, вызывающий супр, должен
                                   иметь диапазон 0 (рукопашный бой)
                                   inf -> fort (форт может сдаться)
                                   fort -> adjacent inf (Инф не убежит) */
                                if (!unit_has_flag( cur_atk->sel_prop, "flying" )&&
                                    !unit_has_flag( cur_def->sel_prop, "flying" ))
                                {
                                    if ( ( atk_result & AR_UNIT_SUPPRESSED &&
                                         !(atk_result & AR_TARGET_SUPPRESSED) &&
                                         cur_def->sel_prop->rng==0 ) || ( cur_player->force_retreat && cur_atk->player != cur_player ) ) {
                                        /* cur_unit подавлен */
                                        engine_handle_suppr( cur_atk, &type, &dx, &dy );
                                        if ( type == UNIT_FLEES ) {
                                            status = STATUS_MOVE;
                                            phase = PHASE_INIT_MOVE;
                                            move_unit = cur_atk;
                                            fleeing_unit = 1;
                                            dest_x = dx; dest_y = dy;
                                            break;
                                        }
                                        else
                                            if ( type == UNIT_SURRENDERS ) {
                                                surrender = 1;
                                                surrender_unit = cur_atk;
                                            }
                                    }
                                    else
                                        if ( ( atk_result & AR_TARGET_SUPPRESSED &&
                                             !(atk_result & AR_UNIT_SUPPRESSED) &&
                                             cur_atk->sel_prop->rng==0 ) || ( cur_player->force_retreat && cur_def->player != cur_player  ) ) {
                                            /* cur_target подавлен */
                                            engine_handle_suppr( cur_def, &type, &dx, &dy );
                                            if ( type == UNIT_FLEES ) {
                                                status = STATUS_MOVE;
                                                phase = PHASE_INIT_MOVE;
                                                move_unit = cur_def;
                                                fleeing_unit = 1;
                                                dest_x = dx; dest_y = dy;
                                                break;
                                            }
                                            else
                                                if ( type == UNIT_SURRENDERS ) {
                                                    surrender = 1;
                                                    surrender_unit = cur_def;
                                                }
                                        }
                                }
                            }
                            /* очистить указатели */
                            if ( cur_atk == 0 ) cur_unit = 0;
                            if ( cur_def == 0 ) cur_target = 0;
                        }
                        if ( broken_up ) {
                            phase = PHASE_BROKEN_UP_MSG;
                            label_write( gui->label_center, gui->font_error, tr("Attack Broken Up!") );
                            reset_delay( &msg_delay );
                            break;
                        }
                        if ( surrender ) {
                            const char *msg = unit_has_flag( surrender_unit->sel_prop, "swimming" ) ? tr("Ship is scuttled!") : tr("Surrenders!");
                            phase = PHASE_SURRENDER_MSG;
                            label_write( gui->label_center, gui->font_error, msg );
                            reset_delay( &msg_delay );
                            break;
                        }
                        phase = PHASE_END_COMBAT;
                    }
                    break;
                case PHASE_SURRENDER_MSG:
                    if ( timed_out( &msg_delay, ms ) ) {
                        if ( surrender_unit == cur_atk ) {
                            cur_unit = 0;
                            cur_atk = 0;
                        }
                        if ( surrender_unit == cur_def ) {
                            cur_def = 0;
                            cur_target = 0;
                        }
                        engine_remove_unit( surrender_unit );
                        phase = PHASE_END_COMBAT;
                    }
                    break;
                case PHASE_BROKEN_UP_MSG:
                    if ( timed_out( &msg_delay, ms ) ) {
                        phase = PHASE_END_COMBAT;
                    }
                    break;
                case PHASE_END_COMBAT:
#ifdef WITH_SOUND
                    audio_fade_out( 2, 1500 ); /* звуковой канал взрыва */
#endif
                    /* стоит одно очко топлива для атакующего */
                    if ( cur_unit && unit_check_fuel_usage( cur_unit ) && cur_unit->cur_fuel > 0 )
                        cur_unit->cur_fuel--;
                    /* обновить список видимых единиц */
                    map_get_vis_units();
                    map_set_vis_infl_mask();
                    /* повторно выбрать юнит */
                    if ( cur_ctrl == PLAYER_CTRL_HUMAN )
                        engine_select_unit( cur_unit );
                    /* статус */
                    engine_set_status( STATUS_NONE );
                    phase = PHASE_NONE;
                    /* разрешить ввод нового человека / процессора */
                    if ( !blind_cpu_turn ) {
                        if ( cur_ctrl == PLAYER_CTRL_HUMAN ) {
                            image_hide( gui->cursors, 0 );
                            gui_set_cursor( CURSOR_STD );
                        }
                        draw_map = 1;
                    }
                    break;
            }
            break;
    }
    /* обновить анимацию */
    if ( status == STATUS_ATTACK ) {
        anim_update( terrain_icons->cross, ms );
        anim_update( terrain_icons->expl1, ms );
        anim_update( terrain_icons->expl2, ms );
    }
    if ( status == STATUS_STRAT_MAP ) {
        if ( timed_out( &blink_delay, ms ) )
            strat_map_blink();
    }
    if ( status == STATUS_CAMP_BRIEFING ) {
        gui_draw_message_pane(camp_pane);
    }
    /* обновление графического интерфейса */
    gui_update( ms );
    if ( edit_update( gui->edit, ms ) ) {
#ifdef WITH_SOUND
        wav_play( gui->wav_edit );
#endif
    }
}

/*
====================================================================
Основной игровой цикл.
Если перезапуск не требуется, настройка изменяется и выполняется повторная установка.
установлено True.
====================================================================
*/
static void engine_main_loop( int *reinit )
{
    int ms;
    if ( status == STATUS_TITLE && !term_game ) {   //показать фон и нет команды завершить игру
        engine_draw_bkgnd();    //нарисуйте обои и фон
        engine_show_game_menu(10,40);   //показать игровое меню
        refresh_screen( 0, 0, 0, 0 );   //обновить прямоугольник (0,0,0,0) -> полноэкранный режим
    }
    else if ( status == STATUS_CAMP_BRIEFING )  //запустить информационный диалог кампании
        ;
    else
        engine_begin_turn( cur_player, setup.type == SETUP_LOAD_GAME /* пропустить подготовку юнита тогда */ ); //ход юнита
    gui_get_bkgnds();   //скрыть / нарисовать с экрана/на экран
    *reinit = 0;
    reset_timer();  //сбросить таймер
    while( !end_scen && !term_game ) {  //если сценарий не завершен и игра не прервана
        engine_begin_frame();   //скрыть все анимированные окна верхнего уровня
        /* проверить события ввода / процессора и поставить в очередь действий */
        engine_check_events(reinit);
        /* обрабатывать действия в очереди */
        engine_handle_next_action( reinit );
        /* получить время */
        ms = get_time();
        /* отложить следующий шаг прокрутки */
        if ( scroll_vert || scroll_hori ) {
            if ( scroll_time > ms ) {
                set_delay( &scroll_delay, scroll_time );
                scroll_block = 1;
                scroll_vert = scroll_hori = SCROLL_NONE;
            }
            else
                set_delay( &scroll_delay, 0 );
        }
        if ( timed_out( &scroll_delay, ms ) )
            scroll_block = 0;
        /* Обновить картинку на экране */
        engine_update( ms );
        engine_end_frame();
        /* короткая задержка */
        SDL_Delay( 5 );
    }
    /* скрыть эти окна, чтобы начальный экран выглядел оригинально */
    frame_hide(gui->qinfo1, 1);
    frame_hide(gui->qinfo2, 1);
}

/*
====================================================================
Общественные
====================================================================
*/

/*
====================================================================
Выберите это устройство и при необходимости отмените выбор старого.
Снимите выделение, если в качестве юнита передается NULL.
====================================================================
*/
void engine_select_unit( Unit *unit )
{
    /* выбрать юнита */
    cur_unit = unit;
    engine_clear_danger_mask();
    if ( cur_unit == 0 ) {
        /* очистка вида */
        if ( modify_fog ) map_set_fog( F_SPOT );
        engine_clear_backup();
        return;
    }
    /* переключатель воздух / земля */
    if ( unit_has_flag( unit->sel_prop, "flying" ) )
        air_mode = 1;
    else
        air_mode = 0;
    /* получить партнеров по слиянию и установить маску merge_unit */
    map_get_merge_units( cur_unit, merge_units, &merge_unit_count );
    /* диапазон движения */
    map_get_unit_move_mask( unit );
    if ( modify_fog && unit->cur_mov > 0 ) {
        map_set_fog( F_IN_RANGE );
        mask[unit->x][unit->y].fog = 0;
    }
    else
        map_set_fog( F_SPOT );
    /* определить опасную зону для авиационных подразделений */
    if ( modify_fog && config.supply && unit->cur_mov
         && unit_has_flag( unit->sel_prop, "flying" ) && unit->sel_prop->fuel)
        has_danger_zone = map_get_danger_mask( unit );
    return;
}

/*
====================================================================
Если x, y отсутствуют на экране, отцентрируйте этот фрагмент карты и проверьте, не
копирование экрана возможно (но только если use_sc имеет значение True)
====================================================================
*/
int engine_focus( int x, int y, int use_sc )
{
    int new_x, new_y;
    if ( x <= map_x + 1 || y <= map_y + 1 || x >= map_x + map_sw - 1 - 2 || y >= map_y + map_sh - 1 - 2 ) {
        new_x = x - ( map_sw >> 1 );
        new_y = y - ( map_sh >> 1 );
        if ( new_x & 1 ) new_x++;
        if ( new_y & 1 ) new_y++;
        engine_goto_xy( new_x, new_y );
        if ( !use_sc ) sc_type = SC_NONE; /* нет копии экрана */
        return 1;
    }
    return 0;
}

/*
====================================================================
Создать движок (загрузить ресурсы, не измененные сценарием)
====================================================================
*/
int engine_create()
{
    gui_load( "Default" );
    return 1;
}
void engine_delete()
{
    engine_shutdown();
    scen_clear_setup();
    if (prev_scen_core_units)
	list_delete(prev_scen_core_units);
    if (prev_scen_fname)
	free(prev_scen_fname);
    gui_delete();
}

/*
====================================================================
Запустите движок, загрузив сценарий либо как сохраненную игру, либо
новый сценарий мировой 'setup'.
====================================================================
*/
int engine_init()
{
    int i, j;
    Player *player;
#ifdef USE_DL
    char path[256];
#endif
    end_scen = 0;
    /* очередь действий построить */
    actions_create();
    /* сценарий и кампания или название*/
    if ( setup.type == SETUP_RUN_TITLE ) {
        status = STATUS_TITLE;
        return 1;
    }
    if ( setup.type == SETUP_CAMP_BRIEFING ) {
        status = STATUS_CAMP_BRIEFING;
        return 1;
    }
    if ( setup.type == SETUP_LOAD_GAME ) {
        if ( !slot_load( gui->load_menu->current_name ) ) return 0;
        group_set_hidden( gui->unit_buttons, ID_REPLACEMENTS, !config.merge_replacements );
        group_set_hidden( gui->unit_buttons, ID_ELITE_REPLACEMENTS, !config.merge_replacements );
        group_set_hidden( gui->unit_buttons, ID_MERGE, config.merge_replacements );
        group_set_hidden( gui->unit_buttons, ID_SPLIT, config.merge_replacements );
    }
    else
        if ( setup.type == SETUP_INIT_CAMP ) {
            if ( !camp_load( setup.fname, setup.camp_entry_name ) ) return 0;
            camp_set_cur( setup.scen_state );
            if ( !camp_cur_scen ) return 0;
            setup.type = SETUP_CAMP_BRIEFING;

            /* новые кампании и так понятно, основной блок переноса списка если */
		if (prev_scen_core_units)
			list_clear(prev_scen_core_units);
            return 1;
        }
        else
        {
            if ( !scen_load( setup.fname ) ) return 0;  //загрузка сценария
            if ( setup.type == SETUP_INIT_SCEN ) {
                /* контроль игрока */
                list_reset( players );
                for ( i = 0; i < setup.player_count; i++ ) {
                    player = list_next( players );
                    player->ctrl = setup.ctrl[i];
                    free( player->ai_fname );
                    player->ai_fname = strdup( setup.modules[i] );
                }
            }
            /* выбрать первого игрока */
            cur_player = players_get_first();
        }
    /* сохранить текущие настройки для настройки */
    scen_set_setup();
    /* загрузить модули ИИ */
    list_reset( players );
    for ( i = 0; i < players->count; i++ ) {
        player = list_next( players );
        /* очистить обратные вызовы */
        player->ai_init = 0;
        player->ai_run = 0;
        player->ai_finalize = 0;
        if ( player->ctrl == PLAYER_CTRL_CPU ) {
#ifdef USE_DL
            if ( strcmp( "default", player->ai_fname ) ) {
                sprintf( path, "%s/ai_modules/%s", get_gamedir(), player->ai_fname );
                if ( ( player->ai_mod_handle = dlopen( path, RTLD_GLOBAL | RTLD_NOW ) ) == 0 )
                    fprintf( stderr, "%s\n", dlerror() );
                else {
                    if ( ( player->ai_init = dlsym( player->ai_mod_handle, "ai_init" ) ) == 0 )
                        fprintf( stderr, "%s\n", dlerror() );
                    if ( ( player->ai_run = dlsym( player->ai_mod_handle, "ai_run" ) ) == 0 )
                        fprintf( stderr, "%s\n", dlerror() );
                    if ( ( player->ai_finalize = dlsym( player->ai_mod_handle, "ai_finalize" ) ) == 0 )
                        fprintf( stderr, "%s\n", dlerror() );
                }
                if ( player->ai_init == 0 || player->ai_run == 0 || player->ai_finalize == 0 ) {
                    fprintf( stderr, tr("%s: AI module '%s' invalid. Use built-in AI.\n"), player->name, player->ai_fname );
                    /* используйте внутренний ИИ */
                    player->ai_init = ai_init;
                    player->ai_run = ai_run;
                    player->ai_finalize = ai_finalize;
                    if ( player->ai_mod_handle ) {
                        dlclose( player->ai_mod_handle );
                        player->ai_mod_handle = 0;
                    }
                }
            }
            else {
                player->ai_init = ai_init;
                player->ai_run = ai_run;
                player->ai_finalize = ai_finalize;
            }
#else
            player->ai_init = ai_init;
            player->ai_run = ai_run;
            player->ai_finalize = ai_finalize;
#endif
        }
    }
    /* юнит не выбран */
    cur_unit = cur_target = cur_atk = cur_def = move_unit = surp_unit = deploy_unit = surrender_unit = 0;
    df_units = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    /* двигатель */
    /* плитка маска */
    /*  1 = плитка карты напрямую попадает
        0 = сосед */
    hex_mask = calloc( hex_w * hex_h, sizeof ( int ) );
    for ( j = 0; j < hex_h; j++ )
        for ( i = 0; i < hex_w; i++ )
            if ( get_pixel( terrain_icons->fog, i, j ) )
                hex_mask[j * hex_w + i] = 1;
    /* буфер копирования экрана */
    sc_buffer = create_surf( sdl.screen->w, sdl.screen->h, SDL_SWSURFACE );
    sc_type = 0;
    /* геометрия карты */
    map_x = map_y = 0;
    map_sx = -hex_x_offset;
    map_sy = -hex_h;
    for ( i = map_sx, map_sw = 0; i < gui->map_frame->w; i += hex_x_offset )
        map_sw++;
    for ( j = map_sy, map_sh = 0; j < gui->map_frame->h; j += hex_h )
        map_sh++;
    /* сбросить задержку прокрутки */
    set_delay( &scroll_delay, 0 );
    scroll_block = 0;
    /* задержка сообщения */
    set_delay( &msg_delay, 1500/config.anim_speed );
    /* скрыть анимацию */
    anim_hide( terrain_icons->cross, 1 );
    anim_hide( terrain_icons->expl1, 1 );
    anim_hide( terrain_icons->expl2, 1 );
    /* список оставшихся юнитов развертывания */
    left_deploy_units = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    /* построить стратегическую карту */
    strat_map_create();
    /* очистить статус */
    status = STATUS_NONE;
    /* Погода */
    cur_weather = scen_get_weather();
    return 1;
}
void engine_shutdown()
{
    engine_set_status( STATUS_NONE );
    modify_fog = 0;
    cur_player = 0; cur_ctrl = 0;
    cur_unit = cur_target = cur_atk = cur_def = move_unit = surp_unit = deploy_unit = surrender_unit = 0;
    memset( merge_units, 0, sizeof( int ) * MAP_MERGE_UNIT_LIMIT );
    merge_unit_count = 0;
    engine_clear_backup();
    scroll_hori = scroll_vert = 0;
    last_button = 0;
    scen_delete();
    if ( df_units ) {
        list_delete( df_units );
        df_units = 0;
    }
    if ( hex_mask ) {
        free( hex_mask );
        hex_mask = 0;
    }
    if ( sc_buffer ) {
        SDL_FreeSurface( sc_buffer );
        sc_buffer = 0;
    }
    sc_type = SC_NONE;
    actions_delete();
    if ( way ) {
        free( way );
        way = 0;
        way_length = 0; way_pos = 0;
    }
    if ( left_deploy_units ) {
        list_delete( left_deploy_units );
        left_deploy_units = 0;
    }
    strat_map_delete();
}

/*
====================================================================
Запустите двигатель (запускается с титульного экрана)
====================================================================
*/
void engine_run()
{
    int reinit = 1;
    if (setup.type == SETUP_UNKNOWN) setup.type = SETUP_RUN_TITLE;
    while ( 1 ) {
        while ( reinit ) {
            reinit = 0;
            if(engine_init() == 0) {
              /* если инициализация двигателя не удалась */
              /* оставайтесь с титульным экраном */
              status = STATUS_TITLE;
              setup.type = SETUP_RUN_TITLE;
            }
            if ( turn == 0 && (camp_loaded != NO_CAMPAIGN) && setup.type == SETUP_CAMP_BRIEFING )
                engine_prep_camp_brief();
            engine_main_loop( &reinit );
            if (term_game) break;
            engine_shutdown();
        }
        if ( scen_done() ) {
            if ( camp_loaded != NO_CAMPAIGN ) {
                engine_store_debriefing(scen_get_result());
                /* определить следующий сценарий в кампании */
                if ( !camp_set_next( scen_get_result() ) )
                    break;
                if ( camp_cur_scen->nexts == 0 ) {
                    /* последнее сообщение */
                    setup.type = SETUP_CAMP_BRIEFING;
                    reinit = 1;
                }
                else if ( !camp_cur_scen->scen ) { /* параметры */
                    setup.type = SETUP_CAMP_BRIEFING;
                    reinit = 1;
                }
                else {
                    /* следующий сценарий */
                    sprintf( setup.fname, "%s", camp_cur_scen->scen );
                    setup.type = SETUP_CAMP_BRIEFING;
                    reinit = 1;
                }
            }
            else {
                setup.type = SETUP_RUN_TITLE;
                reinit = 1;
            }
        }
        else
            break;
        /* очистить результат перед следующим циклом (если есть) */
        scen_clear_result();
    }
}
