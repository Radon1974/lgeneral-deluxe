/***************************************************************************
                          config.h  -  description
                             -------------------
    begin                : Tue Feb 13 2001
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

#ifndef __CONFIG_H
#define __CONFIG_H

/* глупое сходство имен */
#ifdef HAVE_CONFIG_H
#  include <../config.h>
#endif

enum PurchaseOptions {
    NO_PURCHASE = 0,		/* 0 - можно покупать, 1 - отмена покупок */
    DELAYED_PURCHASE = 0,	/* 0 - без задержки ИИ покупает, 1 - задержка */
    INSTANT_PURCHASE = 1	/* 1 - чтобы юниты были доступны к растановке сразу после покупки, 2 - на следующий ход */
};
enum MergeReplacementsOptions {
    OPTION_MERGE = 0,
    OPTION_REPLACEMENTS
};
enum WeatherOptions {
    OPTION_WEATHER_OFF = 0,
    OPTION_WEATHER_PREDETERMINED = 1,
    OPTION_WEATHER_RANDOM = 2
};

/* настроить структуру */
typedef struct {
    /* каталог для сохранения конфигурации */
    char dir_name[512];
    char *mod_name;
    /* параметры gfx */
    int grid; /* шестигранная сетка */
    int tran; /* прозрачность */
    int show_bar; /* показать шкалу жизни и значки юнита */
    int width, height, fullscreen;
    int anim_speed; /* масштабировать анимацию по этому коэффициенту: 1 = нормальный, 2 = удвоенный, ... */
    /* варианты игры */
    int supply; /* юниты должны поставлять */
    int weather; /* погода имеет влияние? */
    int fog_of_war; /* угадай, что? */
    int show_cpu_turn;
    int deploy_turn; /* разрешить развертывание */
    int purchase; /* отключить предопределенные переопределения и разрешить покупку по престижу */
    int campaign_purchase; /* отключить предопределенные переопределения и разрешить покупку по престижу */
    int merge_replacements; /* включить слияние во время боя (0) или включить замены
                                                         по престижности (1) */
    int use_core_units; /* включить основную армию, купленную игроком, пройти кампанию */
    int ai_debug; /* уровень информации о движении ИИ */
    int all_equipment; /* переменная, используемая для включения / выключения покупки любого будущего чит-кода оборудования */
    int zones_of_control; /* переменная, используемая для включения / отключения зон контроля чит-кода */
    int turbo_units; /* переменная, используемая для включения / отключения чит-кода турбо-модулей */
    /* аудио материал */
    int sound_on;
    int sound_volume;
    int music_on;
    int music_volume;
} Config;

/* проверьте, существует ли каталог конфигурации; если нет, создайте его и установите config_dir */
void check_config_dir_name();

/* установить конфигурацию по умолчанию */
void reset_config();

/* загрузить конфигурацию */
void load_config( );

/* сохранить конфигурацию */
void save_config( );

#endif
