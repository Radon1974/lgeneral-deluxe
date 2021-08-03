/***************************************************************************
                          parser.h  -  description
                             -------------------
    begin                : Sat Mar 9 2002
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


#ifndef __PARSER_H
#define __PARSER_H

#include "list.h"
#include <stdio.h>

struct CommonTreeData;

/*
====================================================================
���� ������ ������������� ������� ��� ������� ������ ASCII �� �����.
� �����.
��������:
  ��� ������ <��������� ������> ������1 .. ������X <�������� ������>
  ���������� <set> ��������
������ ������ ����� ���� ���������� ��� ������� (�����������).
�������� ���������� ����� ���� ���� ��������� �������, ���� ������� �������.
�������� � <��������� ������> <�������� ������>.
�����, ����������� � "..", ��������� ����� �������.
====================================================================
*/

/*
====================================================================
�������.
����������. ��� ������� ������������, ���� ��� ��������� � ������ �<���������>�.
��������� ��� ����� ����������� ����� ������.
PARSER_GROUP_BEGIN:   <������ ������>
PARSER_GROUP_END:     <�������� ������>
PARSER_SET:           <�����>
PARSER_LIST_BEGIN:    <������ ������>
PARSER_LIST_END:      <����� ������>
PARSER_COMMENT_BEGIN: <������ �����������>
PARSER_COMMENT_END:   <����� �����������>
PARSER_STRING:        <������>
PARSER_SYMBOLS:       ������ ���� �������� + ������, ������������ ���
                      ��������� ������ � ������.
PARSER_SKIP_SYMBOLS:  ����� ����� ����� ����� ��������� �������������� ���
                      ����������� � ������� ��������� ������������
====================================================================
*/
typedef enum ParserToken {
  PARSER_INVALID =       -2,
  PARSER_EOI =           -1,
  PARSER_GROUP_BEGIN =   '{',
  PARSER_GROUP_END =     '}',
  PARSER_SET =           '=',
  PARSER_LIST_BEGIN =    '(',
  PARSER_LIST_END =      ')',
  PARSER_COMMENT_BEGIN = '[',
  PARSER_COMMENT_END   = ']',
  PARSER_STRING        = 256,
} ParserToken;
#define PARSER_SYMBOLS       " =(){}[]"
#define PARSER_SKIP_SYMBOLS  "[]"

/*
====================================================================
������� ������ ������������� � ����������� ��������� PData.
��� �������������� ��� ������, � ����� ����������� �� ������.
��� ��� ������ ���� ������.
��������������� ���� ����������, ���� �������.
���� 'entry' �� ����� NULL, PData �������� ������� � '��������'
�������� ��������� �� ������ ������ ��� ������.
���� 'values' �� ����� NULL, PData ������������ ����� ������, � 'values' ��������
������ ����� ��������, ��������� � �������.
====================================================================
*/
typedef struct PData {
    char *name;
    List *values;
    List *entries;
    struct CommonTreeData *ctd;
    int lineno:24;	/* ����� ������, �� ������� ���� ���������������� ��� ������ */
} PData;

/*
====================================================================
��� ������� ��������� ������ �� ������, ��������� �������
��������� � �������� ��� ����� ��������. ���� ������ ������ - ���
������� ������������ ��� ����� ��������, �� �� ����������� ��� �����
(����� ������� ������ �� ������).
====================================================================
*/
List* parser_split_string( const char *string, const char *symbols );
/*
====================================================================
��� ����������� ������ parser_split_string, ������� ��������� �������
������ ���� ������ � �� ��������� ��� ������� ���� �
������. ��� �������� �� 2% �������. ���.
====================================================================
*/
List *parser_explode_string( const char *string, char c );

/*
====================================================================
��� ������� ��������� ���� ������� � ����������� ��� �
��������� ������ PData. ��� ������������� ������ ������������ NULL �
parser_error ����������. tree_name - ��� ��� ������ PData.
====================================================================
*/
PData* parser_read_file( const char *tree_name, const char *fname );

/*
====================================================================
��� ������� ����������� ����������� ��������� PData.
====================================================================
*/
void parser_free( PData **pdata );

/*
====================================================================
��� ������� ������� ����� ��������� PData, ��������� �� � ������������,
� ���������� ���.
�� �������� ����� ������������� �� �����.
������� ��������.
====================================================================
*/
PData *parser_insert_new_pdata( PData *parent, const char *name, List *values );

/*
====================================================================
���������� ��� �����, �� �������� ��� ���������� �������������� ������ ����� ����.
====================================================================
*/
const char *parser_get_filename( PData *pd );

/*
====================================================================
���������� ����� ������, �� ������� ��� ���������� �������������� ������ ����� ����.
====================================================================
*/
int parser_get_linenumber( PData *pd );

/*
====================================================================
������� ��� ������� � ������ PData.
'name' - ��� ������ � ������ 'pd', ��� ���������� �����������
by '/' (e.g.: name = 'config/graphics/animations')
parser_get_pdata   : �������� ������ pdata, ��������� � 'name'
parser_get_entries : �������� ������ ����������� (�������� PData) � 'name'
parser_get_values  : �������� ������ �������� 'name'
parser_get_value   : �������� ���� �������� �� ������ �������� 'name'
parser_get_int     : �������� ������ �������� 'name', ��������������� � ����� �����
parser_get_double  : �������� ������ �������� 'name', ��������������� � double
parser_get_string  : �������� ������ �������� 'name' _duplicated_
���� ��������� ������, ��������������� �������� NULL, ������������ False �
parse_error ����������.
====================================================================
*/
int parser_get_pdata  ( PData *pd, const char *name, PData  **result );
int parser_get_entries( PData *pd, const char *name, List   **result );
int parser_get_values ( PData *pd, const char *name, List   **result );
int parser_get_value  ( PData *pd, const char *name, char   **result, int index );
int parser_get_int    ( PData *pd, const char *name, int     *result );
int parser_get_double ( PData *pd, const char *name, double  *result );
int parser_get_string ( PData *pd, const char *name, char   **result );

/*
====================================================================
�������� ������ �������� 'name', ������������ �� ������� ���� �
�������� ������� ������ � ������� ��� �������������_.
====================================================================
*/
int parser_get_localized_string ( PData *pd, const char *name, const char *domain, char **result );

/*
====================================================================
����������, ���� ��������� ������ �������
====================================================================
*/
int parser_is_error(void);

/*
====================================================================
���� ��������� ������, �� ������ ��������� ��������� � ������� ���� �������.
====================================================================
*/
char* parser_get_error( void );

#endif
