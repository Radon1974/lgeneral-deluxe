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
Создайте новый список
  auto_delete:  Свободная память указателя данных при удалении записи
  callback:     Используйте этот обратный вызов для освобождения памяти от данных, включая
                сам указатель данных.
Return Value: Указатель списка
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
Удалите все записи, но сохраните список. Сбросьте current_entry на
указатель головы.
====================================================================
*/
void list_clear( List *list )
{
    while( !list_empty( list ) ) list_delete_pos( list, 0 );
}
/*
====================================================================
Вставьте новый элемент в нужное положение.
Return Value: True в случае успеха, иначе false.
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
    /* создание и закрепление новой записи */
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
Добавьте новый элемент в конце списка.
====================================================================
*/
int list_add( List *list, void *item )
{
    List_Entry    *new_entry = 0;
    /* проверьте, возможна ли вставка */
    if ( item == 0 ) return 0;
    /* создание и закрепление новой записи */
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
Удалить элемент в позиции. Если это была текущая запись, обновите
current_entry до допустимого предыдущего указателя.
Return Value: True в случае успеха, иначе false.
====================================================================
*/
int list_delete_pos( List *list, int pos )
{
    int i;
    List_Entry  *cur = &list->head;

    /* проверьте, возможно ли удаление */
    if ( list_empty( list ) ) return 0;
    if ( pos < 0 || pos >= list->count ) return 0;
    /* перейти к правильной записи */
    for ( i = 0; i <= pos; i++ ) cur = cur->next;
    /* изменить якоря */
    cur->next->prev = cur->prev;
    cur->prev->next = cur->next;
    /* уменьшение счетчика */
    list->count--;
    /* чек current_entry */
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
Return Value: Верно в случае успеха иначе Ложь.
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
Удалить запись.
Return Value: Верно в случае успеха иначе Ложь.
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
Return Value: Указатель на элемент, если обнаружен, еще не указан.
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
Return Value: Позиция пункта иначе -1.
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
Вернуть первый элемент, хранящийся в списке, и установить для него current_entry
вход.
Return Value: Указатель элемента, если найден еще один нулевой указатель.
====================================================================
*/
void* list_first( List *list )
{
    list->cur_entry = list->head.next;
    return list->head.next->item;
}
/*
====================================================================
Верните последний элемент, сохраненный в списке, и установите current_entry в эту
запись.
Return Value: Указатель элемента, если найден еще один нулевой указатель.
====================================================================
*/
void* list_last( List *list )
{
    list->cur_entry = list->tail.prev;
    return list->tail.prev->item;
}
/*
====================================================================
Возврат элемента в current_entry.
Return Value: Указатель элемента, если найден еще один нулевой указатель.
====================================================================
*/
void* list_current( List *list )
{
    return list->cur_entry->item;
}
/*
====================================================================
Верните итератор в список.
====================================================================
*/
ListIterator list_iterator( List *list )
{
  ListIterator it = { list, &list->head };
  return it;
}
/*
====================================================================
Сбросьте current_entry в начало списка.
====================================================================
*/
void list_reset( List *list )
{
    list->cur_entry = &list->head;
}
/*
====================================================================
Получить следующий элемент и обновить current_entry (сбросить, если хвост достигнут)
Возвращаемое значение: указатель элемента, если найдено другое значение Null (если хвост списка).
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
Возвращаемое значение: указатель элемента, если найдено другое значение Null (если заголовок списка).
====================================================================
*/
void* list_prev( List *list )
{
    list->cur_entry = list->cur_entry->prev;
    return list->cur_entry->item;
}
/*
====================================================================
Удалите текущую запись, если она не является хвостом или головой. Это запись
, содержащая последний возвращенный элемент list_next/prev().
Возвращаемое значение: True, если это была допустимая удаляемая запись.
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
Return Value: True, если счетчик списка равен 0, иначе False.
====================================================================
*/
int list_empty( List *list )
{
    return list->count == 0;
}
/*
====================================================================
Возвращаемая запись, содержащая переданный элемент.
Return Value: Истина, если запись найдена, иначе Ложь.
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
Перенесите запись из одного списка в другой, удалив из
"источника" и добавив в "dest", таким образом, если источник не содержит
элемента, то он равен list_add( dest, item ).
====================================================================
*/
void list_transfer( List *source, List *dest, void *item )
{
    int old_auto_flag;
    /* добавить в пункт назначения */
    list_add( dest, item );
    /* поскольку указатель добавляется в dest без изменений только пустой
    запись должна быть удалена в источнике */
    old_auto_flag = source->auto_delete;
    source->auto_delete = LIST_NO_AUTO_DELETE;
    list_delete_item( source, item );
    source->auto_delete = old_auto_flag;
}
/*
====================================================================
Deqeue первая запись списка. (поэтому не следует использовать auto_delete)
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
Возвращает текущий элемент и увеличивает итератор.
====================================================================
*/
void *list_iterator_next( ListIterator *it )
{
    return (it->cur_entry = it->cur_entry->next)->item;
}

/*
====================================================================
Проверяет, есть ли еще элементы в списке.
====================================================================
*/
int list_iterator_has_next( ListIterator *it )
{
    return it->cur_entry != &it->l->tail;
}
