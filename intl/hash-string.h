/* Implements a string hashing function.
   Copyright (C) 1995, 1997 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* @@ end of prolog @@ */

#ifndef PARAMS
# if __STDC__
#  define PARAMS(Args) Args
# else
#  define PARAMS(Args) ()
# endif
#endif

/* Мы предполагаем, что у него есть значение unsigned long int не менее 32 бит.  */
#define HASHWORDBITS 32


/* Определяет так называемую функцию `hashpjw 'П. Дж. Вайнбергера
   [см. Ахо / Сетхи / Уллман, КОМПИЛЯТОРЫ: принципы, методы и инструменты,
   1986, 1987 Bell Telephone Laboratories, Inc.]  */
static unsigned long hash_string PARAMS ((const char *__str_param));

static  unsigned long
hash_string (str_param)
     const char *str_param;
{
  unsigned long int hval, g;
  const char *str = str_param;

  /* Вычислить хеш-значение для данной строки.  */
  hval = 0;
  while (*str != '\0')
    {
      hval <<= 4;
      hval += (unsigned long) *str++;
      g = hval & ((unsigned long) 0xf << (HASHWORDBITS - 4));
      if (g != 0)
	{
	  hval ^= g >> (HASHWORDBITS - 8);
	  hval ^= g;
	}
    }
  return hval;
}
