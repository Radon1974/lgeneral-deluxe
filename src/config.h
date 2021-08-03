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

/* ������ �������� ���� */
#ifdef HAVE_CONFIG_H
#  include <../config.h>
#endif

enum PurchaseOptions {
    NO_PURCHASE = 0,		/* 0 - ����� ��������, 1 - ������ ������� */
    DELAYED_PURCHASE = 0,	/* 0 - ��� �������� �� ��������, 1 - �������� */
    INSTANT_PURCHASE = 1	/* 1 - ����� ����� ���� �������� � ���������� ����� ����� �������, 2 - �� ��������� ��� */
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

/* ��������� ��������� */
typedef struct {
    /* ������� ��� ���������� ������������ */
    char dir_name[512];
    char *mod_name;
    /* ��������� gfx */
    int grid; /* ������������ ����� */
    int tran; /* ������������ */
    int show_bar; /* �������� ����� ����� � ������ ����� */
    int width, height, fullscreen;
    int anim_speed; /* �������������� �������� �� ����� ������������: 1 = ����������, 2 = ���������, ... */
    /* �������� ���� */
    int supply; /* ����� ������ ���������� */
    int weather; /* ������ ����� �������? */
    int fog_of_war; /* ������, ���? */
    int show_cpu_turn;
    int deploy_turn; /* ��������� ������������� */
    int purchase; /* ��������� ���������������� ��������������� � ��������� ������� �� �������� */
    int campaign_purchase; /* ��������� ���������������� ��������������� � ��������� ������� �� �������� */
    int merge_replacements; /* �������� ������� �� ����� ��� (0) ��� �������� ������
                                                         �� ������������ (1) */
    int use_core_units; /* �������� �������� �����, ��������� �������, ������ �������� */
    int ai_debug; /* ������� ���������� � �������� �� */
    int all_equipment; /* ����������, ������������ ��� ��������� / ���������� ������� ������ �������� ���-���� ������������ */
    int zones_of_control; /* ����������, ������������ ��� ��������� / ���������� ��� �������� ���-���� */
    int turbo_units; /* ����������, ������������ ��� ��������� / ���������� ���-���� �����-������� */
    /* ����� �������� */
    int sound_on;
    int sound_volume;
    int music_on;
    int music_volume;
} Config;

/* ���������, ���������� �� ������� ������������; ���� ���, �������� ��� � ���������� config_dir */
void check_config_dir_name();

/* ���������� ������������ �� ��������� */
void reset_config();

/* ��������� ������������ */
void load_config( );

/* ��������� ������������ */
void save_config( );

#endif
