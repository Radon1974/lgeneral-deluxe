/***************************************************************************
                          event.c  -  description
                             -------------------
    begin                : Wed Mar 20 2002
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

#include "lgeneral.h"
#include "event.h"

int buttonup = 0, buttondown = 0;
int motion = 0;
int motion_x = 50, motion_y = 50;
int keystate[SDLK_LAST];
int buttonstate[10];
int sdl_quit = 0;

/*
====================================================================
Локальные
====================================================================
*/

/*
====================================================================
Фильтр событий, блокирующий все события. Используется для очистки
очереди событий SDL.
====================================================================
*/
int all_filter( const SDL_Event *event )
{
    return 0;
}

/*
====================================================================
Возвращает, нажата ли какая-либо кнопка мыши
====================================================================
*/
static int event_is_button_pressed()
{
    return (SDL_GetMouseState(0,0)!=0);
}
static int event_is_key_pressed()
{
    int i;
    Uint8 *keystate=SDL_GetKeyState(0);
    for (i=0;i<SDLK_LAST;i++)
    {
        if (i==SDLK_NUMLOCK||i==SDLK_CAPSLOCK||i==SDLK_SCROLLOCK) continue;
        if (keystate[i]) return 1;
    }
    return 0;
}

/*
====================================================================
Фильтр событий. Пока он активен, никакие события КЛАВИШ или МЫШИ
не будут доступны с помощью SDL_PollEvent().
====================================================================
*/
int event_filter( const SDL_Event *event )
{
    switch ( event->type ) {
        case SDL_MOUSEMOTION:
            motion_x = event->motion.x;
            motion_y = event->motion.y;
            buttonstate[event->motion.state] = 1;
            motion = 1;
            return 0;
        case SDL_MOUSEBUTTONUP:
            buttonstate[event->button.button] = 0;
            buttonup = event->button.button;
            return 0;
        case SDL_MOUSEBUTTONDOWN:
            buttonstate[event->button.button] = 1;
            buttondown = event->button.button;
            return 0;
        case SDL_KEYUP:
            keystate[event->key.keysym.sym] = 0;
            return 0;
        case SDL_KEYDOWN:
            keystate[event->key.keysym.sym] = 1;
            return 1;
        case SDL_QUIT:
            buttonup = event->button.button;
            sdl_quit = 1;
            return 0;
    }
    return 1;
}

/*
====================================================================
Публичные
====================================================================
*/

/*
====================================================================
Включение/выключение фильтрации событий мыши и клавиатуры.
====================================================================
*/
void event_enable_filter( void )
{
    SDL_SetEventFilter( event_filter );
    event_clear();
}
void event_disable_filter( void )
{
    SDL_SetEventFilter( 0 );
}
void event_clear( void )
{
    memset( keystate, 0, sizeof( int ) * SDLK_LAST );
    memset( buttonstate, 0, sizeof( int ) * 10 );
    buttonup = buttondown = motion = 0;
}

/*
====================================================================
Проверьте, произошло ли событие движения с момента последнего вызова.
====================================================================
*/
int event_get_motion( int *x, int *y )
{
    if ( motion ) {
        *x = motion_x;
        *y = motion_y;
        motion = 0;
        return 1;
    }
    return 0;
}
/*
====================================================================
Проверьте, была ли нажата кнопка, и верните ее значение.
====================================================================
*/
int event_get_buttondown( int *button, int *x, int *y )
{
    if ( buttondown ) {
        *button = buttondown;
        *x = motion_x;
        *y = motion_y;
        buttondown = 0;
        return 1;
    }
    return 0;
}
int event_get_buttonup( int *button, int *x, int *y )
{
    if ( buttonup ) {
        *button = buttonup;
        *x = motion_x;
        *y = motion_y;
        buttonup = 0;
        return 1;
    }
    return 0;
}

/*
====================================================================
Получить текущую позицию курсора.
====================================================================
*/
void event_get_cursor_pos( int *x, int *y )
{
    *x = motion_x; *y = motion_y;
}

/*
====================================================================
Проверьте, установлена ли в данный момент кнопка "button".
====================================================================
*/
int event_check_button( int button )
{
    return buttonstate[button];
}

/*
====================================================================
Проверьте, установлен ли в данный момент "ключ".
====================================================================
*/
int event_check_key( int key )
{
    return keystate[key];
}

/*
====================================================================
Проверьте, был ли получен SDL_QUIT.
====================================================================
*/
int event_check_quit()
{
    return sdl_quit;
}

/*
====================================================================
Очистить ключ события SDL (события нажатия клавиши)
====================================================================
*/
void event_clear_sdl_queue()
{
    SDL_Event event;
    SDL_SetEventFilter( all_filter );
    while ( SDL_PollEvent( &event ) );
    SDL_SetEventFilter( event_filter );
}

/*
====================================================================
Подождите, пока не будет нажата ни клавиша, ни кнопка.
====================================================================
*/
void event_wait_until_no_input()
{
    SDL_PumpEvents();
    while (event_is_button_pressed()||event_is_key_pressed())
    {
        SDL_Delay(20);
        SDL_PumpEvents();
    }
    event_clear();
}
