/***************************************************************************
                          date.h  -  description
                             -------------------
    begin                : Sat Jan 20 2001
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

#ifndef __DATE_H
#define __DATE_H

/* типы дат */
enum {
    DIGIT_DATE       = 0,
    FULL_NAME_DATE   = 1,
    SHORT_NAME_DATE  = 2
};

/* структура даты */
typedef struct {
    int day;
    int month;
    int year;
} Date;

/* преобразует строку в дату */
void str_to_date( Date *date, char *str );

/* преобразует дату в строку */
void date_to_str( char *str, Date date, int type );

/* добавьте количество дней до даты и скорректируйте его */
void date_add_days( Date *date, int days );

/* Получить текущую дату в формате DD.MM.YYYY */
void currentDateTime( char *currentDate );

#endif
