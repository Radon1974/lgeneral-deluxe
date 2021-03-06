/***************************************************************************
                          audio.c  -  description
                             -------------------
    begin                : Sun Jul 29 2001
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

#ifdef WITH_SOUND

#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "misc.h"

/*
====================================================================
Если аудиоустройство было правильно инициировано, этот флаг установлен.
Если этот флаг не установлен; для звука никаких действий предприниматься не будет.
====================================================================
*/
int audio_ok = 0;
/*
====================================================================
Если этот флаг не установлен, звук не воспроизводится.
====================================================================
*/
int audio_enabled = 1;
/*
====================================================================
Громкость всех звуков
====================================================================
*/
int audio_volume = 128;

/*
====================================================================
Initiate/close audio
====================================================================
*/
int audio_open()
{
    if ( Mix_OpenAudio( MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 256 ) < 0 ) {
        fprintf( stderr, "%s\n", SDL_GetError() );
        audio_ok = 0;
        return 0;
    }
    audio_ok = 1;
    return 1;
}
void audio_close()
{
    if ( !audio_ok ) return;
    Mix_CloseAudio();
}

/*
====================================================================
Изменить аудио
====================================================================
*/
void audio_enable( int enable )
{
    if ( !audio_ok ) return;
    audio_enabled = enable;
    if ( !enable )
        Mix_Pause( -1 ); /* остановить все звуковые каналы */
}
void audio_set_volume( int level )
{
    if ( !audio_ok ) return;
    if ( level < 0 ) level = 0;
    if ( level > 128 ) level = 128;
    audio_volume = level;
    Mix_Volume( -1, level ); /* все звуковые каналы */
}
void audio_fade_out( int channel, int ms )
{
    if ( !audio_ok ) return;
    Mix_FadeOutChannel( channel, ms );
}

/*
====================================================================
Волна
====================================================================
*/
Wav* wav_load( char *fname, int channel )
{
    Wav *wav = 0;
    if ( !audio_ok ) return 0;
    wav = calloc( 1, sizeof( Wav ) );
    wav->chunk = Mix_LoadWAV( fname );
    if ( wav->chunk == 0 ) {
        fprintf( stderr, "%s\n", SDL_GetError() );
        free( wav ); wav = 0;
    }
    wav->channel = channel;
    return wav;
}
void wav_free( Wav *wav )
{
    if ( !audio_ok ) return;
    if ( wav ) {
        if ( wav->chunk )
            Mix_FreeChunk( wav->chunk );
        free( wav );
    }
}
void wav_set_channel( Wav *wav, int channel )
{
    wav->channel = channel;
}
void wav_play( Wav *wav )
{
    if ( !audio_ok ) return;
    if ( !audio_enabled ) return;
    Mix_Volume( wav->channel, audio_volume );
    Mix_PlayChannel( wav->channel, wav->chunk, 0 );
}
void wav_play_at( Wav *wav, int channel )
{
    if ( !audio_ok ) return;
    wav->channel = channel;
    wav_play( wav );
}
void wav_fade_out( Wav *wav, int ms )
{
    if ( !audio_ok ) return;
    Mix_FadeOutChannel( wav->channel, ms );
}

#endif
