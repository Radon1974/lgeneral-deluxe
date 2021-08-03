/***************************************************************************
                          misc.c  -  description
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

#include "misc.h"
#include "parser.h"
#include "localize.h"
#include "paths.h"
#include "sdl.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern int map_w, map_h; /* FIX ME! */

/* сравнивается со строками и возвращает истину, если их первые символы strlen (str1) равны */
int equal_str( const char *str1, const char *str2 )
{
    if ( strlen( str1 ) != strlen( str2 ) ) return 0;
    return ( !strncmp( str1, str2, strlen( str1 ) ) );
}

/* установить задержку в мс миллисекунды */
void set_delay( Delay *delay, int ms )
{
    delay->limit = ms;
    delay->cur = 0;
}

/* задержка сброса ( cur = 0 )*/
void reset_delay( Delay *delay )
{
    delay->cur = 0;
}

/* проверить, истекло ли время и сбросить */
int timed_out( Delay *delay, int ms )
{
    delay->cur += ms;
    if ( delay->cur >= delay->limit ) {

        delay->cur = 0;
        return 1;

    }
    return 0;
}

/* Определение нестандартной функции */
#ifndef WIN32
void strlwr( char *string)
{
    int i;
    for ( i = 0; i < strlen( string ); i++ )
    {
        string[i] = tolower( string[i] );
    }
}
#endif
/* Подсчитать количество вхождений символа во входной строке */
int count_characters( const char *str, char character )
{
    const char *p = str;
    int count = 0;

    do
    {
        if (*p == character)
            count++;
    } while (*(p++));

    return count;
}

/** Попробуйте открыть файл с именем в нижнем регистре, затем с именем в верхнем регистре.
 * Если оба терпят неудачу, верните NULL. Считается, что сам путь находится в
 * правильный регистр, изменяется только имя после последнего символа '/'. */
FILE *fopen_ic( char *path, const char *mode )
{
	FILE *file = NULL;
	char //path[strlen(_path)+1],
     *start, *ptr;

//	strcpy(path,_path); /* нам нужно скопировать, так как мы его изменяем */

	/* попробуйте точное имя */
	if ((file = fopen(path,mode)))
		return file;

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

/* Преобразуйте координаты сетки в изометрические (диагональные) координаты. */
static void convert_coords_to_diag( int *x, int *y )
{
  *y += (*x + 1) / 2;
}

/* вернуть расстояние между позициями на карте */
int get_dist( int x0, int y0, int x1, int y1 )
{
    int dx, dy;
    convert_coords_to_diag( &x0, &y0 );
    convert_coords_to_diag( &x1, &y1 );
    dx = abs(x1 - x0);
    dy = abs(y1 - y0);
    if ( (y1 <= y0 && x1 >= x0) || (y1 > y0 && x1 < x0) )
        return dx + dy;
    else if (dx > dy)
        return dx;
    else
        return dy;
}

/* начальное значение случайной последовательности инициализации, с помощью ftime */
void set_random_seed()
{
    srand( (unsigned int)time( 0 ) );
}

/* получить координаты из строки */
void get_coord( const char *str, int *x, int *y )
{
    int i;
    char *cur_arg;

    *x = *y = 0;

    /* получить позицию запятой */
    for ( i = 0; i < strlen( str ); i++ )
        if ( str[i] == ',' ) break;
    if ( i == strlen( str ) ) {
        fprintf( stderr, tr("get_coord: no comma found in pair of coordinates '%s'\n"), str );
        return; /* запятая не найдена */
    }

    /* y */
    cur_arg = (char *)str + i + 1;
    if ( cur_arg[0] == 0 )
        fprintf( stderr, tr("get_coord: warning: y-coordinate is empty (maybe you left a space between x and comma?)\n") );
    *y = atoi( cur_arg );
    /* x */
    cur_arg = strdup( str ); cur_arg[i] = 0;
    *x = atoi( cur_arg );
    FREE( cur_arg );
}

/* замените new_lines пробелами в тексте */
void repl_new_lines( char *text )
{
    int i;
    for ( i = 0; i < strlen( text ); i++ )
        if ( (unsigned char)text[i] < 32 )
            text[i] = 32;
}

/** вернуть 1, если ch - символ разрыва строки */
static int text_is_linebreak(char ch)
{
    return ch == '#';
}

/** вернуть 1, если текст можно разбить непосредственно перед концом */
static int text_is_breakable(const char *begin, const char *end)
{
    return begin != end
            && (end[-1] == '\t' || end[-1] == '\n' || end[-1] == ' '
                || end[-1] == '-');
}

/** временные данные для создания структуры текстового объекта */
struct TextData {
    Text *text;
    int reserved;
};

/**
 * Добавляет указанный диапазон, как линии, чтобы структура текстового объекта,
 * выполнение текстовых преобразований по мере необходимости
 */
static void text_add_line(struct TextData *td, const char *begin, const char *end)
{
    int idx = td->text->count;
    char *out;
    if (idx == td->reserved) {
        if (td->reserved == 0) td->reserved = 1;
        td->reserved *= 2;
        td->text->lines = realloc(td->text->lines, td->reserved * sizeof td->text->lines[0]);
    }

    out = td->text->lines[idx] = malloc(end - begin + 1);
    for (; begin != end; ++begin, ++out) {
        switch (*begin) {
            case '\n':
            case '\t':
            case '\r':
            case '#': *out = ' '; break;
            default: *out = *begin; break;
        }
    }
    *out = '\0';

    td->text->count++;
}

/**
 * преобразование str в текст ( для listbox )
 * ширина-это доступная ширина линии в пикселях.
 */
Text* create_text( struct _Font *fnt, const char *orig_str, int width )
{
    struct TextData td;
    const char *line_start = orig_str;
    const char *head = orig_str;
    const char *committed = orig_str;
    int cumulated_width = 0;	/* ширина текста в этой строке */
    int break_line = 0;
    if (width < 0) width = 0;

    memset(&td, 0, sizeof(td));
    td.text = calloc ( 1, sizeof( Text ) );

    while (*committed) {
        int ch_width = char_width(fnt, *head);

        if (committed != head && text_is_linebreak(head[-1]))
            break_line = 1;
        else if (cumulated_width > width) {
            /* если слово слишком длинное, чтобы поместиться в одну строку,
* принудительно сделайте разрыв строки в текущей позиции.
             */
            if (committed == line_start)
                /* уберите последний символ (если только один) и сломайте там */
                committed = head - (head - 1 != line_start);
            head = committed;
            break_line = 1;
        }
        else if (text_is_breakable(committed, head))
            committed = head;

        if (!break_line) {
            cumulated_width += ch_width;
            head++;
        }

        if (!*head) break_line = 1;

        if (break_line) {
            text_add_line(&td, line_start, head);
            line_start = committed = head;
            cumulated_width = 0;
            break_line = 0;
        }
    }

    if (!td.text->lines) text_add_line(&td, "", "" + 1);

    return td.text;
}

/** удалить текст */
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
int check_flag( const char *name, StrToFlag *fct, int *NumberInArray )
{
    /* получить флаги */
    *NumberInArray = 0;
    while ( fct[*NumberInArray].string[0] != 'X' ) {
        if ( STRCMP( fct[*NumberInArray].string, name ) )
            return fct[*NumberInArray].flag;
        (*NumberInArray)++;
    }
    if ( !STRCMP( "none", name ) )
        fprintf( stderr, tr("%s: unknown flag\n"), name );
    return 0;
}

/*
====================================================================
Получите координаты соседних плиток по часовой стрелке с идентификатором от 0 до 5.
====================================================================
*/
int get_close_hex_pos( int x, int y, int id, int *dest_x, int *dest_y )
{
    if ( id == 0 ) {
        *dest_x = x;
        *dest_y = y - 1;
    }
    else
    if ( id == 1 ) {
        if ( x & 1 ) {
            *dest_x = x + 1;
            *dest_y = y;
        }
        else {
            *dest_x = x + 1;
            *dest_y = y - 1;
        }
    }
    else
    if ( id == 2 ) {
        if ( x & 1 ) {
            *dest_x = x + 1;
            *dest_y = y + 1;
        }
        else {
            *dest_x = x + 1;
            *dest_y = y;
        }
    }
    else
    if ( id == 3 ) {
        *dest_x = x;
        *dest_y = y + 1;
    }
    else
    if ( id == 4 ) {
        if ( x & 1 ) {
            *dest_x = x - 1;
            *dest_y = y + 1;
        }
        else {
            *dest_x = x - 1;
            *dest_y = y;
        }
    }
    else
    if ( id == 5 ) {
        if ( x & 1 ) {
            *dest_x = x - 1;
            *dest_y = y;
        }
        else {
            *dest_x = x - 1;
            *dest_y = y - 1;
        }
    }
	if ( *dest_x <= 0 || *dest_y <= 0 || *dest_x >= map_w-1 ||
							*dest_y >= map_h-1 )
		return 0;
    return 1;
}

/*
====================================================================
Проверьте, соседствуют ли эти позиции друг с другом.
====================================================================
*/
int is_close( int x1, int y1, int x2, int y2 )
{
    int i, next_x, next_y;
    if ( x1 == x2 && y1 == y2 ) return 1;
    for ( i = 0; i < 6; i++ )
        if ( get_close_hex_pos( x1, y1, i, &next_x, &next_y ) )
            if ( next_x == x2 && next_y == y2 )
                return 1;
    return 0;
}

/*
====================================================================
Скопируйте источник в dest и на максимальное количество символов. Завершить с 0.
====================================================================
*/
void strcpy_lt( char *dest, const char *src, int limit )
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
Возвращает базовое имя данного файла.
====================================================================
*/
const char *get_basename(const char *filename)
{
    const char *pos = strrchr(filename, '/');
    if (pos) return pos + 1;
    return filename;
}

/*
====================================================================
Вернуть домен из заданного синтаксического дерева. Если не содержится,
построить домен из имени файла.
Будет размещено в куче.
====================================================================
*/
char *determine_domain(struct PData *tree, const char *filename)
{
    char *domain;
    char *pos;
    if ( parser_get_string(tree, "domain", &domain) ) return domain;

    domain = strdup(get_basename(filename));
    pos = strchr(domain, '.');
    if (pos) *pos = 0;
    return domain;
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
