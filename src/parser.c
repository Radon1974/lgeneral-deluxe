/***************************************************************************
                          parser.c  -  description
                             -------------------
    begin                : Sat Mar 9 2002
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

#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "misc.h"
#include "localize.h"

/*
====================================================================
������ ������.
====================================================================
*/
static char parser_sub_error[MAX_LINE_SHORT];
static char parser_error[MAX_LINE_SHORT];

/*
====================================================================
����� ������ ������ ��������������� �������.
====================================================================
*/

struct CommonTreeData {
    char *filename;	/* ��� �����, �� �������� ��� ������ ���� ���������������� */
    int ref;		/* ������� ������ */
};

static struct CommonTreeData *common_tree_data_create(const char *filename) {
    struct CommonTreeData *ctd = malloc(sizeof(struct CommonTreeData));
    ctd->filename = strdup(filename);
    ctd->ref = 0;
    return ctd;
}

static void common_tree_data_delete(struct CommonTreeData *ctd) {
    free(ctd->filename);
}

static void common_tree_data_ref(struct CommonTreeData *ctd) {
    ctd->ref++;
}

static int common_tree_data_deref(struct CommonTreeData *ctd) {
    int del = --ctd->ref == 0;
    if (del) common_tree_data_delete(ctd);
    return del;
}

/*
====================================================================
���� ����� ������������ ��� ������ �������� ������ ��������, �����
������������ ���������� ������.
====================================================================
*/
enum { CBUFFER_SIZE = 1310720*2 }; /* 2560 KB */
static char cbuffer[CBUFFER_SIZE];
static char* cbuffer_pos = 0; /* position in cbuffer */

/*
====================================================================
��������� ��� ����� ���������� ������, ������ ��� ����� ���������� ��������
����� ��� �������.
====================================================================
*/
enum { PARSER_MAX_TOKEN_LENGTH = 1024 };

/*
====================================================================
���������� � ���������, ������� ��� ����� �� ����� ��������������� �������
====================================================================
*/
typedef struct {
    const char *fname;  /* ��� �������� ����� */
    FILE *file;		/* ���������� �������� ����� */
    int lineno;		/* ������� ����� ������ */
    char ch;		/* ������� ������ �� �������� ������ */
    ParserToken tok;	/* ������� ����� */
    char tokbuf[PARSER_MAX_TOKEN_LENGTH]; /* �������� ������ */
    struct CommonTreeData *ctd;
} ParserState;

/*
====================================================================
���������
====================================================================
*/

/*
====================================================================
������ ��� ���������� ������ fread ��� ������ �������.
====================================================================
*/
#define FILE_READCHAR( st, c ) do { \
	fread( &c, sizeof( char ), 1, (st)->file ); \
	if (c == '\n') (st)->lineno++; \
} while(0)

/*
====================================================================
����������, �������� �� ��� ���������� �������� ��� ������.
====================================================================
*/
static int parser_is_valid_string_char(char ch)
{
    switch (ch) {
    case '$': case '/': case '@': case '_': case '.': case ':': case '~':
    case '-': case '%': case '#': case '^':
        return 1;
    default:
        return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z')
            || (ch >= 'a' && ch <= 'z') || (unsigned char)ch > 127;
    }
}

/*
====================================================================
������� ��������� ����� ������ � cbuffer, �������� �� �� \ 0 � �������
��������� �� ������� ������.
====================================================================
*/
static char* parser_get_next_line(ParserState *st)
{
    char *line = cbuffer_pos;
    char *newpos;
    if ( cbuffer_pos[0] == 0 )
        return 0; /* ��������� ��������. ������ ������� �����. */
    if ( ( newpos = strchr( cbuffer_pos, 13 ) ) == 0 )
        cbuffer_pos++;
    if ( ( newpos = strchr( cbuffer_pos, 10 ) ) == 0 )
        cbuffer_pos += strlen( cbuffer_pos ); /* ��������� ����� */
    else {
        cbuffer_pos = newpos + 1; /* ���������� ��������� �� ��������� ������ */
        newpos[0] = 0; /* ��������� ������� ������ */
    }
    st->lineno++;
    return line;
}

/*
====================================================================
���������� ������ ������ ��������������� �������: "file:line: error"
====================================================================
*/
static void parser_set_parse_error( ParserState *st, const char *error )
{
    snprintf( parser_error, sizeof parser_error, "%s: %i: %s",
              st->fname, st->lineno, error );
}

/*
====================================================================
������� �������������� ������� �� stderr � ��������� "file:line: msg"
====================================================================
*/
static void parser_print_warning(ParserState *st, const char *msg)
{
    parser_set_parse_error(st, msg);
    fprintf(stderr, tr("warning: %s\n"), parser_error);
}

/*
====================================================================
��������� ��������� ����� �� ��������� �������� ������ � ������� ���.
====================================================================
*/
static ParserToken parser_read_token(ParserState *st) {
    char ch = st->ch;
    /* ������������ ������� � ����������� */
    int comment_nesting = 0;
    while (!feof(st->file) && (ch == ' ' || ch == '\t' || ch == '\n'
           || comment_nesting > 0 || ch == PARSER_COMMENT_BEGIN
           || (comment_nesting >= 0 && ch == PARSER_COMMENT_END))) {
        if (ch == PARSER_COMMENT_BEGIN) comment_nesting++;
        else if (ch == PARSER_COMMENT_END) comment_nesting--;
        FILE_READCHAR(st, ch);
    }
    if (feof(st->file)) {
        st->tok = PARSER_EOI;
        strcpy(st->tokbuf, tr("<EndOfInput>"));
        return st->tok;
    }

    switch (ch) {
    case PARSER_GROUP_BEGIN:
    case PARSER_GROUP_END:
    case PARSER_SET:
    case PARSER_LIST_BEGIN:
    case PARSER_LIST_END:
        st->tok = ch;
        st->tokbuf[0] = ch;
        st->tokbuf[1] = 0;
        FILE_READCHAR(st, ch);
        break;
    case '"': {		/* ������ � �������� */
        int skip_next_char = 0;
        int idx = 0;
        /* ����������� ��������� �� ������ ������, ����� ������������ ����� ������ ��������� ����� */
        parser_set_parse_error(st, tr("Unterminated string constant"));
        FILE_READCHAR(st, ch); /* ���������� ������� ������ */
        while (!feof(st->file) && (skip_next_char || ch != '"')) {
            skip_next_char = 0;
            if (ch == '\\') skip_next_char = 1;
            else if (idx < PARSER_MAX_TOKEN_LENGTH - 1) st->tokbuf[idx++] = ch;
            FILE_READCHAR(st, ch);
        }
        st->tokbuf[idx] = '\0';
        if (feof(st->file)) st->tok = PARSER_EOI;
        else {
            parser_error[0] = '\0';
            if (idx == PARSER_MAX_TOKEN_LENGTH - 1)
                parser_print_warning(st, tr("token exceeds limit"));
            FILE_READCHAR(st, ch);	/* ���������� ����������� ������ */
            st->tok = PARSER_STRING;
        }
        break;
    }
    default:
        /* ������ ��� ������� */
        if (parser_is_valid_string_char(ch)) {
            int idx = 0;
            do {
                if (idx < PARSER_MAX_TOKEN_LENGTH - 1) st->tokbuf[idx++] = ch;
                FILE_READCHAR(st, ch);
            } while (!feof(st->file) && parser_is_valid_string_char(ch));
            if (idx == PARSER_MAX_TOKEN_LENGTH - 1)
                parser_print_warning(st, tr("token exceeds limit"));
            st->tokbuf[idx] = '\0';
            st->tok = PARSER_STRING;
            break;
        }
        snprintf(parser_sub_error, sizeof parser_sub_error, tr("Invalid character '%c'"), ch);
        st->tok = PARSER_EOI;
        break;
    }
    st->ch = ch;
    return st->tok;
}

/*
====================================================================
���������, ������������ �� ������ ������ � ������ ��������.
���� ������ ������ "", �� ������������ ��� �������������� ���� ��� ����
����� ������������.
====================================================================
*/
static int is_symbol( int c, const char *symbols )
{
    int i = 0;
    if ( symbols[0] == ' ' && c <= 32 ) return 1;
    while ( symbols[i] != 0 )
        if ( c == symbols[i++] )
            return 1;
    return 0;
}

/*
====================================================================
����������� � ������ ������, ���� ��� �� ���������� ��� �� �������� ������������
� ������� ����� �������.
====================================================================
*/
static  const char* string_ignore_whitespace( const char *string )
{
    while ( *string != 0 && (unsigned char)*string <= 32 ) string++;
    return string;
}

/*
====================================================================
������� ����� ������ � pdata.
!! ������� �� ���������� ������� pdata-������� ��� ���� ������� !!
�� ����������. � ��� ����������. ��� ������ ��� ����� �����
����� ������������ ���.
====================================================================
*/
static  PData *parser_create_pdata( char *name, List *values, int lineno, struct CommonTreeData *ctd )
{
    PData *pd = calloc(1, sizeof(PData));
    pd->name = name;
    pd->values = values;
    pd->lineno = lineno;
    pd->ctd = ctd;
    common_tree_data_ref(pd->ctd);
    return pd;
}

/*
====================================================================
������ ���� ������ pdata �� ������� ������� �����
====================================================================
*/
static PData* parser_parse_entry( ParserState *st )
{
    PData *pd = 0, *sub = 0;

    if (st->tok == PARSER_EOI) return 0;

    /* �������� ��� */
    if ( st->tok != PARSER_STRING ) {
        sprintf( parser_sub_error, tr("parse error before '%s'"), st->tokbuf );
        return 0;
    }
    pd = parser_create_pdata(strdup(st->tokbuf), 0, st->lineno, st->ctd);
    parser_read_token(st);
    /* ��� ���� */
    switch ( st->tok ) {
        case PARSER_SET:
            parser_read_token(st);
            pd->values = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
            /* ��������� ���� �������� ��� ������ */
            if (st->tok == PARSER_STRING ) {
                list_add( pd->values, strdup(st->tokbuf) );
                parser_read_token(st);
            } else if ( st->tok == PARSER_LIST_BEGIN ) {
                parser_read_token(st);
                while (st->tok == PARSER_STRING) {
                    list_add( pd->values, strdup(st->tokbuf) );
                    parser_read_token(st);
                }
                if (st->tok != PARSER_LIST_END) goto parse_error;
                parser_read_token(st);
            } else {
parse_error:
                sprintf( parser_sub_error, tr("parse error before '%s'"), st->tokbuf );
                goto failure;
            }
            break;
        case PARSER_GROUP_BEGIN:
            /* ��������� ��� ������, ���� PARSER_GROUP_END */
            parser_read_token(st);
            pd->entries = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
            *parser_sub_error = 0;
            while ( st->tok == PARSER_STRING ) {
                sub = parser_parse_entry( st );
                if ( sub )
                    list_add( pd->entries, sub );
                if (*parser_sub_error)
                    goto failure;
            }
            if (st->tok == PARSER_GROUP_END) {
                parser_read_token(st);
                break;
            }
            /* ����������� */
        default:
            sprintf( parser_sub_error, tr("parse error before '%s'"), st->tokbuf );
            goto failure;
    }
    return pd;
failure:
    parser_free( &pd );
    return 0;
}

/*
====================================================================
���������
====================================================================
*/

/*
====================================================================
��� ������� ��������� ������ �� ������, ��������� �������
��������� � �������� ��� ����� ��������. ���� ������ ������ - ���
������� ������������ ��� ����� ��������, �� �� ����������� ��� �����
(����� ������� ������ �� ������).
====================================================================
*/
List* parser_split_string( const char *string, const char *symbols )
{
    int pos;
    char *token = 0;
    List *list = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
    while ( string[0] != 0 ) {
        if ( symbols[0] == ' ' )
            string = string_ignore_whitespace( string );
        if ( string[0] == 0 ) break;
        pos = 1; /* "������ �" ������ ������ */
        while ( string[pos - 1] != 0 && !is_symbol( string[pos - 1], symbols ) && string[pos - 1] != '"' ) pos++;
        if ( pos > 1 )
            pos--;
        else
            if ( string[pos - 1] == '"' ) {
                /* ��������� ������ */
                string++; pos = 0;
                while ( string[pos] != 0 && string[pos] != '"' ) pos++;
                token = calloc( pos + 1, sizeof( char ) );
                strncpy( token, string, pos ); token[pos] = 0;
                list_add( list, token );
                string += pos + (string[pos] != 0);
                continue;
            }
        token = calloc( pos + 1, sizeof( char ) );
        strncpy( token, string, pos); token[pos] = 0;
        list_add( list, token );
        string += pos;
    }
    return list;
}
/*
====================================================================
��� ����������� ������ parser_split_string, ������� ��������� �������
������ ���� ������ � �� ��������� ��� ������� ���� �
������. ��� �������� �� 2% �������. ���.
====================================================================
*/
List *parser_explode_string( const char *string, char c )
{
    List *list = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
    char *next_slash = 0;
    char small_buf[64];
    while ( *string != 0 && ( next_slash = strchr( string, c ) ) != 0 ) {
        if ( next_slash != string ) {
	    const unsigned buflen = next_slash - string;
	    /* �� ���������� ����. �������� alloca ������ ��� ������� ����� */
	    char *buffer = buflen < sizeof small_buf ? small_buf : alloca(buflen + 1);
            memcpy( buffer, string, buflen );
	    buffer[buflen] = 0;
            list_add( list, strdup( buffer ) );
        }
        string += next_slash - string + 1;
    }
    if ( string[0] != 0 )
        list_add( list, strdup( string ) );
    return list;
}

/*
====================================================================
��� ������� ��������� ���� ������� � ����������� ��� �
��������� ������ PData. ��� ������������� ������ ������������ NULL �
parser_error ����������.
====================================================================
*/
static int parser_read_file_full( ParserState *st, PData *top )
{
    /* parse file */
    *parser_sub_error = 0;
    do {
        PData *sub = 0;
        if ( ( sub = parser_parse_entry( st ) ) != 0 )
            list_add( top->entries, sub );
        if (*parser_sub_error)
            return 0;
    } while (st->tok != PARSER_EOI);
    return 1;
}
static int parser_read_file_compact( ParserState *st, PData *section )
{
    /* ������ - ��� ������������ pdata, �������� ����� ���������
       ������ */
    PData *pd = 0;
    char *line, *cur;
    while ( ( line = parser_get_next_line(st) ) ) {
        switch ( line[0] ) {
            case '>':
                /* ���� ������ �������� */
                return 1;
            case '<':
                /* �������� ����� ��������� */
                pd = parser_create_pdata(strdup( line + 1 ), 0, st->lineno, st->ctd);
                pd->entries = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
                parser_read_file_compact( st, pd );
                /* �������� � ������ */
                list_add( section->entries, pd );
                break;
            default:
                /* ��������� ��� */
                if ( ( cur = strchr( line, '�' ) ) == 0 ) {
                    sprintf( parser_sub_error, tr("parse error: use '%c' for assignment or '<' for section"), '�' );
                    return 0;
                }
                cur[0] = 0; cur++;
                /* ������ �������� ��� ��������� */
                pd = parser_create_pdata(strdup( line ),
                                         parser_explode_string( cur, '�' ),
                                         st->lineno, st->ctd);
                /* �������� � ������ */
                list_add( section->entries, pd );
                break;
        }
    }
    return 1;
}
PData* parser_read_file( const char *tree_name, const char *fname )
{
    int size;
    char magic = 0;
    ParserState state = { fname, 0, 1, 0 };
    ParserState *st = &state;
    PData *top = 0;
    /* ������� ���� */
    if ( ( state.file = fopen( fname, "r" ) ) == 0 ) {
        sprintf( parser_error, tr("%s: file not found"), fname );
        return 0;
    }
    /* ��������� ����� ������ ������ */
    st->ctd = common_tree_data_create(fname);
    /* ������� pdata �������� ������ */
    top = parser_create_pdata(strdup( tree_name ), 0, st->lineno, st->ctd);
    top->entries = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );
    /* ��������� */
    FILE_READCHAR( st, st->ch );
    magic = st->ch;
    if ( magic == '@' ) {
        /* �������� ��� ���������� - 1 � CBUFFER_SIZE ������������� */
        fseek( st->file, 0, SEEK_END ); size = ftell( st->file ) - 2;
        if ( size >= CBUFFER_SIZE ) {
            fprintf( stderr, tr("%s: file's too big to fit the compact buffer (%dKB)\n"), fname, CBUFFER_SIZE / 1024 );
            size = CBUFFER_SIZE - 1;
        }
        fseek( st->file, 2, SEEK_SET );
        fread( cbuffer, 1, size, st->file );
        cbuffer[size] = 0;
        /* ���������� ��������� � ������ ������ */
        cbuffer_pos = cbuffer;
        /* ������������� ����� */
        if ( !parser_read_file_compact( st, top ) ) {
            parser_set_parse_error( st, parser_sub_error );
            goto failure;
        }
    }
    else {
        parser_read_token(st);
        if ( !parser_read_file_full( st, top ) ) {
            parser_set_parse_error( st, parser_sub_error );
            goto failure;
        }
    }
    /* ��������� */
    fclose( st->file );
    return top;
failure:
    fclose( st->file );
    parser_free( &top );
    return 0;
}

/*
====================================================================
��� ������� ����������� ����������� ��������� PData.
====================================================================
*/
void parser_free( PData **pdata )
{
    PData *entry = 0;
    if ( (*pdata) == 0 ) return;
    if ( (*pdata)->entries ) {
        list_reset( (*pdata)->entries );
        while ( ( entry = list_next( (*pdata)->entries ) ) )
            parser_free( &entry );
        list_delete( (*pdata)->entries );
    }
    if ( (*pdata)->name ) free( (*pdata)->name );
    if ( (*pdata)->values ) list_delete( (*pdata)->values );
    common_tree_data_deref((*pdata)->ctd);
    free( *pdata ); *pdata = 0;
}

/*
====================================================================
��� ������� ������� ����� ��������� PData, ��������� �� � ������������,
� ���������� ���.
�� �������� ����� ������������� �� �����.
������� ��������.
====================================================================
*/
PData *parser_insert_new_pdata( PData *parent, const char *name, List *values )
{
    PData *pd = parser_create_pdata(strdup(name), values, 0, parent->ctd);

    if (!parent->entries)
        parent->entries = list_create( LIST_NO_AUTO_DELETE, LIST_NO_CALLBACK );

    list_add(parent->entries, pd);
    return pd;
}

/*
====================================================================
���������� ��� �����, �� �������� ��� ���������� �������������� ������ ����� ����.
====================================================================
*/
const char *parser_get_filename( PData *pd )
{
    return pd->ctd->filename;
}

/*
====================================================================
���������� ����� ������, �� ������� ��� ���������� �������������� ������ ����� ����.
====================================================================
*/
int parser_get_linenumber( PData *pd )
{
    return pd->lineno;
}

/*
====================================================================
������� ��� ������� � ������ PData.
'name' - ��� ������ � ������ 'pd', ��� ���������� �����������
�� '/' (��������: name = 'config / graphics / animations')
parser_get_pdata   : �������� ������ pdata, ��������� � '������'
parser_get_entries : �������� ������ ����������� (�������� PData) � 'name'
parser_get_values  : �������� ������ �������� 'name'
parser_get_value: �������� ���� �������� �� ������ �������� 'name'
parser_get_int: �������� ������ �������� 'name', ��������������� � ����� �����
parser_get_double: �������� ������ �������� 'name', ��������������� � double
parser_get_string: �������� ������ �������� 'name' _duplicated_
���� ��������� ������, ��������������� �������� NULL, ������������ False �
parse_error ����������.
====================================================================
*/
int parser_get_pdata  ( PData *pd, const char *name, PData  **result )
{
    int i, found;
    PData *pd_next = pd;
    PData *entry = 0;
    char *sub = 0;
    List *path = parser_explode_string( name, '/' );
    for ( i = 0, list_reset( path ); i < path->count; i++ ) {
        ListIterator it;
        sub = list_next( path );
        if ( !pd_next->entries ) {
            sprintf( parser_sub_error, tr("%s: no subtrees"), pd_next->name );
            goto failure;
        }
        it = list_iterator( pd_next->entries ); found = 0;
        while ( ( entry = list_iterator_next( &it ) ) )
            if ( strlen( entry->name ) == strlen( sub ) && !strncmp( entry->name, sub, strlen( sub ) ) ) {
                pd_next = entry;
                found = 1;
                break;
            }
        if ( !found ) {
            sprintf( parser_sub_error, tr("%s: subtree '%s' not found"), pd_next->name, sub );
            goto failure;
        }
    }
    list_delete( path );
    *result = pd_next;
    return 1;
failure:
    sprintf( parser_error, "parser_get_pdata: %s/%s: %s", pd->name, name, parser_sub_error );
    list_delete( path );
    *result = 0;
    return 0;
}
int parser_get_entries( PData *pd, const char *name, List   **result )
{
    PData *entry;
    *result = 0;
    if ( !parser_get_pdata( pd, name, &entry ) ) {
        sprintf( parser_sub_error, "parser_get_entries:\n %s", parser_error );
        strcpy( parser_error, parser_sub_error );
        return 0;
    }
    if ( !entry->entries || entry->entries->count == 0 ) {
        sprintf( parser_error, tr("parser_get_entries: %s/%s: no subtrees"), pd->name, name );
        return 0;
    }
    *result = entry->entries;
    return 1;
}
int parser_get_values ( PData *pd, const char *name, List   **result )
{
    PData *entry;
    *result = 0;
    if ( !parser_get_pdata( pd, name, &entry ) ) {
        sprintf( parser_sub_error, "parser_get_values:\n %s", parser_error );
        strcpy( parser_error, parser_sub_error );
        return 0;
    }
    if ( !entry->values || entry->values->count == 0 ) {
        sprintf( parser_error, tr("parser_get_values: %s/%s: no values"), pd->name, name );
        return 0;
    }
    *result = entry->values;
    return 1;
}
int parser_get_value  ( PData *pd, const char *name, char   **result, int index )
{
    List *values;
    if ( !parser_get_values( pd, name, &values ) ) {
        sprintf( parser_sub_error, "parser_get_value:\n %s", parser_error );
        strcpy( parser_error, parser_sub_error );
        return 0;
    }
    if ( index >= values->count ) {
        sprintf( parser_error, tr("parser_get_value: %s/%s: index %i out of range (%i elements)"),
                 pd->name, name, index, values->count );
        return 0;
    }
    *result = list_get( values, index );
    return 1;
}
int parser_get_int    ( PData *pd, const char *name, int     *result )
{
    char *value;
    if ( !parser_get_value( pd, name, &value, 0 ) ) {
        sprintf( parser_sub_error, "parser_get_int:\n %s", parser_error );
        strcpy( parser_error, parser_sub_error );
        return 0;
    }
    *result = atoi( value );
    return 1;
}
int parser_get_double ( PData *pd, const char *name, double  *result )
{
    char *value;
    if ( !parser_get_value( pd, name, &value, 0 ) ) {
        sprintf( parser_sub_error, "parser_get_double:\n %s", parser_error );
        strcpy( parser_error, parser_sub_error );
        return 0;
    }
    *result = strtod( value, 0 );
    return 1;
}
int parser_get_string ( PData *pd, const char *name, char   **result )
{
    char *value;
    if ( !parser_get_value( pd, name, &value, 0 ) ) {
        sprintf( parser_sub_error, "parser_get_string:\n %s", parser_error );
        strcpy( parser_error, parser_sub_error );
        return 0;
    }
    *result = strdup( value );
    return 1;
}

/*
====================================================================
�������� ������ �������� 'name', ������������ �� ������� ���� �
��������� ������� ������, � ������� ��� _duplicated_.
====================================================================
*/
int parser_get_localized_string ( PData *pd, const char *name, const char *domain, char **result )
{
    char *value;
    if ( !parser_get_value( pd, name, &value, 0 ) ) {
        sprintf( parser_sub_error, "parser_get_string:\n %s", parser_error );
        strcpy( parser_error, parser_sub_error );
        return 0;
    }
    *result = strdup( trd(domain, value) );
    return 1;
}

/*
====================================================================
����������, ���� ��������� ������ ��������������� �����������
====================================================================
*/
int parser_is_error(void)
{
    return !!*parser_error;
}

/*
====================================================================
���� ��������� ������, �� ������ ��������� ������� � ������� ���� �������.
====================================================================
*/
char* parser_get_error( void )
{
    return parser_error;
}
