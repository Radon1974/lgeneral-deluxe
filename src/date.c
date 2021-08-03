/***************************************************************************
                          date.c  -  description
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

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "date.h"
#include "misc.h"
#include "localize.h"

/* продолжительность месяца */
int month_length[] = {
    31,
    28,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31
};

/* полные названия месяцев */
const char full_month_names[][10] = {
    TR_NOOP("January"),
    TR_NOOP("February"),
    TR_NOOP("March"),
    TR_NOOP("April"),
    TR_NOOP("May_long"),
    TR_NOOP("June"),
    TR_NOOP("July"),
    TR_NOOP("August"),
    TR_NOOP("September"),
    TR_NOOP("October"),
    TR_NOOP("November"),
    TR_NOOP("December")
};

/* короткие названия месяцев */
const char short_month_names[][4] = {
    TR_NOOP("Jan"),
    TR_NOOP("Feb"),
    TR_NOOP("Mar"),
    TR_NOOP("Apr"),
    TR_NOOP("May"),
    TR_NOOP("Jun"),
    TR_NOOP("Jul"),
    TR_NOOP("Aug"),
    TR_NOOP("Sep"),
    TR_NOOP("Oct"),
    TR_NOOP("Nov"),
    TR_NOOP("Dec")
};

/* преобразует строку в дату */
void str_to_date( Date *date, char *str )
{
    int i;
    char aux_str[12];
    memset( aux_str, 0, sizeof( char ) * 12 );
    // день
    for ( i = 0; i < strlen( str ); i++ )
        if ( str[i] == '.' ) {
            strncpy( aux_str, str, i);
            date->day = atoi( aux_str );
            break;
        }
    str = str + i + 1;
    // месяц
    for ( i = 0; i < strlen( str ); i++ )
        if ( str[i] == '.' ) {
            strncpy( aux_str, str, i);
            date->month = atoi( aux_str ) - 1;
            break;
        }
    str = str + i + 1;
    // год
    date->year = atoi( str );
}

/* преобразует дату в строку */
void date_to_str( char *str, Date date, int type )
{
    switch ( type ) {
        case DIGIT_DATE:
            sprintf(str, "%i.%i.%i", date.day, date.month, date.year );
            break;
        case FULL_NAME_DATE:
            sprintf(str, "%i. %s %i", date.day, tr(full_month_names[date.month]), date.year );
            break;
        case SHORT_NAME_DATE:
            sprintf(str, "%i. %s %i", date.day, tr(short_month_names[date.month]), date.year );
            break;
    }
}

/* добавьте количество дней к дате и отрегулируйте его */
void date_add_days( Date *date, int days )
{
    /* бедный-человек день сумматор. Добавьте только самое низкое количество дней, чтобы обеспечить
     * что за один проход происходит не более одного месячного обертывания.
     */
    for (; days > 0; days -= month_length[(date->month + 11) % 12]) {
        date->day += MINIMUM(days, month_length[date->month]);
        if ( date->day > month_length[date->month] ) {
            date->day = date->day - month_length[date->month];
            date->month++;
            if ( date->month == 12 ) {
                date->month = 0;
                date->year++;
            }
        }
    }
}

/* Получить текущую дату, формат DD.MM.YYYY */
void currentDateTime( char *currentDate )
{
    time_t now;
    struct tm *tstruct;
    now = time(NULL);
    tstruct = localtime(&now);
    strftime( currentDate, 15, "%d.%m.%Y", tstruct );
}

