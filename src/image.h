/***************************************************************************
                          image.h  -  description
                             -------------------
    begin                : Tue Mar 21 2002
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

#ifndef __IMAGE_H
#define __IMAGE_H

/*
====================================================================
������������� �����. ������������ ��� ����������� ������ �����������.
====================================================================
*/
typedef struct {
    SDL_Rect buf_rect;   /* �������� ������������� � ������ */
    SDL_Rect surf_rect;  /* ����� ���������� � �������� */
    SDL_Rect old_rect;   /* �������� ������� ������ ������� ��� ����������� */
    SDL_Surface *buf;    /* ����������� ����� */
    SDL_Surface *surf;   /* ������������� ����� ����������� */
    int hide;            /* ���� True ���� ����� �� �������� */
    int moved;           /* ���� ��� False � ��� ���������� old_rect
                            ���������� � ������ ������ */
    int recently_hidden; /* ����������, ���� ����� � ������ buffer_draw */
} Buffer;

/*
====================================================================
�������� ����� �������� w, h � �������� x, y �� ��������� � surf.
����� ������� ����� SDL ������ ���� ���������������.
====================================================================
*/
Buffer* buffer_create( int w, int h, SDL_Surface *surf, int x, int y );
void buffer_delete( Buffer **buffer );

/*
====================================================================
������ ���� ����� � ����������� (��� �������� ��������� / ���������)
====================================================================
*/
void buffer_hide( Buffer *buffer, int hide );

/*
====================================================================
�������� ����� �� buffer-> surf.
====================================================================
*/
void buffer_get( Buffer *buffer );

/*
====================================================================
���������� ����� � buffer-> surf.
====================================================================
*/
void buffer_draw( Buffer *buffer );

/*
====================================================================
�������� �������������� ����������, ������� �������� � �������� �������.
====================================================================
*/
void buffer_add_refresh_rects( Buffer *buffer );

/*
====================================================================
�������� ��������� ������. �� �������� �� get (), �� draw ().
������������ ������ �������� ��������� �������� �������.
====================================================================
*/
void buffer_move( Buffer *buffer, int x, int y );
void buffer_resize( Buffer *buffer, int w, int h );
void buffer_set_surface( Buffer *buffer, SDL_Surface *surf );
void buffer_get_geometry(Buffer *buffer, int *x, int *y, int *w, int *h );
SDL_Rect *buffer_get_surf_rect( Buffer *buffer );

/*
====================================================================
�����. ������������ ��� ���������� ������� �������� ������ (��������, �������). ��� ����
��������� ��������� ��� �������� (������� �� img_rect).
====================================================================
*/
typedef struct {
    Buffer *bkgnd;          /* ������� ����� ��� ����������� */
    SDL_Rect img_rect;      /* �������� ������������� � ����������� */
    SDL_Surface *img;       /* ����� */
} Image;

/*
====================================================================
�������� �����. ����������� ����������� ��������� �������� image_delete ().
������� ����������� (������� �� ����� ���� ����������) ������������
0,0, buf_w, buf_h, ��� buf_w ��� buw_h 0 �������� ������������� ����� �����������.
������������ ������ ������� ��������� ��������� �������� �������.
������� ������� ��������� - x, y � "surf".
====================================================================
*/
Image *image_create( SDL_Surface *img, int buf_w, int buf_h, SDL_Surface *surf, int x, int y );
void image_delete( Image **image );

/*
====================================================================
��������� ������ �����������
====================================================================
*/
#define image_hide( image, hide ) buffer_hide( image->bkgnd, hide )

/*
====================================================================
�������� ��� �����������
====================================================================
*/
#define image_get_bkgnd( img ) buffer_get( img->bkgnd )

/*
====================================================================
������ ����������� (����������� ��� � ������� ���������).
====================================================================
*/
#define image_draw_bkgnd( image ) buffer_draw( image->bkgnd )

/*
====================================================================
��������� ������� ����������� � ������� ���������.
������� ��������� ���.
��� ������������� �������� refresh_rects, ������� �������� � �������
���������.
====================================================================
*/
void image_draw( Image *image );

/*
====================================================================
�������� ��������� �����������. �� �������� �������� (��
����������� ��� ���), �� ��� ������������� ����� �������� ������ ����.
====================================================================
*/
#define image_move( img, x, y ) buffer_move( img->bkgnd, x, y )
#define image_set_surface( img, surf ) buffer_set_surface( img->bkgnd, surf )
void image_set_region( Image *image, int x, int y, int w, int h );


/*
====================================================================
��������. �����������, ������� ������ ������� ��������� �� ������ "��������"
������������ (��������� ����).
�� ����� ����������� ����� ������� ��������� ��������.
������ ������ ������������ ����� ��������, � ������ ������� #i - ����� �����.
#i ������ ��������.
====================================================================
*/
typedef struct {
    Image *img;     /* ����� ������� */
    int playing;    /* ������, ���� �������� ��������������� */
    int loop;       /* ������, ���� �������� ��������� � ����������� ����� */
    int speed;      /* ����� � ������������� ������������ ���� */
    int cur_time;   /* ���� ��� ��������� ��������, ��������������� ��������� ���� (��� �������� ���������������) */
} Anim;

/*
====================================================================
�������� ��������. ����������� �������� ���������
anim_delete (). ������ ������ ������� ����� - frame_w, frame_h.
������� ������� ��������� - x, y � "surf".
�� ��������� �������� ��������������� � ��������������� �� ������ ����.
������ ��������.
�� ��������� �������� ������.
====================================================================
*/
Anim* anim_create( SDL_Surface *gfx, int speed, int frame_w, int frame_h, SDL_Surface *surf, int x, int y );
void anim_delete( Anim **anim );

/*
====================================================================
������ ��������
====================================================================
*/
#define anim_hide( anim, hide ) buffer_hide( anim->img->bkgnd, hide )
#define anim_get_bkgnd( anim ) buffer_get( anim->img->bkgnd )
#define anim_draw_bkgnd( anim ) buffer_draw( anim->img->bkgnd )
#define anim_draw( anim ) image_draw( anim->img )

/*
====================================================================
�������� ��������� ��������
====================================================================
*/
#define anim_move( anim, x, y ) buffer_move( anim->img->bkgnd, x, y )
#define anim_set_surface( anim, surf ) buffer_set_surface( anim->img->bkgnd, surf )
void anim_set_speed( Anim *anim, int speed );
void anim_set_row( Anim *anim, int id );
void anim_set_frame( Anim *anim, int id );
void anim_play( Anim *anim, int loop );
void anim_stop( Anim *anim );
void anim_update( Anim *anim, int ms );


/*
====================================================================
�����. ����������� ����������� �������������� � ������ � ������
�� ������� �����. ���� ����� <255, ��� �����������
��������� ������� ������� ����������� ��� ���������.
====================================================================
*/
typedef struct {
    Image *img;
    SDL_Surface *shadow;
    SDL_Surface *contents;
    SDL_Surface *frame;
    int alpha;
} Frame;

/*
====================================================================
�������� �����. ���� �����> 0, ���
��������� �� ���� ������������� ������� ����������� ��� ���������.
������ ������, ����������� � ����������� �����
������������ ��������� ����� ����������� img.
������� ������� ��������� - x, y � "surf".
====================================================================
*/
Frame *frame_create( SDL_Surface *img, int alpha, SDL_Surface *surf, int x, int y );
void frame_delete( Frame **frame );

/*
====================================================================
���������� �����
====================================================================
*/
void frame_hide( Frame *frame, int hide );
#define frame_get_bkgnd( frame ) buffer_get( frame->img->bkgnd )
#define frame_draw_bkgnd( frame ) buffer_draw( frame->img->bkgnd )
void frame_draw( Frame *frame );

/*
====================================================================
�������� / �������� ��������� �����
====================================================================
*/
#define frame_move( frame, x, y ) buffer_move( frame->img->bkgnd, x, y )
#define frame_set_surface( frame, surf ) buffer_set_surface( frame->img->bkgnd, surf )
#define frame_get_geometry(frame,x,y,w,h) buffer_get_geometry(frame->img->bkgnd,x,y,w,h)
void frame_apply( Frame *frame ); /* ��������� ���������� ���������� � ����������� */
#define frame_get_width( frame ) ( (frame)->img->img->w )
#define frame_get_height( frame ) ( (frame)->img->img->h )

#endif