/***************************************************************************
                          sdl.c  -  description
                             -------------------
    begin                : Thu Apr 20 2000
    copyright            : (C) 2000 by Michael Speck
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

#include "sdl.h"
#include "SDL_image.h"

#include "misc.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "localize.h"

extern int  term_game;

Sdl sdl;
SDL_Cursor *empty_cursor = 0, *std_cursor = 0;

/* ������ */
int cur_time, last_time;

/* SDL ����������� */

/* ������� ������ ���� � ���������� ����������� */
void get_full_bmp_path( char *full_path, const char *file_name )
{
    snprintf(full_path, 128, "%s/%s", get_gamedir(), file_name );
}

/*
    ��������� ����������� �� �����, �������� �� � ����������� ��� ���������� ������
*/
SDL_Surface* load_surf(const char *fname, int f, int outside_width, int outside_height,
                       int inside_width, int inside_height)
{
    SDL_Surface *buf;
    SDL_Surface *new_sur;
    char path[ MAX_PATH ];
    SDL_PixelFormat *spf;

    get_full_bmp_path( path, fname );

    buf = IMG_Load( path );

    if ( buf == 0 ) {
        fprintf( stderr, "%s: %s\n", fname, SDL_GetError() );
        if ( f & SDL_NONFATAL )
            return 0;
        else
            exit( 1 );
    }
/*    if ( !(f & SDL_HWSURFACE) ) {

        SDL_SetColorKey( buf, SDL_SRCCOLORKEY, 0x0 );
        return buf;

    }
    new_sur = create_surf(buf->w, buf->h, f);
    SDL_BlitSurface(buf, 0, new_sur, 0);
    SDL_FreeSurface(buf);*/
    spf = SDL_GetVideoSurface()->format;

    if ( outside_width == inside_width || outside_height == inside_height )
    {
        new_sur = SDL_ConvertSurface( buf, spf, f );
        SDL_SetColorKey( new_sur, SDL_SRCCOLORKEY, 0x0 );
    }
    else
    {
        int x = 0, y = 0, number_x, number_y, i, j, internal_i, internal_j;
        SDL_Surface *temp;
        Uint32 pixel;
        number_x = buf->w / outside_width;
        number_y = buf->h / outside_height;
        temp = create_surf( inside_width * number_x, inside_height * number_y, SDL_SWSURFACE);
        /* ������� ������ */
        while ( get_pixel( buf, x, y ) == 0xffe1e1 )
        {
            if ( x < buf->w + 1 )
                x++;
            else
            {
                x = 0;
                y++;
            }
        }
        /* ���������� ����������� */
        for ( i = 0; i < number_x; i++ )
            for ( j = 0; j < number_y; j++ )
                /* �������� ����� 0x0 (������) � 0xffe1e1 (�������) */
                for ( internal_i = 0; internal_i < inside_width; internal_i++ )
                    for ( internal_j = 0; internal_j < inside_height; internal_j++ )
                    {
                        pixel = get_pixel( buf, i * outside_width + x + internal_i, j * outside_height + y + internal_j );
                        switch (pixel)
                        {
                            case 0x0:      set_pixel( temp, i * inside_width + internal_i,
                                                      j * inside_height + internal_j, 0x1 );
                                           break;
                            case 0xffe1e1: set_pixel( temp, i * inside_width + internal_i,
                                                      j * inside_height + internal_j, 0x0 );
                                           break;
                            default:       set_pixel( temp, i * inside_width + internal_i,
                                           j * inside_height + internal_j, pixel );
                                           break;
                        }
                    }

        new_sur = SDL_ConvertSurface( temp, spf, f );
        SDL_SetColorKey( new_sur, SDL_SRCCOLORKEY, 0x0 );
        SDL_FreeSurface( temp );
    }
    SDL_FreeSurface( buf );
    SDL_SetAlpha( new_sur, 0, 0 ); /* ��� ����� */
    return new_sur;
}

/*
    ������� �����������
    �� ������ ��������������, ���� �� �������� SDLSCREEN
*/
SDL_Surface* create_surf(int w, int h, int f)
{
    SDL_Surface *sur;
    SDL_PixelFormat *spf = SDL_GetVideoSurface()->format;
    if ((sur = SDL_CreateRGBSurface(f, w, h, spf->BitsPerPixel, spf->Rmask, spf->Gmask, spf->Bmask, spf->Amask)) == 0) {
        fprintf(stderr, "ERR: ssur_create: not enough memory to create surface...\n");
        exit(1);
    }
/*    if (f & SDL_HWSURFACE && !(sur->flags & SDL_HWSURFACE))
        fprintf(stderr, "unable to create surface (%ix%ix%i) in hardware memory...\n", w, h, spf->BitsPerPixel);*/
    SDL_SetColorKey(sur, SDL_SRCCOLORKEY, 0x0);
    SDL_SetAlpha(sur, 0, 0); /* ��� ����� */
    return sur;
}

void free_surf( SDL_Surface **surf )
{
    if ( *surf ) {
        SDL_FreeSurface( *surf );
        *surf = 0;
    }
}
/*
    ������� ������ �����������
*/
int disp_format(SDL_Surface *sur)
{
    if ((sur = SDL_DisplayFormat(sur)) == 0) {
        fprintf(stderr, "ERR: ssur_displayformat: convertion failed\n");
        return 1;
    }
    return 0;
}

/*
    ����������� �������
*/
void lock_surf(SDL_Surface *sur)
{
    if (SDL_MUSTLOCK(sur))
        SDL_LockSurface(sur);
}

/*
    ����������� �������
*/
void unlock_surf(SDL_Surface *sur)
{
    if (SDL_MUSTLOCK(sur))
        SDL_UnlockSurface(sur);
}

/*
    blit ����������� � ����������� DEST � ���������� SOURCE, ��������� ��� ����������� ��������� ����� � �����
*/
void blit_surf(void)
{
    SDL_BlitSurface(sdl.s.s, &sdl.s.r, sdl.d.s, &sdl.d.r);
}

/*
    ������� ����� blit
*/
void alpha_blit_surf(int alpha)
{
    SDL_SetAlpha(sdl.s.s, SDL_SRCALPHA, alpha);
    SDL_BlitSurface(sdl.s.s, &sdl.s.r, sdl.d.s, &sdl.d.r);
    SDL_SetAlpha(sdl.s.s, 0, 0);
}

/*
    ��������� ����������� ������ c
*/
void fill_surf(int c)
{
    SDL_FillRect(sdl.d.s, &sdl.d.r, SDL_MapRGB(sdl.d.s->format, c >> 16, (c >> 8) & 0xFF, c & 0xFF));
}

/* ���������� ������������� ��������� */
void set_surf_clip( SDL_Surface *surf, int x, int y, int w, int h )
{
    SDL_Rect rect = { x, y, w, h };
    if ( w == h || h == 0 )
        SDL_SetClipRect( surf, 0 );
    else
        SDL_SetClipRect( surf, &rect );
}

/* ���������� ������� */
Uint32 set_pixel( SDL_Surface *surf, int x, int y, int pixel )
{
    int pos = 0;

    pos = y * surf->pitch + x * surf->format->BytesPerPixel;
    memcpy( surf->pixels + pos, &pixel, surf->format->BytesPerPixel );
    return pixel;
}

/* �������� ������� */
Uint32 get_pixel( SDL_Surface *surf, int x, int y )
{
    int pos = 0;
    Uint32 pixel = 0;

    pos = y * surf->pitch + x * surf->format->BytesPerPixel;
    memcpy( &pixel, surf->pixels + pos, surf->format->BytesPerPixel );
    return pixel;
}

/* sdl ����� */

/* ������� ������ ���� � ������ */
void get_full_font_path( char *path, const char *file_name )
{
    strcpy( path, file_name );
/*    sprintf(path, "./gfx/fonts/%s", file_name ); */
}

/*
 * ��������� ����� � ����� �� ����� 'fname'.
 *
 * �������� ������� ������:
 *
 * ������ ���� ������ ������������ ����� ��������� �����������, ��������� �� ����� ������ ����������� ������.
 * ����������� �������� ���� ���������� ���� �������.
 *
 * ������ ������ ������ ������������ ������� ���������� �����������.
 *
 * ������ ���� ������������ ��������� ������������, ��� �������� ����.
 * ������ (��� ������ ����� ������������ ��������������� �������� rgb):
 *
 * (1) -----> ........
 *            ........
 *            ..####..
 *            .#....#.
 *            .#....#.
 *            .#....#.
 *            .#....#.
 *            .#....#.
 *            .#.#..#.
 *            .#..#.#.
 *            ..####..
 *            ......#.
 * (2, 3) --> S.......
 *
 * (1): ������� ����� ������� ���������� ������� ������������. ��� �������
 * ��� �� ����, ��� � (1), ��������� ����������, ���� ����
 * ����������.
 * �������� ��������, ��� (1) ����� ����������� ������ ��� * ������� * ����� �
 * ���� � ����������� �� ���� ��������� ������.
 *
 * (2): ������ ����� ������� ���������� ��������� ������� �������������
 * ���� � ��������� ����������� ������. �������� ��� �������, ��������� �������
 * ����� ���������� ������ �����.
 * ���� - ���� # FF00FF ��� ��������������� �����, ���� # FF0000 ��� �������������.
 * ���� (� ���� ������ �� �� ����� ������������,
 * ���������� �� ����, ��� �� �������� � ��������� ������).
 *
 * (3): ��� ������ ������� ������� (2) ����� ������ ��������. ��� ������������
 * ��������� �������� ���������� � ����������� ������.
 * �������� r ��������� ��������� ��� ������� �����.
 * �������� g � b ��������������� � ������ ���� (0)
 *
 * �������������� ����� � ��� ������� �����������, ������� � ����
 * ������� � (3). ����� ����������� ������ ������: ��� ������� ���������
 * (2) ��� ����� ��������, � ��������������� �������� �������� � ������
 * ��������������. ����� ���������� �������� ����� ������������
 * ���������� ������, ������������ � �����.
 *
 * ��� ����������� ��������������, ��� (2) � (3) ����� ���� (1).
 */
void font_load_glyphs(Font *font, const char *fname)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#  define R_INDEX 1
#  define G_INDEX 2
#  define B_INDEX 3
#else
#  define R_INDEX 2
#  define G_INDEX 1
#  define B_INDEX 0
#endif
#define AT_RGB(pixel, idx) (((Uint8 *)&(pixel))+(idx))
    char    path[MAX_PATH];
    int     i;
    Uint32  transparency_key;
    Uint8   start_code, reserved1, reserved2;
    SDL_Surface *new_glyphs;

    get_full_font_path( path, fname );

    if ((new_glyphs = load_surf(path, SDL_SWSURFACE, 0, 0, 0, 0)) == 0) {
        fprintf(stderr, tr("Cannot load new glyphs surface: %s\n"), SDL_GetError());
        exit(1);
    }
    /* ����������� (1) ��� ���� ������������ */
    transparency_key = get_pixel( new_glyphs, 0, 0 );

    /* ������� (3) */
    SDL_GetRGB(get_pixel( new_glyphs, 0, new_glyphs->h - 1 ), new_glyphs->format, &start_code, &reserved1, &reserved2);
    // �������� ������� � ��������������� �� ���������������� ������, ����� �������������� �����
    // ������ ������� ��� ������������
    if (reserved1 || reserved2) {
        fprintf(stderr, tr("Invalid font file: %d, %d\n"), reserved1, reserved2);
        exit(1);
    }

    /* �������� ������ ������ */
    /* FIXME: �� ����������� �����, ����������� ���� */
    font->height = new_glyphs->h;

    /* �������� ������������� ������ ������ � ��� �����, ��� ����� �������� ����� ������ ������,
     * � �������� ���, ��� ����� ������������� ������ ������������ �������� ������
     * �� new_glyphs.
     */
    {
        /* ����� ������ ���������� ����������� ������ */
        int pre_width = font->char_offset[start_code];
        /* ����� ������ ��������� ����������� ������ */
        int post_width = (font->pic ? font->pic->w : 0) - pre_width;
        unsigned code = start_code;
        int fixup;	/* ���������� �������� ����� �������� ������ ���� ���������� */
        SDL_Surface *dest;

        /* �������������� ������ � �������� ����� ������ */
        /* ������������ ��������� ������ ��������� ����������� ������ */
        for (i = 0; i < new_glyphs->w; i++) {
            Uint32 pixel = 0;
            int valid_glyph;
            SDL_GetRGB(get_pixel(new_glyphs, i, new_glyphs->h - 1), new_glyphs->format, AT_RGB(pixel, R_INDEX), AT_RGB(pixel, G_INDEX), AT_RGB(pixel, B_INDEX));
            pixel &= 0xf8f8f8;	/* ���������� � RGB565 � ������� ������������ */
            if (i != 0 && pixel != 0xf800f8 && pixel != 0xf80000) continue;

            if (code > 256) {
                fprintf(stderr, tr("font '%s' contains too many glyphs\n"), path);
                break;
            }

            valid_glyph = i == 0 || pixel != 0xf80000;
            set_pixel(new_glyphs, i, new_glyphs->h - 1, transparency_key);

            font->char_offset[code] = pre_width + i;
            font->keys[code] = valid_glyph;
            code++;

        }

        fixup = pre_width + i - font->char_offset[code];
        post_width -= font->char_offset[code] - font->char_offset[start_code];

        /* ������ �������� �����:
         * 1. �������� ����� �����������, ���������� �������, ����� �������� ��� �����������, ��� � ����� �����.
         * 2. ���� ����������� ���������� �����.
         * 3. ������������� ����������� ��������� �����.
         * 4. ���� ����� �����.
         */
        dest = create_surf(pre_width + new_glyphs->w + post_width,
        		   font->pic ? font->pic->h : new_glyphs->h, SDL_HWSURFACE);
        if (!dest) {
            fprintf(stderr, tr("could not create font surface: %s\n"), SDL_GetError());
            exit(1);
        }

        if (pre_width > 0) {
            assert(font->pic);
            DEST(dest, 0, 0, pre_width, font->height);
            SOURCE(font->pic, 0, 0);
            blit_surf();
        }

        if (post_width > 0) {
            assert(font->pic);
            DEST(dest, font->char_offset[code] + fixup, 0, post_width, font->height);
            SOURCE(font->pic, font->char_offset[code], 0);
            blit_surf();
        }

        DEST(dest, font->char_offset[start_code], 0, new_glyphs->w, font->height);
        FULL_SOURCE(new_glyphs);
        blit_surf();

        /* �������� ������ ������ ����� ������� */
        SDL_FreeSurface(font->pic);
        font->pic = dest;

        /* ���������� ������������ ����������� */
        for (i = code; i < 256; i++)
            font->char_offset[code] += fixup;
        font->width += fixup;
    }

    SDL_SetColorKey( font->pic, SDL_SRCCOLORKEY, transparency_key );

    SDL_FreeSurface(new_glyphs);
#undef R_INDEX
#undef G_INDEX
#undef B_INDEX
#undef AT_RGB
}

/**
 * ������� ����� ����� �� ����� ������� 'fname'
 */
Font* load_font(const char *fname)
{
    Font *fnt = calloc(1, sizeof(Font));
    if (fnt == 0) {
        fprintf(stderr, tr("ERR: %s: not enough memory\n"), __FUNCTION__);
        exit(1);
    }

    fnt->align = ALIGN_X_LEFT | ALIGN_Y_TOP;
    fnt->color = 0x00FFFFFF;

    font_load_glyphs(fnt, fname);
    return fnt;
}

/*
    ��������� ������
*/
void free_font(Font **fnt)
{
    if ( *fnt ) {
        if ((*fnt)->pic) SDL_FreeSurface((*fnt)->pic);
        free(*fnt);
        *fnt = 0;
    }
}

/*
    �������� ���-������ � �������������
*/
int write_text(Font *fnt, SDL_Surface *dest, int x, int y, const char *str, int alpha)
{
    int len = strlen(str);
    int pix_len = text_width(fnt, str);
    int px = x, py = y;
    int i;
    SDL_Surface *spf = SDL_GetVideoSurface();
    const int * const ofs = fnt->char_offset;

    /* ������������ */
    if (fnt->align & ALIGN_X_CENTER)
        px -= pix_len >> 1;
    else if (fnt->align & ALIGN_X_RIGHT)
        px -= pix_len;
    if (fnt->align & ALIGN_Y_CENTER)
        py -= (fnt->height >> 1 ) + 1;
    else
        if (fnt->align & ALIGN_Y_BOTTOM)
            py -= fnt->height;

    fnt->last_x = MAXIMUM(px, 0);
    fnt->last_y = MAXIMUM(py, 0);
    fnt->last_width = MINIMUM(pix_len, spf->w - fnt->last_x);
    fnt->last_height = MINIMUM(fnt->height, spf->h - fnt->last_y);

    if (alpha != 0)
        SDL_SetAlpha(fnt->pic, SDL_SRCALPHA, alpha);
    else
        SDL_SetAlpha(fnt->pic, 0, 0);
    for (i = 0; i < len; i++) {
        unsigned c = (unsigned char)str[i];
        if (!fnt->keys[c]) c = '\177';
        {
            const int w = ofs[c+1] - ofs[c];
            DEST(dest, px, py, w, fnt->height);
            SOURCE(fnt->pic, ofs[c], 0);
            blit_surf();
            px += w;
        }
    }

    return 0;
}

/*
====================================================================
�������� ������ � x, y � �������� y ���, ����� ��� ������������ �
��������� ������.
====================================================================
*/
void write_line( SDL_Surface *surf, Font *font, const char *str, int x, int *y )
{
    write_text( font, surf, x, *y, str, 255 );
    *y += font->height;
}

/*
    ������������� ����������� ������
*/
void lock_font(Font *fnt)
{
    if (SDL_MUSTLOCK(fnt->pic))
        SDL_LockSurface(fnt->pic);
}

/*
    �������������� ����������� ������
*/
void unlock_font(Font *fnt)
{
    if (SDL_MUSTLOCK(fnt->pic))
        SDL_UnlockSurface(fnt->pic);
}

/*
    ������� ������ ���������� ����������
*/
SDL_Rect last_write_rect(Font *fnt)
{
    SDL_Rect    rect={fnt->last_x, fnt->last_y, fnt->last_width, fnt->last_height};
    return rect;
}

int  char_width(Font *fnt, char c)
{
    unsigned i = (unsigned char)c;
    return fnt->char_offset[i + 1] - fnt->char_offset[i];
}

/*
    return the text width in pixels
*/
int text_width(Font *fnt, const char *str)
{
    unsigned int i;
    int pix_len = 0;
    for (i = strlen(str); i > 0; )
        pix_len += char_width(fnt, str[--i]);
    return pix_len;
}

/* SDL */

/*
    ���������������� SDL
*/
void init_sdl( int f )
{
    /* ��������� �����: ���� ���� �� �������, ���� SDL_INIT_AUDIO �� ������ ���� ���������� */
#ifndef WITH_SOUND
    if ( f & SDL_INIT_AUDIO )
        f = f & ~SDL_INIT_AUDIO;
#endif

    sdl.screen = 0;
    if (SDL_Init(f) < 0) {
        fprintf(stderr, "ERR: sdl_init: %s", SDL_GetError());
        exit(1);
    }
    SDL_EnableUNICODE(1);
    atexit(quit_sdl);
    /* ������� ������ ������ */
    empty_cursor = create_cursor( 16, 16, 8, 8,
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                "
                                  "                " );
    std_cursor = SDL_GetCursor();
}

/*
    ���������� �����
*/
void quit_sdl()
{
    if (sdl.screen) SDL_FreeSurface(sdl.screen);
    if ( empty_cursor ) SDL_FreeCursor( empty_cursor );
    if (sdl.vmodes) free(sdl.vmodes);
    SDL_Quit();
    printf("SDL finalized\n");
}

/** �������� ������ ���� ������������. �������� @vmi � ������� ����������
 * ������. */
int get_video_modes( VideoModeInfo **vmi )
{
	SDL_Rect **modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
	int i, nmodes = 0;
	int bpp = SDL_GetVideoInfo()->vfmt->BitsPerPixel;

	*vmi = NULL;

	if (modes == NULL || modes == (SDL_Rect **)-1) {
		VideoModeInfo stdvmi[2] = {
			{ 640, 480, 32, 0 },
			{ 640, 480, 32, 1 }
		};
		fprintf(stderr,"No video modes available or specified.\n");
		/* ���������� �� ����� 640x480 */
		nmodes = 2;
		*vmi = calloc(nmodes,sizeof(VideoModeInfo));
		(*vmi)[0] = stdvmi[0];
		(*vmi)[1] = stdvmi[1];
		return nmodes;
	}

	/* ���������� ���������� ������� */
	for (nmodes = 0; modes[nmodes]; nmodes++);
	nmodes *= 2; /* ������ ��� ���� � ������������� ����� */

	*vmi = calloc(nmodes,sizeof(VideoModeInfo));
	for (i = 0; i < nmodes/2; i++) {
		VideoModeInfo *info = &((*vmi)[i*2]);
		info->width = modes[i]->w;
		info->height = modes[i]->h;
		info->depth = bpp;
		info->fullscreen = 0;
		(*vmi)[i*2+1] = *info;
		(*vmi)[i*2+1].fullscreen = 1;
	}
	return nmodes;
}

/*
====================================================================
������� � ����� ������������ �����.
====================================================================
*/
int set_video_mode( int width, int height, int fullscreen )
{
#ifdef SDL_DEBUG
    SDL_PixelFormat	*fmt;
#endif
    int depth = 32;
    int flags = SDL_SWSURFACE;
    /* ���� ����� ������������� ����������, ���������, ����� �� �� ����� ����� �� ���������� */
    if ( sdl.screen ) {
        if ( sdl.screen->w == width && sdl.screen->h == height )
            if ( ( sdl.screen->flags & SDL_FULLSCREEN ) == fullscreen )
                return 1;
    }
    /* ���������� ������ ����� */
    if (sdl.screen) SDL_FreeSurface( sdl.screen ); sdl.screen = 0;
    /* ���������� ����� ����� */
    if ( fullscreen ) flags |= SDL_FULLSCREEN;
    if ( ( depth = SDL_VideoModeOK( width, height, depth, flags ) ) == 0 ) {
        fprintf( stderr, tr("Requested mode %ix%i, Fullscreen: %i unavailable\n"),
                 width, height, fullscreen );
        sdl.screen = SDL_SetVideoMode( 640, 480, 16, SDL_SWSURFACE );
    }
    else
        if ( ( sdl.screen = SDL_SetVideoMode( width, height, depth, flags ) ) == 0 ) {
            fprintf(stderr, "%s", SDL_GetError());
            return 1;
        }

#ifdef SDL_DEBUG
    if (f & SDL_HWSURFACE && !(sdl.screen->flags & SDL_HWSURFACE))
       	fprintf(stderr, "unable to create screen in hardware memory...\n");
    if (f & SDL_DOUBLEBUF && !(sdl.screen->flags & SDL_DOUBLEBUF))
        fprintf(stderr, "unable to create double buffered screen...\n");
    if (f & SDL_FULLSCREEN && !(sdl.screen->flags & SDL_FULLSCREEN))
        fprintf(stderr, "unable to switch to fullscreen...\n");

    fmt = sdl.screen->format;
    printf("video mode format:\n");
    printf("Masks: R=%i, G=%i, B=%i\n", fmt->Rmask, fmt->Gmask, fmt->Bmask);
    printf("LShft: R=%i, G=%i, B=%i\n", fmt->Rshift, fmt->Gshift, fmt->Bshift);
    printf("RShft: R=%i, G=%i, B=%i\n", fmt->Rloss, fmt->Gloss, fmt->Bloss);
    printf("BBP: %i\n", fmt->BitsPerPixel);
    printf("-----\n");
#endif

    return 0;
}

/*
    �������� ����������� ������������
*/
void hardware_cap()
{
    const SDL_VideoInfo	*vi = SDL_GetVideoInfo();
    char *ny[2] = {"No", "Yes"};

    printf("video hardware capabilities:\n");
    printf("Hardware Surfaces: %s\n", ny[vi->hw_available]);
    printf("HW_Blit (CC, A): %s (%s, %s)\n", ny[vi->blit_hw], ny[vi->blit_hw_CC], ny[vi->blit_hw_A]);
    printf("SW_Blit (CC, A): %s (%s, %s)\n", ny[vi->blit_sw], ny[vi->blit_sw_CC], ny[vi->blit_sw_A]);
    printf("HW_Fill: %s\n", ny[vi->blit_fill]);
    printf("Video Memory: %i\n", vi->video_mem);
    printf("------\n");
}

/*
    �������� ������������� (0,0,0,0) -> ������������� �����
*/
void refresh_screen(int x, int y, int w, int h)
{
    SDL_UpdateRect(sdl.screen, x, y, w, h);
    sdl.rect_count = 0;
}

/*
    ���������� ��� ������� ����������
*/
void refresh_rects()
{
    if (sdl.rect_count == RECT_LIMIT)
        SDL_UpdateRect(sdl.screen, 0, 0, sdl.screen->w, sdl.screen->h);
    else
        if ( sdl.rect_count > 0 )
            SDL_UpdateRects(sdl.screen, sdl.rect_count, sdl.rect);
    sdl.rect_count = 0;
}

/*
    �������� ������� ���������� / �������������
*/
void add_refresh_region( int x, int y, int w, int h )
{
    if (sdl.rect_count == RECT_LIMIT) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > sdl.screen->w)
        w = sdl.screen->w - x;
    if (y + h > sdl.screen->h)
        h = sdl.screen->h - y;
    if (w <= 0 || h <= 0)
        return;
    sdl.rect[sdl.rect_count].x = x;
    sdl.rect[sdl.rect_count].y = y;
    sdl.rect[sdl.rect_count].w = w;
    sdl.rect[sdl.rect_count].h = h;
    sdl.rect_count++;
}
void add_refresh_rect( SDL_Rect *rect )
{
    if ( rect )
        add_refresh_region( rect->x, rect->y, rect->w, rect->h );
}

/*
    ��������� ����� �� �������
*/
void dim_screen(int steps, int delay, int trp)
{
#ifndef NODIM
    SDL_Surface    *buffer;
    int per_step = trp / steps;
    int i;
    if (term_game) return;
    buffer = create_surf(sdl.screen->w, sdl.screen->h, SDL_SWSURFACE);
    SDL_SetColorKey(buffer, 0, 0);
    FULL_DEST(buffer);
    FULL_SOURCE(sdl.screen);
    blit_surf();
    for (i = 0; i <= trp; i += per_step) {
        FULL_DEST(sdl.screen);
        fill_surf(0x0);
        FULL_SOURCE(buffer);
        alpha_blit_surf(i);
        refresh_screen( 0, 0, 0, 0);
        SDL_Delay(delay);
    }
    if (trp == 255) {
        FULL_DEST(sdl.screen);
        fill_surf(0x0);
        refresh_screen( 0, 0, 0, 0);
    }
    SDL_FreeSurface(buffer);
#else
    refresh_screen( 0, 0, 0, 0);
#endif
}

/*
    ����� undim
*/
void undim_screen(int steps, int delay, int trp)
{
#ifndef NODIM
    SDL_Surface    *buffer;
    int per_step = trp / steps;
    int i;
    if (term_game) return;
    buffer = create_surf(sdl.screen->w, sdl.screen->h, SDL_SWSURFACE);
    SDL_SetColorKey(buffer, 0, 0);
    FULL_DEST(buffer);
    FULL_SOURCE(sdl.screen);
    blit_surf();
    for (i = trp; i >= 0; i -= per_step) {
        FULL_DEST(sdl.screen);
        fill_surf(0x0);
        FULL_SOURCE(buffer);
        alpha_blit_surf(i);
        refresh_screen( 0, 0, 0, 0);
        SDL_Delay(delay);
    }
    FULL_DEST(sdl.screen);
    FULL_SOURCE(buffer);
    blit_surf();
    refresh_screen( 0, 0, 0, 0);
    SDL_FreeSurface(buffer);
#else
    refresh_screen( 0, 0, 0, 0);
#endif
}

/*
    ������� ����
*/
int wait_for_key()
{
    /* ��� ����� */
    SDL_Event event;
    while (1) {
        SDL_WaitEvent(&event);
        if (event.type == SDL_QUIT) {
            term_game = 1;
            return 0;
        }
        if (event.type == SDL_KEYUP)
            return event.key.keysym.sym;
    }
}

/*
    ��������� ������� ������� ��� ������ ����
*/
void wait_for_click()
{
    /* ����� ����� ��� ������ */
    SDL_Event event;
    while (1) {
        SDL_WaitEvent(&event);
        if (event.type == SDL_QUIT) {
            term_game = 1;
            return;
        }
        if (event.type == SDL_KEYUP || event.type == SDL_MOUSEBUTTONUP)
            return;
    }
}

/*
    ����������� �����
*/
void lock_screen()
{
    if (SDL_MUSTLOCK(sdl.screen))
        SDL_LockSurface(sdl.screen);
}

/*
    �������������� �����������
*/
void unlock_screen()
{
    if (SDL_MUSTLOCK(sdl.screen))
        SDL_UnlockSurface(sdl.screen);
}

/*
    ���� ���������� ������ (������� �����)
*/
void flip_screen()
{
    SDL_Flip(sdl.screen);
}

/* ������ */

/* ������� ������ */
SDL_Cursor* create_cursor( int width, int height, int hot_x, int hot_y, const char *source )
{
    unsigned char *mask = 0, *data = 0;
    SDL_Cursor *cursor = 0;
    int i, j, k;
    char data_byte, mask_byte;
    int pot;

    /* �������� ������� �� ���������:
        b: ������, w: �����, '': ���������� */

    /* ������� ����� � ������ */
    mask = malloc( width * height * sizeof ( char ) / 8 );
    data = malloc( width * height * sizeof ( char ) / 8 );

    k = 0;
    for (j = 0; j < width * height; j += 8, k++) {

        pot = 1;
        data_byte = mask_byte = 0;
        /* ������� ���� */
        for (i = 7; i >= 0; i--) {

            switch ( source[j + i] ) {

                case 'b':
                    data_byte += pot;
                case 'w':
                    mask_byte += pot;
                    break;

            }
            pot *= 2;

        }
        /* �������� � ����� */
        data[k] = data_byte;
        mask[k] = mask_byte;

    }

    /* ������� � ������� ������ */
    cursor = SDL_CreateCursor( data, mask, width, height, hot_x, hot_y );
    free( mask );
    free( data );
    return cursor;
}

/*
    �������� ������������ � ������� ���������� ������
*/
int get_time()
{
    int ms;
    cur_time = SDL_GetTicks();
    ms = cur_time - last_time;
    last_time = cur_time;
    if (ms == 0) {
        ms = 1;
        SDL_Delay(1);
    }
    return ms;
}

/*
    �������� ������
*/
void reset_timer()
{
    last_time = SDL_GetTicks();
}
