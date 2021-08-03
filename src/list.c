/***************************************************************************
                          list.c  -  description
                             -------------------
    begin                : Sun Sep 2 2001
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

#include <stdlib.h>

#include "list.h"

/*
====================================================================
�������� ����� ������
  auto_delete:  ��������� ������ ��������� ������ ��� �������� ������
  callback:     ����������� ���� �������� ����� ��� ������������ ������ �� ������, �������
                ��� ��������� ������.
Return Value: ��������� ������
====================================================================
*/
List *list_create( int auto_delete, void (*callback)(void*) )
{
    List *list = calloc( 1, sizeof( List ) );
    list->head.next = &list->tail;
    list->head.prev = &list->head;
    list->tail.next = &list->tail;
    list->tail.prev = &list->head;
    list->auto_delete = auto_delete;
    list->callback = callback;
    list->cur_entry = &list->head;
    return list;
}
/*
====================================================================
������� ������ � ������.
====================================================================
*/
void list_delete( List *list )
{
    list_clear( list );
    free( list );
}
/*
====================================================================
������� ��� ������, �� ��������� ������. �������� current_entry ��
��������� ������.
====================================================================
*/
void list_clear( List *list )
{
    while( !list_empty( list ) ) list_delete_pos( list, 0 );
}
/*
====================================================================
�������� ����� ������� � ������ ���������.
Return Value: True � ������ ������, ����� false.
====================================================================
*/
int list_insert( List *list, void *item, int pos )
{
    int         i;
    List_Entry    *cur = &list->head;
    List_Entry    *new_entry = 0;

    /* ���������, �������� �� ������� */
    if ( pos < 0 || pos > list->count ) return 0;
    if ( item == 0 ) return 0;
    /* ������� � ���������� ������ */
    for (i = 0; i < pos; i++) cur = cur->next;
    /* �������� � ����������� ����� ������ */
    new_entry = calloc( 1, sizeof( List_Entry ) );
    new_entry->item = item;
    new_entry->next = cur->next;
    new_entry->prev = cur;
    cur->next->prev = new_entry;
    cur->next = new_entry;
    list->count++;
    return 1;
}
/*
====================================================================
�������� ����� ������� � ����� ������.
====================================================================
*/
int list_add( List *list, void *item )
{
    List_Entry    *new_entry = 0;
    /* ���������, �������� �� ������� */
    if ( item == 0 ) return 0;
    /* �������� � ����������� ����� ������ */
    new_entry = calloc( 1, sizeof( List_Entry ) );
    new_entry->item = item;
    new_entry->next = &list->tail;
    new_entry->prev = list->tail.prev;
    list->tail.prev->next = new_entry;
    list->tail.prev = new_entry;
    list->count++;
    return 1;
}
/*
====================================================================
������� ������� � �������. ���� ��� ���� ������� ������, ��������
current_entry �� ����������� ����������� ���������.
Return Value: True � ������ ������, ����� false.
====================================================================
*/
int list_delete_pos( List *list, int pos )
{
    int i;
    List_Entry  *cur = &list->head;

    /* ���������, �������� �� �������� */
    if ( list_empty( list ) ) return 0;
    if ( pos < 0 || pos >= list->count ) return 0;
    /* ������� � ���������� ������ */
    for ( i = 0; i <= pos; i++ ) cur = cur->next;
    /* �������� ����� */
    cur->next->prev = cur->prev;
    cur->prev->next = cur->next;
    /* ���������� �������� */
    list->count--;
    /* ��� current_entry */
    if ( list->cur_entry == cur )
        list->cur_entry = list->cur_entry->prev;
    /* ��������� ������ */
    if ( list->auto_delete ) {
        if ( list->callback )
            (list->callback)( cur->item );
        else
            free( cur->item );
    }
    free( cur );
    return 1;
}
/*
====================================================================
������� �������, ���� �� ���� � ������. ���� ��� ���� ������� ���������� ������
current_entry �� �������������� ���������� ���������.
Return Value: ����� � ������ ������ ����� ����.
====================================================================
*/
int list_delete_item( List *list, void *item )
{
    return list->cur_entry->item == item
        ? list_delete_current( list )
        : list_delete_pos( list, list_check( list, item ) );
}
/*
====================================================================
������� ������.
Return Value: ����� � ������ ������ ����� ����.
====================================================================
*/
int list_delete_entry( List *list, List_Entry *entry )
{
    /* ������� ��������? */
    if ( entry == 0 ) return 0;
    if ( list->count == 0 ) return 0;
    if ( entry == &list->head || entry == &list->tail ) return 0;
    /* �������������� ����� � ������� */
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    list->count--;
    /* ��������� current_entry */
    if ( list->cur_entry == entry )
        list->cur_entry = list->cur_entry->prev;
    /* ��������� ������ */
    if ( list->auto_delete ) {
        if ( list->callback )
            (list->callback)( entry->item );
        else
            free( entry->item );
    }
    free( entry );
    return 1;
}
/*
====================================================================
�������� ������� �� �������, ���� � ������.
Return Value: ��������� �� �������, ���� ���������, ��� �� ������.
====================================================================
*/
void* list_get( List *list, int pos )
{
    int i;
    List_Entry *cur = &list->head;

    if ( pos < 0 || pos >= list->count ) return 0;
    for ( i = 0; i <= pos; i++ ) cur = cur->next;
    return cur->item;
}
/*
====================================================================
���������, ���� �� ������� � ������.
Return Value: ������� ������ ����� -1.
====================================================================
*/
int list_check( List *list, void *item )
{
    int pos = -1;
    List_Entry *cur = list->head.next;
    while ( cur != &list->tail ) {
        pos++;
        if ( cur->item == item ) return pos;
        cur = cur->next;
    }
    return -1;
}
/*
====================================================================
������� ������ �������, ���������� � ������, � ���������� ��� ���� current_entry
����.
Return Value: ��������� ��������, ���� ������ ��� ���� ������� ���������.
====================================================================
*/
void* list_first( List *list )
{
    list->cur_entry = list->head.next;
    return list->head.next->item;
}
/*
====================================================================
������� ��������� �������, ����������� � ������, � ���������� current_entry � ���
������.
Return Value: ��������� ��������, ���� ������ ��� ���� ������� ���������.
====================================================================
*/
void* list_last( List *list )
{
    list->cur_entry = list->tail.prev;
    return list->tail.prev->item;
}
/*
====================================================================
������� �������� � current_entry.
Return Value: ��������� ��������, ���� ������ ��� ���� ������� ���������.
====================================================================
*/
void* list_current( List *list )
{
    return list->cur_entry->item;
}
/*
====================================================================
������� �������� � ������.
====================================================================
*/
ListIterator list_iterator( List *list )
{
  ListIterator it = { list, &list->head };
  return it;
}
/*
====================================================================
�������� current_entry � ������ ������.
====================================================================
*/
void list_reset( List *list )
{
    list->cur_entry = &list->head;
}
/*
====================================================================
�������� ��������� ������� � �������� current_entry (��������, ���� ����� ���������)
������������ ��������: ��������� ��������, ���� ������� ������ �������� Null (���� ����� ������).
====================================================================
*/
void* list_next( List *list )
{
    list->cur_entry = list->cur_entry->next;
    if ( list->cur_entry == &list->tail ) list_reset( list );
    return list->cur_entry->item;
}
/*
====================================================================
�������� ���������� ������� � �������� current_entry.
������������ ��������: ��������� ��������, ���� ������� ������ �������� Null (���� ��������� ������).
====================================================================
*/
void* list_prev( List *list )
{
    list->cur_entry = list->cur_entry->prev;
    return list->cur_entry->item;
}
/*
====================================================================
������� ������� ������, ���� ��� �� �������� ������� ��� �������. ��� ������
, ���������� ��������� ������������ ������� list_next/prev().
������������ ��������: True, ���� ��� ���� ���������� ��������� ������.
====================================================================
*/
int list_delete_current( List *list )
{
	if ( list->cur_entry == 0 || list->cur_entry == &list->head || list->cur_entry == &list->tail ) return 0;
	list_delete_entry( list, list->cur_entry );
	return 1;
}
/*
====================================================================
���������, ���� �� ������.
Return Value: True, ���� ������� ������ ����� 0, ����� False.
====================================================================
*/
int list_empty( List *list )
{
    return list->count == 0;
}
/*
====================================================================
������������ ������, ���������� ���������� �������.
Return Value: ������, ���� ������ �������, ����� ����.
====================================================================
*/
List_Entry *list_entry( List *list, void *item )
{
    List_Entry *entry = list->head.next;
    while ( entry != &list->tail ) {
        if ( entry->item == item ) return entry;
        entry = entry->next;
    }
    return 0;
}
/*
====================================================================
���������� ������ �� ������ ������ � ������, ������ ��
"���������" � ������� � "dest", ����� �������, ���� �������� �� ��������
��������, �� �� ����� list_add( dest, item ).
====================================================================
*/
void list_transfer( List *source, List *dest, void *item )
{
    int old_auto_flag;
    /* �������� � ����� ���������� */
    list_add( dest, item );
    /* ��������� ��������� ����������� � dest ��� ��������� ������ ������
    ������ ������ ���� ������� � ��������� */
    old_auto_flag = source->auto_delete;
    source->auto_delete = LIST_NO_AUTO_DELETE;
    list_delete_item( source, item );
    source->auto_delete = old_auto_flag;
}
/*
====================================================================
Deqeue ������ ������ ������. (������� �� ������� ������������ auto_delete)
====================================================================
*/
void *list_dequeue( List *list )
{
    void *item;
    if ( list->count > 0 ) {
        item = list->head.next->item;
        list_delete_pos( list, 0 );
        return item;
    }
    else
        return 0;
}

/*
====================================================================
���������� ������� ������� � ����������� ��������.
====================================================================
*/
void *list_iterator_next( ListIterator *it )
{
    return (it->cur_entry = it->cur_entry->next)->item;
}

/*
====================================================================
���������, ���� �� ��� �������� � ������.
====================================================================
*/
int list_iterator_has_next( ListIterator *it )
{
    return it->cur_entry != &it->l->tail;
}
