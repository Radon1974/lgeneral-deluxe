/***************************************************************************
                          campaign.c  -  description
                             -------------------
    begin                : Fri Apr 5 2002
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

#include "lgeneral.h"
#include "parser.h"
#include "list.h"
#include "unit.h"
#include "date.h"
#include "scenario.h"
#include "campaign.h"
#include "localize.h"
#include "misc.h"
#include "file.h"
#include "FPGE/pgf.h"

/*
====================================================================
Внешние
====================================================================
*/
extern Setup setup;
extern Config config;

/* флаг кампании - 0: игра по одиночному сценарию
                   1: сценарий игровой кампании
                   2: продолжить кампанию
                   3: перезапуск сценария кампании */
int camp_loaded = NO_CAMPAIGN;
char *camp_fname = 0;
char *camp_name = 0;
char *camp_desc = 0;
char *camp_authors = 0;
List *camp_entries = 0; /* записи сценариев */
Camp_Entry *camp_cur_scen = 0;
char *camp_first;

/*
====================================================================
Локальные
====================================================================
*/

/**
 * разрешите ссылку на значение, определенное в другом состоянии сценария
 * @param дерево синтаксического анализа записей, содержащее состояния сценария
 * @param имя ключа входа
 * @param scenstat имя сценария-состояния, из которого мы решаем
 * @return разрешенное значение или 0, если значение не может быть разрешено
 * решено.
 */
static const char *camp_resolve_ref(PData *entries, const char *key, const char *scen_stat)
{
    const char *cur_scen_stat = scen_stat;
    char *value = (char *)scen_stat;
    do {
        PData *scen_entry;
        if (!parser_get_pdata(entries, cur_scen_stat, &scen_entry)) break;
        if (!parser_get_value(scen_entry, key, &value, 0)) {
            /* если ключ указанной записи не найден, то обычно
это ошибка -> уведомить пользователя. */
            if (cur_scen_stat != scen_stat) {
                fprintf(stderr, tr("%s: %d: warning: entry '%s' not found in '%s/%s'\n"),
                        parser_get_filename(scen_entry),
                        parser_get_linenumber(scen_entry),
                        key, entries->name, cur_scen_stat);
            }
            return 0;
        }

        /* является ли это ценностью? */
        if (value[0] != '@') break; /* да, верните его прямо сейчас */

        /* в противном случае разыменуйте его */
        cur_scen_stat = value + 1;

        /* проверьте наличие циклов и спасайтесь при обнаружении */
        if (strcmp(cur_scen_stat, scen_stat) == 0) {
            fprintf(stderr, "%s: cycle detected: %s\n", __FUNCTION__, value);
            break;
        }
    } while (1);
    return value;
}

/** решение ключевых слов и перевести результат относительно домена */
static char *camp_resolve_ref_localized(PData *entries, const char *key, const char *scen_stat, const char *domain)
{
    const char *res = camp_resolve_ref(entries, key, scen_stat);
    return res ? strdup(trd(domain, res)) : 0;
}

/*
====================================================================
Загрузите кампанию в старом формате lgcam.
====================================================================
*/
int camp_load_lgcam( const char *fname, const char *path )
{
    Camp_Entry *centry = 0;
    PData *pd, *scen_ent, *sub, *pdent, *subsub;
    List *entries, *next_entries;
    char str[MAX_LINE_SHORT];
    char *result, *next_scen;
    char *domain = 0;
    camp_delete();
    camp_fname = strdup( fname );

    if ( ( pd = parser_read_file( fname, path ) ) == 0 ) goto parser_failure;
    domain = determine_domain(pd, fname);
    locale_load_domain(domain, 0/*FIXME*/);
    /* название, описание, авторы */
    if ( !parser_get_localized_string( pd, "name", domain, &camp_name ) ) goto parser_failure;
    if ( !parser_get_localized_string( pd, "desc", domain, &camp_desc ) ) goto parser_failure;
    if ( !parser_get_localized_string( pd, "authors", domain, &camp_authors ) ) goto parser_failure;
    /* записи */
    if ( !parser_get_pdata( pd, "scenarios", &scen_ent ) ) goto parser_failure;
    entries = scen_ent->entries;
    list_reset( entries );
    camp_entries = list_create( LIST_AUTO_DELETE, camp_delete_entry );
    while ( ( sub = list_next( entries ) ) ) {
        if (!camp_first) camp_first = strdup(sub->name);
        centry = calloc( 1, sizeof( Camp_Entry ) );
        centry->id = strdup( sub->name );
        parser_get_string( sub, "scenario", &centry->scen );
        centry->title = camp_resolve_ref_localized(scen_ent, "title", centry->id, domain);
        centry->brief = camp_resolve_ref_localized(scen_ent, "briefing", centry->id, domain);
        if (!centry->brief) goto parser_failure;

        if (!parser_get_string( sub, "coretransfer", &centry->core_transfer ))
			centry->core_transfer = strdup("denied");

        if ( parser_get_entries( sub, "next", &next_entries ) ) {
            centry->nexts = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
            list_reset( next_entries );
            while ( ( subsub = list_next( next_entries ) ) ) {
                result = subsub->name;
                next_scen = list_first( subsub->values );
                snprintf( str, sizeof(str), "%s>%s", result, next_scen );
                list_add( centry->nexts, strdup( str ) );
            }
        }
        /* если сценарий задан, ищите поддерево "разбор полетов", в противном случае
         * для поддерева "параметры". Внутренне они управляются одинаково.
         */
        if ( (centry->scen && parser_get_pdata( sub, "debriefings", &pdent ))
             || parser_get_pdata( sub, "options", &pdent ) ) {
            next_entries = pdent->entries;
            centry->descs = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
            list_reset( next_entries );
            while ( ( subsub = list_next( next_entries ) ) ) {
                char key[MAX_LINE_SHORT];
                const char *result = subsub->name;
                const char *next_desc;
                snprintf(key, sizeof key, "%s/%s", pdent->name, subsub->name);
                next_desc = camp_resolve_ref(scen_ent, key, centry->id);
                next_desc = trd(domain, next_desc ? next_desc : list_first(subsub->values));
                snprintf( str, sizeof(str), "%s>%s", result, next_desc );
                list_add( centry->descs, strdup( str ) );
            }
        }
        list_add( camp_entries, centry );
    }
    parser_free( &pd );
    camp_loaded = FIRST_SCENARIO;
    camp_verify_tree();
    free(domain);
    return 1;
parser_failure:
    fprintf( stderr, "%s\n", parser_get_error() );
    camp_delete();
    free(domain);
    return 0;
}

/*
====================================================================
Загрузите кампанию в новом формате lgdcam.
====================================================================
*/
int camp_load_lgdcam( const char *fname, const char *path, const char *info_entry_name )
{
    List *temp_list;
    Camp_Entry *centry = 0;
    FILE *inf;
    char outcomes[10][256];
    char line[MAX_SCENARIO_LINE],tokens[MAX_TOKENS][MAX_CAMPAIGN_TOKEN_LENGTH];
    char str[MAX_LINE_SHORT];
    int j,i,block=0,last_line_length=-1,cursor=0,token=0, current_outcome = 0;
    int utf16 = 0, lines = 0;
    camp_delete();

    inf=fopen(path,"rb");
    if (!inf)
    {
        fprintf( stderr, "Couldn't open campaign file\n");
        return 0;
    }

    while (load_line(inf,line,utf16)>=0)
    {
        lines++;
        //комментарии к полосе
        if (line[0]==0x23 || line[0]==0x09) { line[0]=0; }
        if (strlen(line)>0 && last_line_length==0)
        {
            block++;
        }
        last_line_length=strlen(line);
        token=0;
        cursor=0;
        for(i=0;i<strlen(line);i++)
            if (line[i]==0x09)
            {
                tokens[token][cursor]=0;
                token++;
                cursor=0;
            }
            else
            {
                tokens[token][cursor]=line[i];
                cursor++;
            }
        tokens[token][cursor]=0;
        token++;

        //Block#1 точки входа
        if (block == 1 && token > 1)
        {
            if ( strcmp( tokens[0], info_entry_name ) == 0 )
            {
                /* название, по убыванию */
                camp_fname = strdup( fname );
                camp_name = strdup( tokens[0] );
                camp_desc = strdup( tokens[2] );
                temp_list = list_create( LIST_AUTO_DELETE, camp_delete_entry );
                camp_first = strdup( tokens[1] );
            }
        }

        //Block#2 результаты
        if (block == 2 && token > 1)
        {
            snprintf( outcomes[current_outcome], 256, "%s", tokens[0] );
            current_outcome++;
        }

        //Block#3 путь
        if (block == 3 && token > 1)
        {
            /* проверка ввода выбора */
            if ( atoi( tokens[1] ) == 0 )
            {
                /* записи */
                centry = calloc( 1, sizeof( Camp_Entry ) );
                centry->id = strdup( tokens[0] );
                centry->scen = strdup( tokens[2] );
                //первичный инструктаж
                if (strlen(tokens[3])>0)
                {
                    centry->brief = strdup( tokens[3] );
                }

                centry->nexts = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
                centry->prestige = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
                for (j = 0; j < current_outcome; j++)
                {
                    if ( strstr( tokens[current_outcome * j + 4], "END" ) )
                    {
                        // окончание записи
                        Camp_Entry *end_entry = calloc( 1, sizeof( Camp_Entry ) );
                        end_entry->id = strdup( tokens[current_outcome * j + 4] );
                        end_entry->brief = strdup( tokens[current_outcome * j + 4 + 2] );
                        // проверка наличия конечной записи в памяти кампании
                        Camp_Entry *search_entry;
                        int found = 0;
                        list_reset( temp_list );
                        while ( ( search_entry = list_next( temp_list ) ) )
                        {
                            if ( strcmp( search_entry->id, end_entry->id ) == 0 )
                                found = 1;
                        }
                        if ( !found )
                        {
                            list_add( temp_list, end_entry );
//                        fprintf(stderr, "%s\n", end_entry->brief );
                        }
                        // конечная запись для текущего сценария 'outcome'>'next_scen_id'
                        snprintf( str, sizeof(str), "%s>%s", outcomes[j], tokens[current_outcome * j + 4] );
                        list_add( centry->nexts, strdup( str ) );
                        snprintf( str, sizeof(str), "%s>%s", outcomes[j], tokens[current_outcome * j + 4 + 1] );
                        list_add( centry->prestige, strdup( str ) );
                    }
                    else
                    {
                        // линейный следующий сценарий 'outcome'>'next_scen_id'
                        snprintf( str, sizeof(str), "%s>%s", outcomes[j], tokens[current_outcome * j + 4] );
                        list_add( centry->nexts, strdup( str ) );
                        // линейный следующий сценарий престиж
                        snprintf( str, sizeof(str), "%s>%s", outcomes[j], tokens[current_outcome * j + 4 + 1] );
                        list_add( centry->prestige, strdup( str ) );
                    }
                    // разбор полетов
                    centry->descs = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
                    snprintf( str, sizeof(str), "%s>%s", outcomes[j], tokens[current_outcome * j + 4 + 2] );
                    list_add( centry->descs, strdup( str ) );
//                    fprintf(stderr, "%s\n", str );
                }
                list_add( temp_list, centry );
            }
            else
            {
                /* путь выбора */
                // TODO внедрение престижных затрат
                // запись выбора
                Camp_Entry *select_entry = calloc( 1, sizeof( Camp_Entry ) );
                select_entry->id = strdup( tokens[0] );
                // выберите следующие сценарии
                select_entry->nexts = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
                snprintf( str, sizeof(str), "%s>%s", tokens[0], tokens[4] );
                list_add( select_entry->nexts, strdup( str ) );
                snprintf( str, sizeof(str), "%s>%s", tokens[0], tokens[7] );
                list_add( select_entry->nexts, strdup( str ) );
                // выберите описания
                select_entry->descs = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
                snprintf( str, sizeof(str), "%s>%s", tokens[0], tokens[3] );
                list_add( select_entry->descs, strdup( str ) );
                snprintf( str, sizeof(str), "%s>%s", tokens[0], tokens[6] );
                list_add( select_entry->descs, strdup( str ) );
                // выберите "престиж" прибыль/стоимость
                select_entry->prestige = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
                snprintf( str, sizeof(str), "%s>%s", tokens[0], tokens[5] );
                list_add( select_entry->prestige, strdup( str ) );
                snprintf( str, sizeof(str), "%s>0", tokens[0] );
                list_add( select_entry->prestige, strdup( str ) );
                // проверка наличия записи выбора в памяти кампании
                Camp_Entry *search_entry;
                int found = 0;
                list_reset( temp_list );
                while ( ( search_entry = list_next( temp_list ) ) )
                {
                    if ( strcmp( search_entry->id, select_entry->id ) == 0 )
                        found = 1;
                }
                if ( !found )
                {
                    list_add( temp_list, select_entry );
//                 fprintf(stderr, "%s\n", select_entry->id );
                }
                // выберите запись для текущего сценария
                snprintf( str, sizeof(str), "%s>%s", outcomes[j], select_entry->id );
                list_add( centry->nexts, strdup( str ) );
            }
        }
    }

    //конечный узел
    fclose(inf);

    /* построить кампанию, начиная с выбранной точки входа */
    camp_entries = list_create( LIST_AUTO_DELETE, camp_delete_entry );
    list_reset( temp_list );
    /* поиск точки входа */
    for ( i = 0; i < temp_list->count; i++ )
    {
        centry = list_next( temp_list );
        if ( strcmp( centry->id, camp_first ) == 0 )
        {
            list_transfer( temp_list, camp_entries, centry );
            break;
        }
    }

    Camp_Entry *final_entry;
    char *next = 0, *ptr;
    list_reset( camp_entries );
    /* поиск последующих записей */
    while ( ( final_entry = list_next( camp_entries ) ) )
    {
        if ( final_entry->nexts && final_entry->nexts->count > 0 )
        {
            list_reset( final_entry->nexts );
            while ( (next = list_next( final_entry->nexts )) )
            {
                ptr = strchr( next, '>' );
                if ( ptr )
                {
//fprintf(stderr, "%s\n", next );
                    ptr++;
                    list_reset( temp_list );
                    while ( ( centry = list_next( temp_list ) ) )
                        if ( strcmp( ptr, centry->id ) == 0 )
                        {
                            list_transfer( temp_list, camp_entries, centry );
                        }
                }
            }
        }
    }

    /* уборка */
    camp_loaded = FIRST_SCENARIO;
    camp_verify_tree();

    return 1;
}

/*
====================================================================
Загрузите описание кампании в старом формате lgcam (новая выделенная строка)
и настройте setup :), за исключением типа, который устанавливается при выполнении
движком действия загрузки.
====================================================================
*/
char* camp_load_lgcam_info( const char *fname, const char *path )
{
    PData *pd;
    char *name, *desc;
    char *domain = 0;
    char *info = 0;
    if ( ( pd = parser_read_file( fname, path ) ) == 0 ) goto parser_failure;
    domain = determine_domain(pd, fname);
    locale_load_domain(domain, 0/*FIXME*/);
    if ( !parser_get_value( pd, "name", &name, 0 ) ) goto parser_failure;
    name = (char *)trd(domain, name);
    if ( !parser_get_value( pd, "desc", &desc, 0 ) ) goto parser_failure;
    desc = (char *)trd(domain, desc);
    if ( ( info = calloc( strlen( name ) + strlen( desc ) + 3, sizeof( char ) ) ) == 0 ) {
        fprintf( stderr, tr("Out of memory\n") );
        goto failure;
    }
    sprintf( info, "%s##%s", name, desc );
    strcpy( setup.fname, fname );
    setup.scen_state = 0;
    parser_free( &pd );
    free(domain);
    return info;
parser_failure:
    fprintf( stderr, "%s\n", parser_get_error() );
failure:
    if ( info ) free( info );
    parser_free( &pd );
    free(domain);
    return 0;
}

/*
====================================================================
Загрузите описание кампании в новом формате lgdcam (новая выделенная строка)
и настройте setup :), за исключением типа, который задается при выполнении
движком действия загрузки.
====================================================================
*/
char* camp_load_lgdcam_info( List *campaign_entries, const char *fname, const char *path, char *info_entry_name )
{
    FILE *inf;
    char line[MAX_SCENARIO_LINE],tokens[MAX_TOKENS][MAX_CAMPAIGN_TOKEN_LENGTH];
    int i,block=0,last_line_length=-1,cursor=0,token=0;
    int utf16 = 0, lines=0;

    inf=fopen(path,"rb");
    if (!inf)
    {
        fprintf( stderr, "Couldn't open scenario file\n");
        return 0;
    }

    while (load_line(inf,line,utf16)>=0)
    {
        lines++;
        //комментарии к полосе
        if (line[0]==0x23 || line[0]==0x09) { line[0]=0; }
        if (strlen(line)>0 && last_line_length==0)
        {
            block++;
        }
        last_line_length=strlen(line);
        token=0;
        cursor=0;
        for(i=0;i<strlen(line);i++)
            if (line[i]==0x09)
            {
                tokens[token][cursor]=0;
                token++;
                cursor=0;
            }
            else
            {
                tokens[token][cursor]=line[i];
                cursor++;
            }
        tokens[token][cursor]=0;
        token++;

        //точки входа
        if (block == 1 && token > 1)
        {
            if ( campaign_entries != 0 )
            {
                Name_Entry_Type *name_entry;
                name_entry = calloc( 1, sizeof( Name_Entry_Type ) );
                name_entry->file_name = strdup( fname );
                name_entry->internal_name = strdup( tokens[0] );
                list_add( campaign_entries, name_entry );
            }
            else if ( info_entry_name != 0 )
            {
                if ( strcmp( tokens[0], info_entry_name ) == 0 )
                {
                    fclose(inf);
                    char *info = calloc( strlen( tokens[0] ) + strlen( tokens[2] ) + 3, sizeof( char ) );
                    sprintf( info, "%s##%s", tokens[0], tokens[2] );
                    strcpy( setup.fname, fname );
                    strcpy( setup.camp_entry_name, info_entry_name );
                    setup.scen_state = 0;
                    return info;
                }
            }
        }
        //Block#5  путь
        if (block == 2)
        {
            if ( campaign_entries != 0 )
            {
                fclose(inf);
                return "1";
            }
        }
    }

    fclose(inf);

    return "1";
}

/*
====================================================================
Публичные
====================================================================
*/

/*
====================================================================
Удалить запись кампании.
====================================================================
*/
void camp_delete_entry( void *ptr )
{
    Camp_Entry *entry = (Camp_Entry*)ptr;
    if ( entry ) {
        if ( entry->id ) free( entry->id );
        if ( entry->scen ) free( entry->scen );
        if ( entry->brief ) free( entry->brief );
        if ( entry->core_transfer ) free( entry->core_transfer );
        if ( entry->nexts ) list_delete( entry->nexts );
        if ( entry->descs ) list_delete( entry->descs );
        free( entry );
    }
}

/* проверьте, есть ли у всех следующих записей соответствующая запись сценария */
void camp_verify_tree()
{
    int i;
    Camp_Entry *entry = 0;
    char *next = 0, *ptr;

    for ( i = 0; i < camp_entries->count; i++ )
    {
        entry = list_get( camp_entries, i );
        if ( entry->nexts && entry->nexts->count > 0 )
        {
            list_reset( entry->nexts );
            while ( (next = list_next(entry->nexts)) )
            {
                ptr = strchr( next, '>' );
                if ( ptr )
                    ptr++;
                else
                    ptr = next;
                if ( camp_get_entry(ptr) == 0 )
                    printf( "  (is a 'next' entry in scenario %s)\n", entry->id );
            }
        }
    }
}

/*
====================================================================
Загрузить кампанию.
====================================================================
*/
int camp_load( const char *fname, const char *camp_entry )
{
    char *path, *extension;
    path = calloc( 256, sizeof( char ) );
    extension = calloc( 10, sizeof( char ) );
    search_file_name( path, extension, fname, config.mod_name, "Scenario", 'c' );
    if ( strcmp( extension, "lgcam" ) == 0 )
        return camp_load_lgcam( fname, path );
    else if ( strcmp( extension, "pgcam" ) == 0 )
        return parse_pgcam( fname, path, camp_entry );
    else if ( strcmp( extension, "lgdcam" ) == 0 )
        return camp_load_lgdcam( fname, path, camp_entry );
    return 0;
}

/*
====================================================================
Загрузите описание кампании.
====================================================================
*/
char *camp_load_info( char *fname, char *camp_entry )
{
    char path[MAX_PATH], extension[10];
    search_file_name( path, extension, fname, config.mod_name, "Scenario", 'c' );
    if ( strcmp( extension, "lgcam" ) == 0 )
    {
        return camp_load_lgcam_info( fname, path );
    }
    else if ( strcmp( extension, "pgcam" ) == 0 )
    {
        return parse_pgcam_info( 0, fname, path, camp_entry );
    }
    else if ( strcmp( extension, "lgdcam" ) == 0 )
    {
        return camp_load_lgdcam_info( 0, fname, path, camp_entry );
    }
    return 0;
}

void camp_delete()
{
    if ( camp_fname ) free( camp_fname ); camp_fname = 0;
    if ( camp_name ) free( camp_name ); camp_name = 0;
    if ( camp_desc ) free( camp_desc ); camp_desc = 0;
    if ( camp_authors ) free( camp_authors ); camp_authors = 0;
    if ( camp_entries ) list_delete( camp_entries ); camp_entries = 0;
    camp_loaded = NO_CAMPAIGN;
    camp_cur_scen = 0;
    free(camp_first); camp_first = 0;

}

/*
====================================================================
Запросите следующую запись сценария кампании по этому результату для текущей
записи. Если 'идентификатор' является нулем первая запись называется "первый" загружается.
====================================================================
*/
Camp_Entry *camp_get_entry( const char *id )
{
    const char *search_str = id ? id : camp_first;
    Camp_Entry *entry;
    list_reset( camp_entries );
    while ( ( entry = list_next( camp_entries ) ) )
        if ( strcmp( entry->id, search_str ) == 0 ) {
            return entry;
        }
    fprintf( stderr, tr("Campaign entry '%s' not found in campaign '%s'\n"), search_str, camp_name );
    return 0;
}

/*
====================================================================
Возвращает список идентификаторов результатов для текущей записи кампании.
(List of char *)
====================================================================
*/
List *camp_get_result_list()
{
    List *list = list_create( LIST_AUTO_DELETE, LIST_NO_CALLBACK );
    if (camp_cur_scen->nexts) {
        const char *str;
        list_reset( camp_cur_scen->nexts );
        while ( ( str = list_next( camp_cur_scen->nexts ) ) ) {
            List *tokens;
            if ( ( tokens = parser_split_string( str, ">" ) ) ) {
                list_add( list, strdup(list_first( tokens )) );
                list_delete( tokens );
            }
        }
    }
    return list;
}

/*
====================================================================
Возвращает текстовое описание результата или 0, если описание отсутствует.
====================================================================
*/
const char *camp_get_description(const char *result)
{
    const char *ret = 0;
    if (!result) return 0;
    if (camp_cur_scen->descs) {
        const char *str;
        list_reset( camp_cur_scen->descs );
        while ( ( str = list_next( camp_cur_scen->descs ) ) ) {
            List *tokens;
            if ( ( tokens = parser_split_string( str, ">" ) ) ) {
                int found = 0;
                if ( STRCMP( result, list_first( tokens ) ) ) {
                    ret = strchr( str, '>' ) + 1;
                    found = 1;
                }
                list_delete( tokens );
                if ( found ) break;
            }
        }
    }
    return ret;
}

/*
====================================================================
Установите следующую запись сценария, выполнив поиск результатов в текущей
записи сценария. Если 'идентификатор' значение null запись "первый" загружается
====================================================================
*/
int camp_set_next( const char *id )
{
    List *tokens;
    char *next_str;
    int found = 0;
    if ( id == 0 )
        camp_cur_scen = camp_get_entry( camp_first );
    else {
        if (!camp_cur_scen->nexts) return 0;
        list_reset( camp_cur_scen->nexts );
        while ( ( next_str = list_next( camp_cur_scen->nexts ) ) ) {
            if ( ( tokens = parser_split_string( next_str, ">" ) ) ) {
                if ( STRCMP( id, list_first( tokens ) ) ) {
                    camp_cur_scen = camp_get_entry( list_get( tokens, 2 ) );
                    found = 1;
                }
                list_delete( tokens );
                if ( found ) break;
            }
        }
    }
    return camp_cur_scen != 0;
}

/*
====================================================================
Установка текущего сценария лагерь сцен идентификатор записи.
====================================================================
*/
void camp_set_cur( const char *id )
{
    camp_cur_scen = camp_get_entry( id );
}
