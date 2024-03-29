/***************************************************************************
                          unit.h  -  description
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

#ifndef __UNIT_H
#define __UNIT_H

#include "unit_lib.h"
#include "nation.h"
#include "player.h"
#include "terrain.h"

//#define DEBUG_ATTACK

/*
====================================================================
���� ������� ��� �����.
====================================================================
*/
enum {
    EMBARK_NONE = 0,
    EMBARK_GROUND,
    EMBARK_SEA,
    EMBARK_AIR,
    DROP_BY_PARACHUTE
};

/*
====================================================================
����������� ������� ������ (������������ ��� ������������� ������).
====================================================================
*/
enum {
    UNIT_ORIENT_RIGHT = 0,
    UNIT_ORIENT_LEFT,
    UNIT_ORIENT_UP = 0,
    UNIT_ORIENT_RIGHT_UP,
    UNIT_ORIENT_RIGHT_DOWN,
    UNIT_ORIENT_DOWN,
    UNIT_ORIENT_LEFT_DOWN,
    UNIT_ORIENT_LEFT_UP
};

/* ������� ����� ����� */
/* ������������ �� �����, � ��������� ����������� � ������� �������� �������� */
enum {
    BAR_WIDTH = 31,
    BAR_HEIGHT = 4,
    BAR_TILE_WIDTH = 3,
    BAR_TILE_HEIGHT = 4
};

/* �������� � ��������������� ����� */
enum{
    AUXILIARY = 0,
    CORE,
    STARTING_CORE
};

/*
====================================================================
����������� ����.
�� ��������� ����������� ������, ������� �������� �������� ����� ���������� �
������� �� ��������� �� ����������, � ������ �����
(��� ��������, ��� ������������� ������, ���, ������, ����� �������� �����������
� �� ������).
����� ����������, ���� �� � ����� ���������� unit-> trsp_prop-> id
���������. (0 = ��� ������������, 1 = ���� �����������)
====================================================================
*/
typedef struct _Unit {
    Unit_Lib_Entry prop;        /* �������� */
    Unit_Lib_Entry trsp_prop;   /* �������� ������������� */
    Unit_Lib_Entry land_trsp_prop; /* ��������� �������� ������������� (������������ �� ����� ������� ���������) */
    Unit_Lib_Entry *sel_prop;   /* ��������� ���������: ���� prop, ���� trsp_prop */
    struct _Unit *backup;       /* ������������ ��� ���������� ����������� ��������� �������� ������, ������� ����� �������� ���������� (��������, ��������� ������) */
    char name[24];              /* �������� ������������� */
    Player *player;             /* �������������� ����� */
    Nation *nation;             /* ������������ ���� ����������� */
    Terrain_Type *terrain;      /* ���������, �� ������� � ��������� ����� ��������� ���� */
    int x, y;                   /* ��������� �� ����� */
    int str;                    /* ���� */
    int max_str;                /* ������� ������������ ���� ����� �������� */
    int cur_str_repl;           /* ������� ������� � ��������� ����� ����� �������� */
    int repl_exp_cost;          /* ������ ����� ��� ������ */
    int repl_prestige_cost;     /* ������� ��� ������ � ������ ������ */
    int entr;                   /* ���� */
    int exp;                    /* ���� */
    int exp_level;              /* ������� ����� ����������� �� ����� */
    int delay;                  /* �������� ���� �� ����, ��� ���� ������ ���������
                                   ��� ������������ */
    int embark;                 /* ��� ������� */
    int orient;                 /* ������� ���������� */
    int icon_offset;            /* �������� � �������� sel_prop->icon */
    int icon_tiny_offset;       /* �������� � sep_prop->tiny_icon */
    int supply_level;           /* � ���������; ������� ������� �������� �������� ��������� */
    int cur_fuel;               /* ������� ������� */
    int cur_ammo;               /* ������� ���������� */
    int cur_mov;                /* �������� ��������� ���� ��� */
    int cur_mov_saved;          /* ����������� ��������� ����� �������� ��� ���-���� "�����-�����" */
    int cur_atk_count;          /* ���������� ���� � ���� ��� */
    int unused;                 /* ����� ���� �� ������������ �������� � ���� ��� */
    int damage_bar_width;       /* ������� ������ ������ ����� � map->life_icons */
    int damage_bar_offset;      /* �������� � map->damage_icons */
    int suppr;                  /* ���������� ����� �� ��������� ���
                                   (������� �����������, ������� ����� ���) */
    int turn_suppr;             /* ���������� ����� �� ���� ������
                                   ������� ����������� ��������������, ������ ������� ������� (<20) ���
                                   ������ �������� (<2) */
    int is_guarding;            /* �� ��������� � ������ ��� ���� �� ���������� ����� */
    int killed;                 /* 1: ������� � ����� � ������� ���� ����
                                   2: ������� ������ � �����
                                   3: ������� ������ */
    int fresh_deploy;           /* ���� ��� ������ �� ��� ������������� ���� ���������� � ���� ��������
                                   � ���� �� �� ������������ �� ����� ���� �� ��������� � ���
                                    �� ��� */
    char tag[32];               /* ���� ��� ���������� �� ���� ����������� � ������ ������ �������
                                    �������������� ��������� ������ units_killed()
                                   � units_saved() */
    int core;                   /* 1: �������� ����
                                   2: ��������������� ���� */
    int eval_score;             /* �� 0 �� 1000, ����������� �� ��������� ����� */
    /* �� ������ */
    int target_score;           /* ����� ���� ����� �� ���������� ��� �������� ���������������
                                   � ���������� ����������� ���� �� ����� ������������� �� ������ ���� */
    char star[6][20];           /* �������� ���������� � ������ �������� */
} __attribute((packed)) Unit;

/*
====================================================================
������, ����������� ��� �������� ������� ����� ����������
������� ����: ������ ��������������� ����� ����� ����� �����, �� 32 ������ ���� ������
���������� � �������� ��������� ... ��������� � ����� �������� ����������������!
====================================================================
*/
typedef struct {
	Unit_Lib_Entry prop;
	Unit_Lib_Entry trsp_prop;
	Unit_Lib_Entry land_trsp_prop;
	char prop_id[32];
	char trsp_prop_id[32];
	char land_trsp_prop_id[32];
	char name[24];
	char player_id[32];
	char nation_id[32];
	int str;
	int exp;
	char tag[32];
} transferredUnitProp;

/*
====================================================================
�������� ����, ������� ��������� ����� �� ��������� ������� ���������:
  �, �, ��. ���������, �����, ��������, ������, �����, �����.
��� ������� ����� ������������ ���������� �������� ��� �������� ��������� ���������
�� ����� ��������� ����� ����������.
====================================================================
*/
Unit *unit_create( Unit_Lib_Entry *prop, Unit_Lib_Entry *trsp_prop,
                   Unit_Lib_Entry *land_trsp_prop, Unit *base );

/*
====================================================================
������� �������. ��������� ��������� ��� void*, ����� ��������� ������������� � ��������
��������� ������ ��� ������.
====================================================================
*/
void unit_delete( void *ptr );

/*
====================================================================
����� ������� ����� ��������.
====================================================================
*/
void unit_set_generic_name( Unit *unit, int number, const char *stem );

/*
====================================================================
�������� ������ ���������� � ������������ � ��� �����������.
====================================================================
*/
void unit_adjust_icon( Unit *unit );

/*
====================================================================
������������� ���������� (� ������������� ������) ����������, ���� �� �������� � ����������� x, y.
====================================================================
*/
void unit_adjust_orient( Unit *unit, int x, int y );

/*
====================================================================
���������, ����� �� ������������� ���-�� ��������� (����������, �������, ��� ������), �
������� ����������, ������� ����� ���������.
====================================================================
*/
enum {
    UNIT_SUPPLY_AMMO = 1,
    UNIT_SUPPLY_FUEL,
    UNIT_SUPPLY_ANYTHING,
    UNIT_SUPPLY_ALL
};
int unit_check_supply( Unit *unit, int type, int *missing_ammo, int *missing_fuel );

/*
====================================================================
������� ������ ������������� ���������� �������/�����������/� ����, � �������
������� True, ���� ������� ��������� ���� ����������.
====================================================================
*/
int unit_supply_intern( Unit *unit, int type );
int unit_supply( Unit *unit, int type );

/*
====================================================================
���������, ���������� �� ���� ������� � ����� ������� ��������� (��������� ��� ���).
====================================================================
*/
int unit_check_fuel_usage( Unit *unit );

/*
====================================================================
�������� ���� � ��������� ������� �����.
������� True, ���� ������� �������.
====================================================================
*/
int unit_add_exp( Unit *unit, int exp );

/*
====================================================================
���������� / �������������� ���� �� �������� ������������.
====================================================================
*/
void unit_mount( Unit *unit );
void unit_unmount( Unit *unit );

/*
====================================================================
���������, ��������� �� ����� ������ ���� � �����. ��� ������ �� ��������
������������ ������.
====================================================================
*/
int unit_is_close( Unit *unit, Unit *target );

/*
====================================================================
���������, ����� �� ���� ������� ��������� (�����, �������������� �������) ���
��������� ����� (����� �� �������������� ����, ������ �����) ����.
====================================================================
*/
enum {
    UNIT_ACTIVE_ATTACK = 0,
    UNIT_PASSIVE_ATTACK,
    UNIT_DEFENSIVE_ATTACK
};
int unit_check_attack( Unit *unit, Unit *target, int type );

/*
====================================================================
��������� ���� / ����������, ������� �������� ���� ��� ����� �����
����. ������� �������� �� ����� ��������. ���� ����������� �������� "��������"
����� �������, ����� ��� �������������� �������.
====================================================================
*/
void unit_get_damage( Unit *aggressor, Unit *unit, Unit *target,
                      int type,
                      int real, int rugged_def,
                      int *damage, int *suppr );

/*
====================================================================
��������� ��������� ��� (��� �������� ��������������� ����) �� ���������� ����������.
unit_surprise_attack () ������������ ����� � ����������� �����
(��������, Out Of The Sun)
���� � ������� ��� �������� �������� ������ (�����������_����� �����
������ �������) 'rugged_def' ����������.
����������� �������� � ���������� ����.
unit_normal_attack () ��������� UNIT_ACTIVE_ATTACK ���
UNIT_DEFENSIVE_ATTACK ��� ���� � ����������� �� ����, ������������ �� ��� ����������
��� ������� �������.
====================================================================
*/
enum {
    AR_NONE                  = 0,            /* ������ ���������� */
    AR_UNIT_ATTACK_BROKEN_UP = ( 1L << 1),   /* ���� �� ���� ������ � ����� ������ */
    AR_UNIT_SUPPRESSED       = ( 1L << 2),   /* ���� ���, �� ��������� �������� */
    AR_TARGET_SUPPRESSED     = ( 1L << 3),   /* ��� */
    AR_UNIT_KILLED           = ( 1L << 4),   /* �������� ���� 0 */
    AR_TARGET_KILLED         = ( 1L << 5),   /* ��� */
    AR_RUGGED_DEFENSE        = ( 1L << 6),   /* ���� ������� �������� ������� */
    AR_EVADED                = ( 1L << 7),   /* ���� ��������� */
};
int unit_normal_attack( Unit *unit, Unit *target, int type );
int unit_surprise_attack( Unit *unit, Unit *target );

/*
====================================================================
�������� ������ ������ ���� ������ ����, ������� ��������� (!)
�������� �������� ��������� � ��� ��������� �����������.
������� ������������� ����, ���������� ������ �������.
��������� ��������� ����� ������ �� ������������� ���������, �����
����� ����� (���� ������������� ������).
====================================================================
*/
void unit_get_expected_losses( Unit *unit, Unit *target, int *unit_damage, int *target_damage );

/*
====================================================================
��� ������� ��������� ����� �� ������� ����������� �����.
��� ���� �������������� ����� ����� ��������� ���������
����� ������ ����� ����� �����. ��� ������� ���������� � 'df_units'
(������� ����� �� ���������)
====================================================================
*/
void unit_get_df_units( Unit *unit, Unit *target, List *units, List *df_units );

/*
====================================================================
���������, ����� �� �������� ����������. ��� ������ ���
ELITE_REPLACEMENTS.
====================================================================
*/
enum {
    REPLACEMENTS = 0,
    ELITE_REPLACEMENTS
};
int unit_check_replacements( Unit *unit, int type );

/*
====================================================================
��������� ������� ���� �� ���� ������. ���
������ ��� ELITE_REPLACEMENTS.
====================================================================
*/
int unit_get_replacement_strength( Unit *unit, int type );

/*
====================================================================
�������� ������ ��� �����. ��� ������ ���
ELITE_REPLACEMENTS.
====================================================================
*/
void unit_replace( Unit *unit, int type );

/*
====================================================================
���������, ��������� �� ���� ���� ������ ��������� ���� � ������.
====================================================================
*/
int unit_check_merge( Unit *unit, Unit *source );

/*
====================================================================
�������� ������������ ����, ������� ����� ����� ����, �������� ���
������� ���������. � ������ ������ ���� �� ����� 3 ���������� ���.
====================================================================
*/
int unit_get_split_strength( Unit *unit );

/*
====================================================================
���������� ��� ��� �����: ���� - ��� ����� ������, � �������� ������ ����
��������� � ����� � ������ ����� ������ ���� �������.
====================================================================
*/
void unit_merge( Unit *unit, Unit *source );

/*
====================================================================
������� True, ���� ���� ���������� �������� �����������.
====================================================================
*/
int unit_check_ground_trsp( Unit *unit );

/*
====================================================================
��������� ����������� �� ��������� ���������� ����������� (���������� �����)
====================================================================
*/
void unit_backup( Unit *unit );
void unit_restore( Unit *unit );

/*
====================================================================
���������, ����� �� ���� ���������� �������� ������
====================================================================
*/
int unit_check_rugged_def( Unit *unit, Unit *target );

/*
====================================================================
��������� ���� �������� ������.
====================================================================
*/
int unit_get_rugged_def_chance( Unit *unit, Unit *target );

/*
====================================================================
����������� ���������� ��������������� �������. "���������" - ��� ������� ��������� �������, ������� ������ ����
���������� �� ���� �������� �� ���������. ��������� ����� ���������������� �� ���� �������������.
====================================================================
*/
int unit_calc_fuel_usage( Unit *unit, int cost );

/*
====================================================================
�������� ������ �����.
====================================================================
*/
void unit_update_bar( Unit *unit );

/*
====================================================================
��������� ��� ��������.
====================================================================
*/
void unit_set_as_used( Unit *unit );

/*
====================================================================
���������� ������, ������ ����� ��� (��� ����������) ��� ���
����� ��� (��� ���������� � ����������� ��������).
====================================================================
*/
Unit *unit_duplicate( Unit *unit, int generate_new_name );

/* ��� �������� � ���������� �������� ���������� ������������� ��������. */
transferredUnitProp *unit_create_transfer_props( Unit *unit );
/*
====================================================================
���������, ���� �� � ����� ���� ����������� ��� �������.
====================================================================
*/
int unit_low_fuel( Unit *unit );
int unit_low_ammo( Unit *unit );

/*
====================================================================
���������, ����� �� ������������� ���� ��� �������������.
====================================================================
*/
int unit_supports_deploy( Unit *unit );

/*
====================================================================
���������� �������� ����� (����������� ����� � ���������� ��������).
====================================================================
*/
void unit_reset_attributes( Unit *unit );

#endif
