/***************************************************************************
                          audio.h  -  description
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

#ifndef __AUDIO_H
#define __AUDIO_H

#ifdef WITH_SOUND
#include <SDL_mixer.h>

/*
====================================================================
������� ��� ������� SDL_mixer.
====================================================================
*/

/*
====================================================================
���������������� / ������� �����
====================================================================
*/
int audio_open();
void audio_close();

/*
====================================================================
�������� �����
====================================================================
*/
void audio_enable( int enable );
void audio_set_volume( int level ); /* 0 - 128 */
void audio_fade_out( int channel, int ms );

/*
====================================================================
�����
====================================================================
*/
typedef struct {
    Mix_Chunk   *chunk;     /* ���� */
    int         channel;    /* ���� ��������������� �� ���� ������ */
} Wav;
Wav* wav_load( char *fname, int channel );
void wav_free( Wav *wav );
void wav_set_channel( Wav *wav, int channel );
void wav_play( Wav *wav );
void wav_play_at( Wav *wav, int channel );

#endif

#endif
