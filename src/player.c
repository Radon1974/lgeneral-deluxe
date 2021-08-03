/***************************************************************************
                          player.c  -  description
                             -------------------
    begin                : Fri Jan 19 2001
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

#ifdef USE_DL
  #include <dlfcn.h>
#endif
#include "lgeneral.h"
#include "list.h"
#include "nation.h"
#include "unit_lib.h"
#include "player.h"

/*
====================================================================
Материал игрока
====================================================================
*/
List *players = 0;
int cur_player_id = 0;
extern int deploy_turn;

/*
====================================================================
Локальные
====================================================================
*/

/*
====================================================================
Публичные
====================================================================
*/

/*
====================================================================
Добавить игрока в список игроков.
====================================================================
*/
void player_add( Player *player )
{
    if ( players == 0 )
        players = list_create( LIST_AUTO_DELETE, player_delete );
    list_add( players, player );
}

/*
====================================================================
Удалить запись игрока. Передайте как void * для совместимости с
list_delete
====================================================================
*/
void player_delete( void *ptr )
{
    Player *player = (Player*)ptr;
    if ( player == 0 ) return;
    if ( player->id ) free( player->id );
    if ( player->name ) free( player->name );
    if ( player->ai_fname ) free( player->ai_fname );
#ifdef USE_DL
    if ( player->ai_mod_handle )
        dlclose( player->ai_mod_handle );
#endif
    if ( player->nations ) free( player->nations );
    if ( player->allies ) list_delete( player->allies );
    if ( player->prestige_per_turn ) free( player->prestige_per_turn );
    free( player );
}

/*
====================================================================
Удалить всех игроков.
====================================================================
*/
void players_delete()
{
    if ( players ) {
        list_delete( players );
        players = 0;
    }
}

/*
====================================================================
Получите самого первого игрока.
====================================================================
*/
Player* players_get_first()
{
    int dummy;
    cur_player_id = -1;
    return players_get_next(&dummy);
}

/*
====================================================================
Получить следующего игрока в цикле, а также установить для new_turn значение true, если все
игроки закончили свой ход, и пришло время
новый ход.
====================================================================
*/
Player *players_get_next( int *new_turn )
{
    Player *player;
    *new_turn = 0;
    do {
        cur_player_id++;
        if ( cur_player_id == players->count ) {
            if (deploy_turn)
                deploy_turn = 0;
            else
                *new_turn = 1;
            cur_player_id = 0;
        }
        player = list_get( players, cur_player_id );
        //printf("Pl: %s,%d\n",player->name,player->no_init_deploy);
    } while (deploy_turn&&player->no_init_deploy);
    return player;
}

/*
====================================================================
Проверьте, кто будет следующим игроком, но не выбирайте его.
====================================================================
*/
Player *players_test_next()
{
    if ( cur_player_id + 1 == players->count )
        return list_get( players, 0 );
    return list_get( players, cur_player_id + 1 );
}

/*
====================================================================
Установить и получить игрока как текущий по индексу.
====================================================================
*/
Player *players_set_current( int index )
{
    if ( index < 0 ) index = 0;
    if ( index > players->count ) index = 0;
    cur_player_id = index;
    return list_get( players, cur_player_id );
}

/*
====================================================================
Проверьте, являются ли эти два игрока союзниками.
====================================================================
*/
int player_is_ally( Player *player, Player *second )
{
    Player *ally;
    if ( player == second || player == 0 ) return 1;
    list_reset( player->allies );
    while ( ( ally = list_next( player->allies ) ) )
        if ( ally == second )
            return 1;
    return 0;
}

/*
====================================================================
Получите игрока, который контролирует нацию
====================================================================
*/
Player *player_get_by_nation( Nation *nation )
{
    int i;
    Player *player;
    list_reset( players );
    while ( ( player = list_next( players ) ) ) {
        for ( i = 0; i < player->nation_count; i++ )
            if ( nation == player->nations[i] )
                return player;
    }
    return 0;
}

/*
====================================================================
Получить игрока с этой строкой идентификатора.
====================================================================
*/
Player *player_get_by_id( char *id )
{
    Player *player;
    list_reset( players );
    while ( ( player = list_next( players ) ) ) {
        if ( STRCMP( player->id, id ) )
            return player;
    }
    return 0;
}

/*
====================================================================
Получить игрока с этим индексом
====================================================================
*/
Player *player_get_by_index( int index )
{
    return list_get( players, index );
}

/*
====================================================================
Получить индекс игрока в списке
====================================================================
*/
int player_get_index( Player *player )
{
    Player *entry;
    int i = 0;
    list_reset( players );
    while ( ( entry = list_next( players ) ) ) {
        if ( player == entry )
            return i;
        i++;
    }
    return 0;
}
