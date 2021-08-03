/* Internal header for GNU gettext internationalization functions.
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

#ifndef _GETTEXT_H
#define _GETTEXT_H 1

#include <stdio.h>

#if HAVE_LIMITS_H || _LIBC
# include <limits.h>
#endif

/* @@ конец пролога @@ */

/* Магический номер формата каталога сообщений GNU.  */
#define _MAGIC 0x950412de
#define _MAGIC_SWAPPED 0xde120495

/* Номер версии используемого в настоящее время (двоичного) формата файла .mo.  */
#define MO_REVISION_NUMBER 0

/* Следующие изменения представляют собой попытку использовать препроцессор C
   для определения целочисленного типа без знака шириной 32 бита. An
   альтернативный подход - использовать макрос AC_CHECK_SIZEOF autoconf, но
   для этого потребуется, чтобы скрипт конфигурации компилировался и * запускался *
   получившийся исполняемый файл. Локальный запуск кросс-скомпилированных исполняемых файлов
   обычно это невозможно.  */

#if __STDC__
# define UINT_MAX_32_BITS 4294967295U
#else
# define UINT_MAX_32_BITS 0xFFFFFFFF
#endif

/* Если UINT_MAX не определен, предположим, что это 32-битный тип.
   Это должно быть справедливо для всех систем, о которых заботится GNU, потому что
   что не включает 16-битные системы, а только современные системы
   (которые, безусловно, имеют <limits.h>) имеют 64 + -битные целочисленные типы.  */

#ifndef UINT_MAX
# define UINT_MAX UINT_MAX_32_BITS
#endif

#if UINT_MAX == UINT_MAX_32_BITS
typedef unsigned nls_uint32;
#else
# if USHRT_MAX == UINT_MAX_32_BITS
typedef unsigned short nls_uint32;
# else
#  if ULONG_MAX == UINT_MAX_32_BITS
typedef unsigned long nls_uint32;
#  else
  /* The following line is intended to throw an error.  Using #error is
     not portable enough.  */
  "Cannot determine unsigned 32-bit data type."
#  endif
# endif
#endif


/* Заголовок для двоичного файла формата .mo.  */
struct mo_file_header
{
  /* Магическое число.  */
  nls_uint32 magic;
  /* Номер версии формата файла.  */
  nls_uint32 revision;
  /* Количество пар струн.  */
  nls_uint32 nstrings;
  /* Смещение таблицы с начальными смещениями исходных строк.  */
  nls_uint32 orig_tab_offset;
  /* Смещение таблицы с начальными смещениями строк перевода.  */
  nls_uint32 trans_tab_offset;
  /* Размер хеш-таблицы.  */
  nls_uint32 hash_tab_size;
  /* Смещение первой записи хеширования.  */
  nls_uint32 hash_tab_offset;
};

struct string_desc
{
  /* Длина адресуемой строки.  */
  nls_uint32 length;
  /* Смещение строки в файле.  */
  nls_uint32 offset;
};

/* @@ начало эпилога @@ */

#endif	/* gettext.h  */
