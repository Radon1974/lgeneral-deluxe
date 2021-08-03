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

/* проверьте, нечетное ли число или четное */
#define ODD( x ) ( x & 1 )
#define EVEN( x ) ( !( x & 1 ) )

/* бесплатно с чеком */
#define FREE( ptr ) { if ( ptr ) free( ptr ); ptr = 0; }

/* проверьте, установлен ли в источнике серьезный флаг */
#define CHECK_FLAGS( source, flags ) ( source & (flags) )

/* вернуть случайное значение между верхним и нижним пределом (включительно) */
#define RANDOM( lower, upper ) ( ( rand() % ( ( upper ) - ( lower ) + 1 ) ) + ( lower ) )
#define DICE(maxeye) (1+(int)(((double)maxeye)*rand()/(RAND_MAX+1.0)))

/* проверьте, находится ли внутри этого прямоугольника */
#define FOCUS( cx, cy, rx, ry, rw, rh ) ( cx >= rx && cy >= ry && cx < rx + rw && cy < ry + rh )

/* сравнить строки */
#define STRCMP( str1, str2 ) ( strlen( str1 ) == strlen( str2 ) && !strncmp( str1, str2, strlen( str1 ) ) )

/* минимум возврата */
#define MINIMUM( a, b ) ((a<b)?a:b)

/* максимум возврата */
#define MAXIMUM( a, b ) ((a>b)?a:b)

/* время компиляции assert */
#define COMPILE_TIME_ASSERT( x )

/* проверить наличие символа во время компиляции */
#ifndef NDEBUG
#  define COMPILE_TIME_ASSERT_SYMBOL( s ) COMPILE_TIME_ASSERT( sizeof s )
#else
#  define COMPILE_TIME_ASSERT_SYMBOL( s )
#endif

/* Определение нестандартной функции */
#ifndef WIN32
void strlwr( char *string);
#endif

/* Подсчитать количество вхождений символа во входной строке */
int count_characters( const char *str, char character );

/** Попробуйте открыть файл с именем в нижнем регистре, затем с именем в верхнем регистре.
 * Если оба терпят неудачу, верните NULL. Сам путь считается находящимся в
 * правильный регистр, изменяется только имя после последнего символа '/'. */
FILE *fopen_ic( char *path, const char *mode );

/* ascii-коды игровых символов */
enum GameSymbols {
    CharDunno1 = 1,	/* не знаю (паз?) */
    CharDunno2 = 2,	/* не знаю (паз?) */
    CharStrength = 3,
    CharFuel = 4,
    CharAmmo = 5,
    CharEntr = 6,
    CharBack = 17,	/* на самом деле понятия не имею (кулькан?) */
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

/* структура задержки */
typedef struct {
    int limit;
    int cur;
} Delay;

/* установить задержку в мс миллисекунды */
void set_delay( Delay *delay, int ms );

/* задержка сброса (cur = 0)*/
void reset_delay( Delay *delay );

/* проверьте, истек ли время (добавьте миллисекунды) и сбросьте */
int timed_out( Delay *delay, int ms );

/* расстояние возврата между позициями на карте */
int get_dist( int x1, int y1, int x2, int y2 );

/* начальное значение случайной последовательности инициализации, с помощью ftime */
void set_random_seed();

/* получить координаты из строки */
void get_coord( const char *str, int *x, int *y );

/** структура текста */
typedef struct {
    char **lines;
    int count;
} Text;
/** преобразовать строку в текст (для списка) */
Text* create_text( struct _Font *fnt, const char *str, int width );
/** удалить текст */
void delete_text( Text *text );

/*
====================================================================
Удалите массив строк и установите его и счетчик 0.
====================================================================
*/
void delete_string_list( char ***list, int *count );

/*
====================================================================
Чтобы упростить преобразование из строки в таблицы флагов этих
записи используются.
====================================================================
*/
typedef struct { char *string; int flag; } StrToFlag;
/*
====================================================================
Эта функция проверяет, встречается ли 'name' в fct и возвращает флаг
или 0, если не найден.
====================================================================
*/
int check_flag( const char *name, StrToFlag *fct, int *NumberInArray );

/*
====================================================================
Получите координаты соседних плиток по часовой стрелке с идентификатором от 0 до 5.
====================================================================
*/
int get_close_hex_pos( int x, int y, int id, int *dest_x, int *dest_y );

/*
====================================================================
Проверьте, близки ли эти позиции друг к другу.
====================================================================
*/
int is_close( int x1, int y1, int x2, int y2 );

/*
====================================================================
Скопируйте источник в dest и при максимальном ограничении символов. Завершите с 0.
====================================================================
*/
void strcpy_lt( char *dest, const char *src, int limit );

/*
====================================================================
Возвращает базовое имя данного файла.
====================================================================
*/
const char *get_basename(const char *filename);

/*
====================================================================
Вернуть домен из заданного синтаксического дерева. Если не содержится,
построить домен из имени файла.
Будет размещено в куче.
====================================================================
*/
char *determine_domain(struct PData *tree, const char *filename);

/*
====================================================================
Верните каталог, в котором установлены данные игры.
====================================================================
*/
const char *get_gamedir(void);

#endif
