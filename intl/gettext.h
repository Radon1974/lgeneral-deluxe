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

/* @@ ����� ������� @@ */

/* ���������� ����� ������� �������� ��������� GNU.  */
#define _MAGIC 0x950412de
#define _MAGIC_SWAPPED 0xde120495

/* ����� ������ ������������� � ��������� ����� (���������) ������� ����� .mo.  */
#define MO_REVISION_NUMBER 0

/* ��������� ��������� ������������ ����� ������� ������������ ������������ C
   ��� ����������� �������������� ���� ��� ����� ������� 32 ����. An
   �������������� ������ - ������������ ������ AC_CHECK_SIZEOF autoconf, ��
   ��� ����� �����������, ����� ������ ������������ �������������� � * ���������� *
   ������������ ����������� ����. ��������� ������ �����-���������������� ����������� ������
   ������ ��� ����������.  */

#if __STDC__
# define UINT_MAX_32_BITS 4294967295U
#else
# define UINT_MAX_32_BITS 0xFFFFFFFF
#endif

/* ���� UINT_MAX �� ���������, �����������, ��� ��� 32-������ ���.
   ��� ������ ���� ����������� ��� ���� ������, � ������� ��������� GNU, ������ ���
   ��� �� �������� 16-������ �������, � ������ ����������� �������
   (�������, ����������, ����� <limits.h>) ����� 64 + -������ ������������� ����.  */

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


/* ��������� ��� ��������� ����� ������� .mo.  */
struct mo_file_header
{
  /* ���������� �����.  */
  nls_uint32 magic;
  /* ����� ������ ������� �����.  */
  nls_uint32 revision;
  /* ���������� ��� �����.  */
  nls_uint32 nstrings;
  /* �������� ������� � ���������� ���������� �������� �����.  */
  nls_uint32 orig_tab_offset;
  /* �������� ������� � ���������� ���������� ����� ��������.  */
  nls_uint32 trans_tab_offset;
  /* ������ ���-�������.  */
  nls_uint32 hash_tab_size;
  /* �������� ������ ������ �����������.  */
  nls_uint32 hash_tab_offset;
};

struct string_desc
{
  /* ����� ���������� ������.  */
  nls_uint32 length;
  /* �������� ������ � �����.  */
  nls_uint32 offset;
};

/* @@ ������ ������� @@ */

#endif	/* gettext.h  */
