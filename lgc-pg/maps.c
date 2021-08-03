/***************************************************************************
                          maps.c -  description
                             -------------------
    begin                : Tue Mar 12 2002
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <SDL_endian.h>
#include "maps.h"
#include "misc.h"
#include "terrain.h"

/*
====================================================================
�������
====================================================================
*/
extern char *source_path;
extern char *dest_path;
extern char target_name[128];
extern int map_or_scen_files_missing;
extern int terrain_tile_count;
extern char tile_type[];

/*
====================================================================
���������
====================================================================
*/

/*
====================================================================
�������������� tile_id ����������� PG � ������ ��������� �����
  terrain_type_char + tile_id
====================================================================
*/
static void tile_get_id_string( int id, char *string )
{
    char type;
    int i;
    int sub_id = 0;
    type = tile_type[id];
    for ( i = 0; i < id; i++ )
        if ( tile_type[i] == type )
            sub_id++;
    sprintf( string, "%c%i", type, sub_id );
}

/*
====================================================================
��������� ��� ��������� ����� � ���� ��������������� � buf
====================================================================
*/
static void tile_get_name( FILE *file, int id, char *buf )
{
    memset( buf, 0, 21 );
    fseek( file, 2 + id * 20, SEEK_SET );
    if ( feof( file ) ) 
        sprintf( buf, "none" );
    else
        fread( buf, 20, 1, file );
}

/*
====================================================================
���������
====================================================================
*/

/*
====================================================================
If map_id is -1 �������������� ��� �����, ��������� � 'source_path'.
If map_id is >= 0 ��� ���� ������������� ����� � ������� �
������� �������.
====================================================================
*/
int maps_convert( int map_id )
{
    int i, start, end;
    char path[MAXPATHLEN];
    FILE *dest_file, *source_file, *name_file;
    int width, height;
    int tile_id;
    char map_tile_str[8];
    char name_buf[24];
    int x, y, ibuf;

    if (map_id == -1) {
        snprintf( path, MAXPATHLEN, "%s/maps/%s", dest_path, target_name );
        mkdir( path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
    }

	/* ��� �������������� �������� pg ���������� �������� ���� � �������� ��������� 
	 * �� ���������: ����� ���� ����� ��������������� � ������ 
	if (strcmp(target_name,"pg") == 0) {
		char spath[MAXPATHLEN], dpath[MAXPATHLEN];
		snprintf(spath,MAXPATHLEN,"%s/mapnames.str",source_path);
		snprintf(dpath,MAXPATHLEN,"%s/convdata/mapnames", get_gamedir() );
		copy_ic(spath,dpath);
	} */

    printf( "Maps...\n" );
    /* ���� ����� (���������� ���, ������� ��������� � lgc-pg ��� ���������) */
    snprintf( path, MAXPATHLEN, "%s/mapnames.str", source_path );
    if ( (name_file = fopen_ic( path, "r" )) == NULL ) {
        snprintf( path, MAXPATHLEN, "%s/convdata/mapnames", get_gamedir() );
        if ( ( name_file = fopen( path, "r" ) ) == NULL ) {
            fprintf( stderr, "ERROR: mapnames neither found in %s nor in lgc-pg directory?\n",
                    	source_path);
            return 0;
        }
    }
    
    /* ���������� �������� ����� */
    if ( map_id == -1 ) {
        start = 1;
        end = 38;
    } else {
        start = end = map_id;
    }
    
    for ( i = start; i <=end; i++ ) {
        /* ������� ���� ������ */
        snprintf( path, MAXPATHLEN, "%s/map%02d.set", source_path, i );
        if (( source_file = fopen_ic( path, "r" ) ) == NULL) {
            fprintf( stderr, "%s: file not found\n", path );
            /* ��� ���������������� �������� ����� �������������� �� ��� ����� / ��������, �������
             * �� �������� ��� ���������, ������ ����������� */
            if (map_id == -1 && strcmp(target_name,"pg")) {
                map_or_scen_files_missing = 1;
                continue;
            } else
                return 0;
        }
        
        /* ������� �������� ���� */
        if ( map_id == -1 )
            snprintf( path, MAXPATHLEN, "%s/maps/%s/map%02d", 
                                                dest_path, target_name, i );
        else
            snprintf( path, MAXPATHLEN, "%s/scenarios/%s",
                                                dest_path, target_name );
        if ( ( dest_file = fopen( path, (map_id==-1)?"w":"a" ) ) == NULL ) {
            fprintf( stderr, "%s: access denied\n", path );
            fclose( source_file );
            return 0;
        }
        
        /* ����� ��� ������ ����� */
        if ( map_id == -1 )
            fprintf( dest_file, "@\n" );
        /* ���� ��������� */
        if (map_id == -1)
            fprintf( dest_file, "terrain_db�%s.tdb\n", target_name );
        else
            fprintf( dest_file, "terrain_db�pg.tdb\n" );
        /* ����� */
        fprintf( dest_file, "domain�pg\n" );
        /* ������ / ������ ������� ����� */
        width = height = 0;
        fseek( source_file, 101, SEEK_SET );
        fread( &width, 2, 1, source_file ); 
        width = SDL_SwapLE16( width );
        fseek( source_file, 103, SEEK_SET );
        fread( &height, 2, 1, source_file ); 
        height = SDL_SwapLE16( height );
        width++; height++;
        fprintf( dest_file, "width�%i\nheight�%i\n", width, height );
        /* �������������� ����������� */
        fseek( source_file, 123 + 5 * width * height, SEEK_SET );
        fprintf( dest_file, "tiles�" );
        for ( y = 0; y < height; y++ ) {
            for ( x = 0; x < width; x++ ) {
                tile_id = 0;
                fread( &tile_id, 2, 1, source_file ); 
                tile_id = SDL_SwapLE16( tile_id );
                tile_get_id_string( tile_id, map_tile_str );
                fprintf( dest_file, "%s", map_tile_str );
                if ( y < height - 1 || x < width - 1 )
                    fprintf( dest_file, "�" );
            }
        }
        fprintf( dest_file, "\n" );
        fprintf( dest_file, "names�" );
        fseek( source_file, 123, SEEK_SET );
        for ( y = 0; y < height; y++ ) {
            for ( x = 0; x < width; x++ ) {
                ibuf = 0; fread( &ibuf, 2, 1, source_file );
                ibuf = SDL_SwapLE16( ibuf );
                tile_get_name( name_file, ibuf, name_buf );
                fprintf( dest_file, "%s", name_buf );
                if ( y < height - 1 || x < width - 1 )
                    fprintf( dest_file, "�" );
            }
        }
        fprintf( dest_file, "\n" );
        fclose( source_file );
        fclose( dest_file );
    }
    fclose( name_file );
    return 1;
}
