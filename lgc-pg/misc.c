/***************************************************************************
                          tools.c  -  description
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

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "misc.h"
#include "shp.h"
#include "util/paths.h"

extern char *source_path;
extern char *dest_path;

/* сравнивается со строками и возвращает истину, если их первые символы strlen (str1) равны */
inline int equal_str( char *str1, char *str2 )
{
    if ( strlen( str1 ) != strlen( str2 ) ) return 0;
    return ( !strncmp( str1, str2, strlen( str1 ) ) );
}

/* установить задержку в мс миллисекунды */
inline void set_delay( Delay *delay, int ms )
{
    delay->limit = ms;
    delay->cur = 0;
}

/* задержка сброса ( cur = 0 )*/
inline void reset_delay( Delay *delay )
{
    delay->cur = 0;
}

/* проверить, истекло ли время и сбросить */
inline int timed_out( Delay *delay, int ms )
{
    delay->cur += ms;
    if ( delay->cur >= delay->limit ) {

        delay->cur = 0;
        return 1;

    }
    return 0;
}


inline void goto_tile( int *x, int *y, int d )
{
    /*  0 - вверх, по часовой стрелке, 5 - влево вверх */
    switch ( d ) {

        case 1:
            if ( !( (*x) & 1 ) )
                (*y)--;
            (*x)++;
            break;
        case 2:
            if ( (*x) & 1 )
                (*y)++;
            (*x)++;
            break;
        case 4:
            if ( (*x) & 1 )
                (*y)++;
            (*x)--;
            break;
        case 5:
            if ( !( (*x) & 1 ) )
                (*y)--;
            (*x)--;
            break;

    }
}

/* вернуть расстояние между позициями на карте */
int get_dist( int x1, int y1, int x2, int y2 )
{
    int range = 0;

    while ( x1 != x2 || y1 != y2 ) {

        /* подход к x2,y2 */
        /*  0 - вверх, по часовой стрелке, 5 - влево вверх */
        if ( y1 < y2 ) {

            if ( x1 < x2 )
                goto_tile( &x1, &y1, 2 );
            else
                if ( x1 > x2 )
                    goto_tile( &x1, &y1, 4 );
                else
                    y1++;

        }
        else
            if ( y1 > y2 ) {

                if ( x1 < x2 )
                    goto_tile( &x1, &y1, 1 );
                else
                    if ( x1 > x2 )
                        goto_tile( &x1, &y1, 5 );
                    else
                        y1--;

            }
            else {

                if ( x1 < x2 )
                    x1++;
                else
                    if ( x1 > x2 )
                        x1--;

            }

        range++;
    }

    return range;
}

/* инициировать случайное семя с помощью ftime */
void set_random_seed()
{
    srand( (unsigned int)time( 0 ) );
}

/* получить координаты из строки */
void get_coord( char *str, int *x, int *y )
{
    int i;
    char *cur_arg = 0;

    *x = *y = 0;

    /* получить позицию запятой */
    for ( i = 0; i < strlen( str ); i++ )
        if ( str[i] == ',' ) break;
    if ( i == strlen( str ) ) {
        fprintf( stderr, "get_coord: no comma found in pair of coordinates '%s'\n", str );
        return; /* запятая не найдена */
    }

    /* y */
    cur_arg = str + i + 1;
    if ( cur_arg[0] == 0 )
        fprintf( stderr, "get_coord: warning: y-coordinate is empty (maybe you left a space between x and comma?)\n" );
    *y = atoi( cur_arg );
    /* x */
    cur_arg = strdup( str ); cur_arg[i] = 0;
    *x = atoi( cur_arg );
    FREE( cur_arg );
}

/* заменить new_lines пробелами в тексте */
void repl_new_lines( char *text )
{
    int i;
    for ( i = 0; i < strlen( text ); i++ )
        if ( text[i] < 32 )
            text[i] = 32;
}

// преобразовать строку в текст (для списка) //
// ширина символа - это ширина строки в символах //
Text* create_text( char *orig_str, int char_width )
{
    int i, j;
    int pos;
    int last_space;
    int new_line;
    Text *text = 0;
    char *str = 0;

    text = calloc ( 1, sizeof( Text ) );

    // возможно, orig_str - постоянное выражение; дубликат для безопасности //
    str = strdup( orig_str );

    // заменить исходные new_lines пробелами //
    repl_new_lines( str );

    /* измените некоторые пробелы на new_lines, чтобы новый текст соответствовал требуемой line_length * /
    / * ПРИМЕЧАНИЕ: '#' означает новую_строку! * /
    / * если символ с 0, это всего лишь одна строка */
    if ( char_width > 0 ) {
        pos = 0;
        while ( pos < strlen( str ) ) {
            last_space = 0;
            new_line = 0;
            i = 0;
            while ( !new_line && i < char_width && i + pos < strlen( str ) ) {
                switch ( str[pos + i] ) {
                    case '#': new_line = 1;
                    case 32: last_space = i; break;
                }
                i++;
            }
            if ( i + pos >= strlen( str ) ) break;
            if ( last_space == 0 ) {
                /* эээ ... слишком долго, придется слово разрезать на части ... */
                last_space = char_width / 2;
            }
            str[pos + last_space] = 10;
            pos += last_space;
        }
    }

    /* считать строки */
    if ( char_width > 0 ) {
        for ( i = 0; i < strlen( str ); i++ )
            if ( str[i] == 10 )
                text->count++;
        /* возможно одна незаконченная строка */
        if ( str[strlen( str ) - 1] != 10 )
            text->count++;
    }
    else
        text->count = 1;

    /* получить память */
    text->lines = calloc( text->count, sizeof( char* ) );

    pos = 0;
    /* получить все строки */
    for ( j = 0; j < text->count; j++ ) {
        i = 0;
        while ( pos + i < strlen( str ) && str[pos + i] != 10 ) i++;
        text->lines[j] = calloc( i + 1, sizeof( char ) );
        strncpy( text->lines[j], str + pos, i );
        text->lines[j][i] = 0;
        pos += i; pos++;
    }

    if ( text->count == 0 )
        fprintf( stderr, "conv_to_text: warning: line_count is 0\n" );

    free( str );

    return text;
}

// удалить текст //
void delete_text( Text *text )
{
    int i;

    if ( text == 0 ) return;

    if ( text->lines ) {
        for ( i = 0; i < text->count; i++ )
            if ( text->lines[i] )
                free( text->lines[i] );
        free( text->lines );
    }
    free( text );
}

/*
====================================================================
Удалите массив строк и установите его и счетчик 0.
====================================================================
*/
void delete_string_list( char ***list, int *count )
{
    int i;
    if ( *list == 0 ) return;
    for ( i = 0; i < *count; i++ )
        if ( (*list)[i] ) free( (*list)[i] );
    free( *list );
    *list = 0; *count = 0;
}

/*
====================================================================
Эта функция проверяет, встречается ли 'name' в fct и возвращает флаг
или 0, если не найден.
====================================================================
*/
int check_flag( char *name, StrToFlag *fct )
{
    /* получить флаги */
    int i = 0;
    while ( fct[i].string[0] != 'X' ) {
        if ( STRCMP( fct[i].string, name ) )
            return fct[i].flag;
        i++;
    }
    if ( !STRCMP( "none", name ) ) 
        fprintf( stderr, "%s: unknown flag\n", name );
    return 0;
}

/*
====================================================================
Скопируйте источник в dest и на максимальное количество символов. Завершить с 0.
====================================================================
*/
void strcpy_lt( char *dest, char *src, int limit )
{
    int len = strlen( src );
    if ( len > limit ) {
        strncpy( dest, src, limit );
        dest[limit] = 0;
    }
    else
        strcpy( dest, src );
}

/*
====================================================================
Вернуть (указатель на статический дубликат) строку в нижнем регистре.
====================================================================
*/
char *strlower( const char *str ) {
    int i = strlen(str); 
    static char lstr[512];
    
    if (i>=512)
        i = 511; /* усечь до размера статического буфера */
    
    lstr[i] = 0;
    for ( ; i > 0; ) {
        i--;
        lstr[i] = str[i];
        if ( str[i] >= 'A' && str[i] <= 'Z' )
            lstr[i] = str[i] + 32;
    }
    return lstr;
}

/*
====================================================================
Копировать файл (copy_ic игнорирует регистр @sname)
====================================================================
*/
void copy_ic( char *sname, char *dname )
{
    int size;
    char *buffer;
    FILE *source, *dest;
    if ( ( source = fopen_ic( sname, "r" ) ) == 0 ){
        fprintf( stderr, "%s: file not found\n", sname );
        return;
    }
    if ( ( dest = fopen( dname, "w" ) ) == 0 ) {
        fprintf( stderr, "%s: write access denied\n", dname );
        return;
    }
    fseek( source, 0, SEEK_END ); 
    size = ftell( source );
    fseek( source, 0, SEEK_SET );
    buffer = calloc( size, sizeof( char ) );
    fread( buffer, sizeof( char ), size, source );
    fwrite( buffer, sizeof( char ), size, dest );
    free( buffer );
    fclose( source );
    fclose( dest );
}
void copy( char *sname, char *dname )
{
    int size;
    char *buffer;
    FILE *source, *dest;
    if ( ( source = fopen( sname, "r" ) ) == 0 ){
        fprintf( stderr, "%s: file not found\n", sname );
        return;
    }
    if ( ( dest = fopen( dname, "w" ) ) == 0 ) {
        fprintf( stderr, "%s: write access denied\n", dname );
        return;
    }
    fseek( source, 0, SEEK_END ); 
    size = ftell( source );
    fseek( source, 0, SEEK_SET );
    buffer = calloc( size, sizeof( char ) );
    fread( buffer, sizeof( char ), size, source );
    fwrite( buffer, sizeof( char ), size, dest );
    free( buffer );
    fclose( source );
    fclose( dest );
}
int copy_pg_bmp( char *src, char *dest )
{
    SDL_Surface *surf = 0;
    char path[MAXPATHLEN];
    snprintf( path, MAXPATHLEN, "%s/convdata/%s", get_gamedir(), src );
    if ( ( surf = SDL_LoadBMP( path ) ) == 0 ) {
        fprintf( stderr, "%s: %s\n", path, SDL_GetError() );
        return 0;
    }
    snprintf( path, MAXPATHLEN, "%s/gfx/units/%s", dest_path, dest );
    if ( SDL_SaveBMP( surf, path ) != 0 ) {
        fprintf( stderr, "%s: %s\n", path, SDL_GetError() );
        return 0;
    }
    SDL_FreeSurface( surf );
    return 1;
}

/*
====================================================================
Вернуть путь к установочному каталогу как статическую строку (не поток
сейф).
====================================================================
*/
const char *get_gamedir(void)
{
#ifndef INSTALLDIR
    return ".";
#else
    static char gamedir[MAXPATHLEN];
    snprintf( gamedir, MAXPATHLEN, "%s", INSTALLDIR );
    return gamedir;
#endif
}

/** Попробуйте открыть файл с именем в нижнем регистре, затем с именем в верхнем регистре.
 * Если оба терпят неудачу, верните NULL. Сам путь считается находящимся в
 * правильный регистр, изменяется только имя после последнего символа '/'. */
FILE *fopen_ic( const char *_path, const char *mode )
{
	FILE *file = NULL;
	char path[strlen(_path)+1], *start, *ptr;
	
	strcpy(path,_path); /* we need to copy since we modify it */
	
	/* начать за разделителем файлов */
	if ((start = strrchr(path,'/')) == NULL) /* Linux */
		start = strrchr(path,'\\'); /* Windows */
	if (start)
		start++;
	else	
		start = path; /* только имя файла */
	
	/* попробуйте все строчные буквы */
	for (ptr = start; *ptr != 0; ptr++)
		*ptr = tolower(*ptr);
	if ((file = fopen(path,mode)))
		return file;
	
	/* попробуйте первый верхний регистр */
	start[0] = toupper(start[0]);
	if ((file = fopen(path,mode)))
		return file;
	
	/* попробуйте все в верхнем регистре */
	for (ptr = start + 1; *ptr != 0; ptr++)
		*ptr = toupper(*ptr);
	if ((file = fopen(path,mode)))
		return file;
	
	return NULL;
}

/*
====================================================================
Копировать в dest из источника с горизонтальным зеркальным отображением.
====================================================================
*/
void copy_surf_mirrored( SDL_Surface *source, SDL_Rect *srect, SDL_Surface *dest, SDL_Rect *drect )
{
    int mirror_i, i, j;
    for ( j = 0; j < srect->h; j++ )
        for ( i = 0, mirror_i = drect->x + drect->w - 1; i < srect->w; i++, mirror_i-- )
            set_pixel( dest, mirror_i, j + drect->y, get_pixel( source, i + srect->x, j + srect->y ) );
}


