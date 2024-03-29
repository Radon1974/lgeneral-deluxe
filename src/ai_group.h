/***************************************************************************
                          ai_tools.h  -  description
                             -------------------
    begin                : Sun Jul 14 2002
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

#ifndef __AI_GROUP_H
#define __AI_GROUP_H

/*
====================================================================
������ ����� ��
====================================================================
*/
enum {
    GS_ART_FIRE = 0,
    GS_MOVE
};
typedef struct {
    List *units;
    int order; /* -2: �������� ������� ����� ����������
                  -1: �������� �������
                   0: ������� � �������
                   1: ������� �����
                   2: �������� ������� ����� ���������� */
    int x, y; /* �������, ������� �������� ������� ��������� ���� ������ */
    int state; /* �������, ��� ������� ���� */
    /* �������� ��� ���������������� ������ �������� */
    int ground_count, aircraft_count, bomber_count;
} AI_Group;

/*
====================================================================
������� / ������� ������
====================================================================
*/
AI_Group *ai_group_create( int order, int x, int y );
void ai_group_add_unit( AI_Group *group, Unit *unit );
void ai_group_delete_unit( AI_Group *group, Unit *unit );
void ai_group_delete( void *ptr );
/*
====================================================================
����������� � ���������� ����� � ������, ����� ��������� ������� ������ ��� �����������
��������� ��������. ���� ������ ��������� ����������, ������������ False.
====================================================================
*/
int ai_group_handle_next_unit( AI_Group *group );

/* HACK - ��� ������ ��������� � ai.c, �� ������ ���� �������� ����� ���
   �� �������� ���������� ��� � ai_groups.c */
enum {
    AI_FIND_DEPOT,
    AI_FIND_OWN_TOWN,
    AI_FIND_ENEMY_TOWN,
    AI_FIND_OWN_OBJ,
    AI_FIND_ENEMY_OBJ
};

#endif
