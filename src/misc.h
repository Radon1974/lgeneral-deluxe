/***************************************************************************
                          misc.h  -  description
                             -------------------
    begin                : Fri Jan 19 2001
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

#ifndef __MISC_H
#define __MISC_H
#include <stdio.h>

struct PData;
struct _Font;

#define MAX_BUFFER 4096
#define MAX_LINE_SHORT 1024
#define MAX_PATH 512
#define MAX_LINE 256
#define MAX_MESSAGE_LINE 100
#define MAX_NAME 50
#define MAX_EXTENSION 10

/* ���������, �������� �� ����� ��� ������ */
#define ODD( x ) ( x & 1 )
#define EVEN( x ) ( !( x & 1 ) )

/* ��������� � ����� */
#define FREE( ptr ) { if ( ptr ) free( ptr ); ptr = 0; }

/* ���������, ���������� �� � ��������� ��������� ���� */
#define CHECK_FLAGS( source, flags ) ( source & (flags) )

/* ������� ��������� �������� ����� ������� � ������ �������� (������������) */
#define RANDOM( lower, upper ) ( ( rand() % ( ( upper ) - ( lower ) + 1 ) ) + ( lower ) )
#define DICE(maxeye) (1+(int)(((double)maxeye)*rand()/(RAND_MAX+1.0)))

/* ���������, ��������� �� ������ ����� �������������� */
#define FOCUS( cx, cy, rx, ry, rw, rh ) ( cx >= rx && cy >= ry && cx < rx + rw && cy < ry + rh )

/* �������� ������ */
#define STRCMP( str1, str2 ) ( strlen( str1 ) == strlen( str2 ) && !strncmp( str1, str2, strlen( str1 ) ) )

/* ������� �������� */
#define MINIMUM( a, b ) ((a<b)?a:b)

/* �������� �������� */
#define MAXIMUM( a, b ) ((a>b)?a:b)

/* ����� ���������� assert */
#define COMPILE_TIME_ASSERT( x )

/* ��������� ������� ������� �� ����� ���������� */
#ifndef NDEBUG
#  define COMPILE_TIME_ASSERT_SYMBOL( s ) COMPILE_TIME_ASSERT( sizeof s )
#else
#  define COMPILE_TIME_ASSERT_SYMBOL( s )
#endif

/* ����������� ������������� ������� */
#ifndef WIN32
void strlwr( char *string);
#endif

/* ���������� ���������� ��������� ������� �� ������� ������ */
int count_characters( const char *str, char character );

/** ���������� ������� ���� � ������ � ������ ��������, ����� � ������ � ������� ��������.
 * ���� ��� ������ �������, ������� NULL. ��� ���� ��������� ����������� �
 * ���������� �������, ���������� ������ ��� ����� ���������� ������� '/'. */
FILE *fopen_ic( char *path, const char *mode );

/* ascii-���� ������� �������� */
enum GameSymbols {
    CharDunno1 = 1,	/* �� ���� (���?) */
    CharDunno2 = 2,	/* �� ���� (���?) */
    CharStrength = 3,
    CharFuel = 4,
    CharAmmo = 5,
    CharEntr = 6,
    CharBack = 17,	/* �� ����� ���� ������� �� ���� (�������?) */
    CharDistance = 26,
    CharNoExp = 128,
    CharExpGrowth = CharNoExp,
    CharExp = 133,
    CharStar = 134,
    CharCheckBoxEmpty = 136,
    CharCheckBoxEmptyFocused = 137,
    CharCheckBoxChecked = 138,
    CharCheckBoxCheckedFocused = 139,
};

#define GS_STRENGTH "\003"
#define GS_FUEL "\004"
#define GS_AMMO "\005"
#define GS_ENTR "\006"
#define GS_BACK "\021"
#define GS_DISTANCE "\032"
#define GS_NOEXP "\200"
#define GS_EXP "\205"
#define GS_STAR "\206"
#define GS_CHECK_BOX_EMPTY "\210"
#define GS_CHECK_BOX_EMPTY_FOCUSED "\211"
#define GS_CHECK_BOX_CHECKED "\212"
#define GS_CHECK_BOX_CHECKED_FOCUSED "\213"

/* ��������� �������� */
typedef struct {
    int limit;
    int cur;
} Delay;

/* ���������� �������� � �� ������������ */
void set_delay( Delay *delay, int ms );

/* �������� ������ (cur = 0)*/
void reset_delay( Delay *delay );

/* ���������, ����� �� ����� (�������� ������������) � �������� */
int timed_out( Delay *delay, int ms );

/* ���������� �������� ����� ��������� �� ����� */
int get_dist( int x1, int y1, int x2, int y2 );

/* ��������� �������� ��������� ������������������ �������������, � ������� ftime */
void set_random_seed();

/* �������� ���������� �� ������ */
void get_coord( const char *str, int *x, int *y );

/** ��������� ������ */
typedef struct {
    char **lines;
    int count;
} Text;
/** ������������� ������ � ����� (��� ������) */
Text* create_text( struct _Font *fnt, const char *str, int width );
/** ������� ����� */
void delete_text( Text *text );

/*
====================================================================
������� ������ ����� � ���������� ��� � ������� 0.
====================================================================
*/
void delete_string_list( char ***list, int *count );

/*
====================================================================
����� ��������� �������������� �� ������ � ������� ������ ����
������ ������������.
====================================================================
*/
typedef struct { char *string; int flag; } StrToFlag;
/*
====================================================================
��� ������� ���������, ����������� �� 'name' � fct � ���������� ����
��� 0, ���� �� ������.
====================================================================
*/
int check_flag( const char *name, StrToFlag *fct, int *NumberInArray );

/*
====================================================================
�������� ���������� �������� ������ �� ������� ������� � ��������������� �� 0 �� 5.
====================================================================
*/
int get_close_hex_pos( int x, int y, int id, int *dest_x, int *dest_y );

/*
====================================================================
���������, ������ �� ��� ������� ���� � �����.
====================================================================
*/
int is_close( int x1, int y1, int x2, int y2 );

/*
====================================================================
���������� �������� � dest � ��� ������������ ����������� ��������. ��������� � 0.
====================================================================
*/
void strcpy_lt( char *dest, const char *src, int limit );

/*
====================================================================
���������� ������� ��� ������� �����.
====================================================================
*/
const char *get_basename(const char *filename);

/*
====================================================================
������� ����� �� ��������� ��������������� ������. ���� �� ����������,
��������� ����� �� ����� �����.
����� ��������� � ����.
====================================================================
*/
char *determine_domain(struct PData *tree, const char *filename);

/*
====================================================================
������� �������, � ������� ����������� ������ ����.
====================================================================
*/
const char *get_gamedir(void);

#endif
