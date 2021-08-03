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
Создать новый список
  auto_delete: Освободить память указателя данных при удалении записи
  обратный вызов: используйте этот обратный вызов, чтобы освободить память данных, включая
                сам указатель данных.
Возвращаемое значение: указатель на список
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
Удалить список и записи.
====================================================================
*/
void list_delete( List *list )
{
    list_clear( list );
    free( list );
}
/*
====================================================================
Удалить все записи, но сохранить список. Сбросить current_entry в голову
указатель.
====================================================================
*/
void list_clear( List *list )
{
    while( !list_empty( list ) ) list_delete_pos( list, 0 );
}
/*
====================================================================
Вставить новый элемент в позицию.
Возвращаемое значение: True в случае успеха иначе False.
====================================================================
*/
int list_insert( List *list, void *item, int pos )
{
    int         i;
    List_Entry    *cur = &list->head;
    List_Entry    *new_entry = 0;

    /* проверьте, возможна ли вставка */
    if ( pos < 0 || pos > list->count ) return 0;
    if ( item == 0 ) return 0;
    /* перейти к предыдущей записи */
    for (i = 0; i < pos; i++) cur = cur->next;
    /* создать и привязать новую запись */
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
Добавить новый элемент в конец списка.
====================================================================
*/
int list_add( List *list, void *item )
{
    List_Entry    *new_entry = 0;
    /* проверьте, возможна ли вставка */
    if ( item == 0 ) return 0;
    /* создать и привязать новую запись */
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
Удалить элемент в позиции. Если это было текущее обновление записи
current_entry на действительный предыдущий указатель.
Возвращаемое значение: True в случае успеха иначе False.
====================================================================
*/
int list_delete_pos( List *list, int pos )
{
    int         i;
    List_Entry  *cur = &list->head;

    /* проверьте, возможно ли удаление */
    if ( list_empty( list ) ) return 0;
    if ( pos < 0 || pos >= list->count ) return 0;
    /* перейти к правильной записи */
    for ( i = 0; i <= pos; i++ ) cur = cur->next;
    /* изменить якоря */
    cur->next->prev = cur->prev;
    cur->prev->next = cur->next;
    /* уменьшить счетчик */
    list->count--;
    /* проверить current_entry */
    if ( list->cur_entry == cur )
        list->cur_entry = list->cur_entry->prev;
    /* свободная память */
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
Удалить элемент, если он есть в списке. Если это было текущее обновление записи
current_entry на действительный предыдущий указатель.
Возвращаемое значение: True в случае успеха иначе False.
====================================================================
*/
int list_delete_item( List *list, void *item )
{
    return list_delete_pos( list, list_check( list, item ) );
}
/*
====================================================================
Удалить запись.
Возвращаемое значение: True в случае успеха иначе False.
====================================================================
*/
int list_delete_entry( List *list, List_Entry *entry )
{
    /* удалить возможно? */
    if ( entry == 0 ) return 0;
    if ( list->count == 0 ) return 0;
    if ( entry == &list->head || entry == &list->tail ) return 0;
    /* отрегулировать якорь и счетчик */
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    list->count--;
    /* проверить current_entry */
    if ( list->cur_entry == entry )
        list->cur_entry = list->cur_entry->prev;
    /* свободная память */
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
Получить элемент из позиции, если в списке.
Возвращаемое значение: указатель на элемент, если он найден, другой указатель NULL.
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
Проверить, есть ли элемент в списке.
Возвращаемое значение: позиция элемента else -1.
====================================================================
*/
int list_check( List *list, void *item )
{
    int pos = -1;
    List_Entry *cur = list->head.next;
    while ( cur != &list->tail ) {
        pos++;
        if ( cur->item == item ) break;
        cur = cur->next;
    }
    return pos;
}
/*
====================================================================
Вернуть первый элемент, хранящийся в списке, и установить для него current_entry
вход.
Возвращаемое значение: указатель на элемент, если он найден, другой указатель NULL.
====================================================================
*/
void* list_first( List *list )
{
    list->cur_entry = list->head.next;
    return list->head.next->item;
}
/*
====================================================================
Вернуть последний элемент, хранящийся в списке, и установить для него current_entry
вход.
Возвращаемое значение: указатель на элемент, если он найден, другой указатель NULL.
====================================================================
*/
void* list_last( List *list )
{
    list->cur_entry = list->tail.prev;
    return list->tail.prev->item;
}
/*
====================================================================
Вернуть элемент в current_entry.
Возвращаемое значение: указатель на элемент, если он найден, другой указатель NULL.
====================================================================
*/
void* list_current( List *list )
{
    return list->cur_entry->item;
}
/*
====================================================================
Сбросить current_entry в начало списка.
====================================================================
*/
void list_reset( List *list )
{
    list->cur_entry = &list->head;
}
/*
====================================================================
Получить следующий элемент и обновить current_entry (сбросить, если достигнут хвост)
Возвращаемое значение: указатель элемента, если он найден, иначе Null (если конец списка).
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
Получить предыдущий элемент и обновить current_entry.
Возвращаемое значение: указатель элемента, если он найден, иначе Null (если заголовок списка).
====================================================================
*/
void* list_prev( List *list )
{
    list->cur_entry = list->cur_entry->prev;
    return list->cur_entry->item;
}
/*
====================================================================
Удалите текущую запись, если нет хвоста или головы. Это запись
который содержит последний элемент, возвращенный list_next / prev ().
Возвращаемое значение: Истина, если это была допустимая удаляемая запись.
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
Проверьте, пуст ли список.
Возвращаемое значение: Истина, если счетчик списка равен 0, иначе Ложь.
====================================================================
*/
int list_empty( List *list )
{
    return list->count == 0;
}
/*
====================================================================
Возвратная запись, содержащая переданный элемент.
Возвращаемое значение: Истина, если запись найдена, иначе Ложь.
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
Перенести запись из одного списка в другой, удалив из
'source' и добавив к 'dest' таким образом, если источник не содержит
элемент this эквивалентен list_add (dest, item).
====================================================================
*/
void list_transfer( List *source, List *dest, void *item )
{
    int old_auto_flag;
    /* добавить в пункт назначения */
    list_add( dest, item );
    /* поскольку указатель добавляется в dest без изменений, только пустой
    запись должна быть удалена в источнике */
    old_auto_flag = source->auto_delete;
    source->auto_delete = LIST_NO_AUTO_DELETE;
    list_delete_item( source, item );
    source->auto_delete = old_auto_flag;
}
/*
====================================================================
Deqeue первой записи списка. (поэтому не следует использовать auto_delete)
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
