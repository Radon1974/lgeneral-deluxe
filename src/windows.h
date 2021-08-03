/***************************************************************************
                          windows.h  -  description
                             -------------------
    begin                : Tue Mar 21 2002
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

#ifndef __WINDOWS_H
#define __WINDOWS_H

#include "image.h"

enum {
    ARRANGE_COLUMNS = 0,
    ARRANGE_ROWS,
    ARRANGE_INSIDE
};

/*
====================================================================
Метка. Рамка с нарисованным на ней текстом.
====================================================================
*/
typedef struct {
    Frame *frame;
    Font  *def_font;
} Label;

/*
====================================================================
Создайте этикетку в рамке.
По умолчанию метка скрыта и пуста.
Текущая позиция отрисовки - x, y в "surf".
====================================================================
*/
Label *label_create( SDL_Surface *frame, int alpha, Font *def_font, SDL_Surface *surf, int x, int y );
void label_delete( Label **label );

/*
====================================================================
Нарисовать этикетку
====================================================================
*/
#define label_hide( label, hide ) buffer_hide( label->frame->img->bkgnd, hide )
#define label_get_bkgnd( label ) buffer_get( label->frame->img->bkgnd )
#define label_draw_bkgnd( label ) buffer_draw( label->frame->img->bkgnd )
#define label_draw( label ) frame_draw( label->frame )

/*
====================================================================
Изменить настройки ярлыка
====================================================================
*/
#define label_move( label, x, y ) buffer_move( label->frame->img->bkgnd, x, y )
#define label_set_surface( label, surf ) buffer_set_surface( label->frame->img->bkgnd, surf )

/*
====================================================================
Напишите текст как содержимое метки и установите hide = 0.
Если 'font' равен NULL, используется указатель шрифта по умолчанию.
====================================================================
*/
void label_write( Label *label, Font *font, const char *text );

/*
====================================================================
Группа кнопок. Рамка с несколькими кнопками.
====================================================================
*/
typedef struct {
    SDL_Rect button_rect;   /* область на поверхности кнопки */
    SDL_Rect surf_rect;     /* регион в buffer::surface */
    int id;                 /* если возвращается при нажатии */
    char tooltip_left[32];  /* отображается как краткая справка */
    char tooltip_center[32];/* отображается как краткая справка */
    char tooltip_right[32]; /* отображается как краткая справка */
    char **lock_info;       /* справочная информация для кнопок переключения */
    int active;             /* истина, если кнопка может быть использована */
    int down;               /* последовательные числа - разные государственные позиции */
    int lock;               /* удерживайте кнопку, когда отпустите */
    int states;             /* количество возможных состояний для этой кнопки; 2 кнопки состояния наиболее распространены */
    int hidden;             /* эта кнопка скрыта и неактивна в меню */
} Button;
typedef struct {
    Frame *frame;       /* кадр группы */
    SDL_Surface *img;   /* поверхность кнопки. строка - это кнопка, столбец - это состояние */
    int w, h;           /* размер кнопки */
    Button *buttons;
    int button_count;   /* количество кнопок */
    int button_limit;   /* предел кнопок */
    int base_id;
} Group;

/*
====================================================================
Создайте группу кнопок. На максимальном уровне могут быть добавлены кнопки «limit».
Всплывающая подсказка кнопок отображается в «метке».
Фактический идентификатор кнопки вычисляется как base_id + id.
====================================================================
*/
Group *group_create( SDL_Surface *frame, int alpha, SDL_Surface *img, int w, int h, int limit, int base_id,
                     SDL_Surface *surf, int x, int y );
void group_delete( Group **group );

/*
====================================================================
Добавьте кнопку в точке x, y в группе. Если установлен lock_info, эта кнопка является переключателем.
'id' * group :: h - смещение кнопки по оси y.
====================================================================
*/
int group_add_button( Group *group, int id, int x, int y, char **lock_info, const char *tooltip, int states );

/*
====================================================================
Добавьте кнопку в точке x, y в группе. Если установлен lock_info, эта кнопка является переключателем.
'icon_id' используется для значка кнопки вместо 'id' (позволяет
несколько кнопок одного значка)
====================================================================
*/
int group_add_button_complex( Group *group, int id, int icon_id, int x, int y,
                              char **lock_info, const char *tooltip, int states );

/*
====================================================================
Получить кнопку по глобальному идентификатору.
====================================================================
*/
Button* group_get_button( Group *group, int id );

/*
====================================================================
Установить активное состояние кнопки по глобальному идентификатору.
====================================================================
*/
void group_set_active( Group *group, int id, int active );

/*
====================================================================
Установить скрытое состояние кнопки по глобальному идентификатору.
====================================================================
*/
void group_set_hidden( Group *group, int id, int hidden );

/*
====================================================================
Кнопка блокировки.
====================================================================
*/
void group_lock_button( Group *group, int id, int down );

/*
====================================================================
Проверьте событие движения и измените кнопки и метку.
Верните True, если в этом кадре.
====================================================================
*/
int group_handle_motion( Group *group, int x, int y );
/*
====================================================================
Проверить событие клика. Вернуть True, если была нажата кнопка, и вернуться
кнопка.
====================================================================
*/
int group_handle_button( Group *group, int button_id, int x, int y, Button **button );

/*
====================================================================
Нарисуйте группу.
====================================================================
*/
#define group_hide( group, hide ) buffer_hide( group->frame->img->bkgnd, hide )
#define group_get_bkgnd( group ) buffer_get( group->frame->img->bkgnd )
#define group_draw_bkgnd( group ) buffer_draw( group->frame->img->bkgnd )
void group_draw( Group *group );

/*
====================================================================
Изменить / получить настройки.
====================================================================
*/
#define group_set_surface( group, surf ) buffer_set_surface( group->frame->img->bkgnd, surf )
#define group_get_geometry(group,x,y,w,h) buffer_get_geometry(group->frame->img->bkgnd,x,y,w,h)
void group_move( Group *group, int x, int y );
#define group_get_width( group ) frame_get_width( (group)->frame )
#define group_get_height( group ) frame_get_height( (group)->frame )

/*
====================================================================
Верните True, если x, y находятся на групповой кнопке.
====================================================================
*/
int button_focus( Button *button, int x, int y );


/*
====================================================================
Редактируемая этикетка.
====================================================================
*/
typedef struct {
    Label *label;
    char  *text;    /* текстовый буфер */
    int   limit;    /* максимальный размер текста */
    int   blink;    /* если отображается истинный курсор (мигает) */
    int   blink_time; /* используется для переключения мигания */
    int   cursor_pos; /* позиция в тексте */
    int   cursor_x;   /* позиция на этикетке */
    int   last_sym;   /* keysym, если последний ключ (или -1, если его нет) */
    int   last_mod;   /* флаги модификаторов последней клавиши */
    int   last_uni;   /* юникод */
    int   delay;      /* задержка между ключевыми событиями при нажатии клавиши */
} Edit;

/*
====================================================================
Создано редактировать.
====================================================================
*/
Edit *edit_create( SDL_Surface *frame, int alpha, Font *font, int limit, SDL_Surface *surf, int x, int y );
void edit_delete( Edit **edit );

/*
====================================================================
Нарисовать этикетку
====================================================================
*/
#define edit_hide( edit, hide ) buffer_hide( edit->label->frame->img->bkgnd, hide )
#define edit_get_bkgnd( edit ) buffer_get( edit->label->frame->img->bkgnd )
#define edit_draw_bkgnd( edit ) buffer_draw( edit->label->frame->img->bkgnd )
void edit_draw( Edit *edit );

/*
====================================================================
Изменить настройки ярлыка
====================================================================
*/
#define edit_move( edit, x, y ) buffer_move( edit->label->frame->img->bkgnd, x, y )
#define edit_set_surface( edit, surf ) buffer_set_surface( edit->label->frame->img->bkgnd, surf )

/*
====================================================================
Установите буфер и отрегулируйте курсор x. Если 'newtext' равен NULL, текущий
отображается текст.
====================================================================
*/
void edit_show( Edit *edit, const char *newtext );

/*
====================================================================
Измените текстовый буфер в соответствии с юникодом, символом ключа и модификатором.
====================================================================
*/
void edit_handle_key( Edit *edit, int keysym, int modifier, int unicode );

/*
====================================================================
Обновите мигающий курсор и добавьте клавиши при нажатии. Вернуть True, если
ключ был написан.
====================================================================
*/
int edit_update( Edit *edit, int ms );


/*
====================================================================
Список
====================================================================
*/
typedef struct _LBox {
    Group *group;       /* базовая рамка + кнопки вверх / вниз */
    /* отбор */
    List *items;        /* список предметов */
    void *cur_item;     /* текущий выделенный элемент */
    /* клетки */
    int step;           /* прыгать на это количество клеток, если колесо вверх / вниз */
    int cell_offset;    /* идентификатор первого отображаемого элемента */
    int cell_count;     /* количество ячеек */
    int cell_x, cell_y; /* нарисовать позицию первой ячейки */
    int cell_w, cell_h; /* размер ячейки */
    int cell_gap;       /* промежуток между ячейками (вертикальный) */
    int cell_color;     /* цвет выделения */
    SDL_Surface *cell_buffer; /* буфер, используемый для рендеринга одной ячейки */
    void  (*render_cb)( void*, SDL_Surface* ); /* рендерить элемент на поверхность */
} LBox;

/*
====================================================================
Создайте список с несколькими ячейками. Список предметов
По умолчанию NULL.
====================================================================
*/
LBox *lbox_create( SDL_Surface *frame, int alpha, int border, SDL_Surface *buttons, int button_w, int button_h,
                   int cell_count, int step, int cell_w, int cell_h, int cell_gap, int cell_color,
                   void (*cb)(void*, SDL_Surface*),
                   SDL_Surface *surf, int x, int y );
void lbox_delete( LBox **lbox );

/*
====================================================================
Список рисования
====================================================================
*/
#define lbox_hide( lbox, hide ) buffer_hide( lbox->group->frame->img->bkgnd, hide )
#define lbox_get_bkgnd( lbox ) buffer_get( lbox->group->frame->img->bkgnd )
#define lbox_draw_bkgnd( lbox ) buffer_draw( lbox->group->frame->img->bkgnd )
#define lbox_draw( lbox ) group_draw( lbox->group )

/*
====================================================================
Изменить / получить настройки списка
====================================================================
*/
#define lbox_set_surface( lbox, surf ) buffer_set_surface( lbox->group->frame->img->bkgnd, surf )
#define lbox_move( lbox, x, y ) group_move( lbox->group, x, y )
#define lbox_get_width( lbox ) group_get_width( (lbox)->group )
#define lbox_get_height( lbox ) group_get_height( (lbox)->group )
#define lbox_get_selected_item( lbox ) ( (lbox)->cur_item )
#define lbox_get_selected_item_index( lbox ) \
	(list_check( (lbox)->items, (lbox)->cur_item ))
#define lbox_clear_selected_item( lbox ) \
	do { (lbox)->cur_item = 0; lbox_apply(lbox); } while (0)
void *lbox_select_first_item( LBox *lbox );
#define lbox_is_empty( lbox ) \
	((lbox)->items == NULL || (lbox)->items->count == 0)
#define lbox_select_item(lbox, item) \
	do { (lbox)->cur_item = (item); lbox_apply(lbox); } while (0)

/*
====================================================================
Восстановить графику списка (lbox->group->frame).
====================================================================
*/
void lbox_apply( LBox *lbox );

/*
====================================================================
Удалите старый список элементов (если есть) и используйте новый (будет
удалено этим списком)
====================================================================
*/
void lbox_set_items( LBox *lbox, List *items );
#define lbox_clear_items( lbox ) lbox_set_items( lbox, NULL )

/*
====================================================================
handle_motion устанавливает кнопку, если вверх / вниз находится фокус, и возвращает элемент
если таковые были выбраны.
====================================================================
*/
int lbox_handle_motion( LBox *lbox, int cx, int cy, void **item );
/*
====================================================================
handle_button внутренне обрабатывает щелчок вверх / вниз и возвращает элемент
если один был выбран.
====================================================================
*/
int lbox_handle_button( LBox *lbox, int button_id, int cx, int cy, Button **button, void **item );

/** Render listbox item @item (which is a simple string) to listbox cell
 * surface @buffer. */
void lbox_render_text( void *item, SDL_Surface *buffer );


/*
====================================================================
Файловый диалог с пробелом для отображения информации о файле
(например, информация о сценарии)
====================================================================
*/
typedef struct {
    LBox *lbox;                         /* список файлов */
    Group *group;                       /* информационная рамка с кнопками ОК / Отмена */
    char root[1024];                    /* корневая директория */
    char subdir[1024];                  /* текущий каталог в корневом каталоге */
    void (*file_cb)( const char*, const char*, SDL_Surface* );
                                        /* вызывается, если файл был выбран
                                               дан полный путь */
    int info_x, info_y;                 /* информационная позиция */
    SDL_Surface *info_buffer;           /* буфер информации */
    int button_x, button_y;             /* положение первой кнопки (правый верхний угол,
                                           слева добавлены новые кнопки) */
    int button_dist;                    /* расстояние между кнопками (промежуток + ширина) */
    int emptyFile;                      /* указывает, требует ли этот файловый диалог пустой файл */
    int arrangement;                    /* расположение двух окон
                                           0: расположены рядом друг с другом
                                           1: расположены один над другим */
    int dir_only;                       /* этот файловый диалог ищет только папки */
    char *current_name;                 /* текущий выбранный файл */
    int file_type;                      /* показать определенный тип файла в этом диалоговом окне файла */
} FDlg;

/*
====================================================================
Создайте файл-диалог. По умолчанию список пуст.
Первые две кнопки в conf_buttons добавляются как ok и cancel.
Для всех остальных кнопок в conf_buttons есть пробелы
зарезервировано, и эти кнопки могут быть добавлены с помощью fdlg_add_button ().
====================================================================
*/
FDlg *fdlg_create(
                   SDL_Surface *lbox_frame, int alpha, int border,
                   SDL_Surface *lbox_buttons, int lbox_button_w, int lbox_button_h,
                   int cell_h,
                   SDL_Surface *conf_frame,
                   SDL_Surface *conf_buttons, int conf_button_w, int conf_button_h,
                   int id_ok,
                   void (*lbox_cb)( void*, SDL_Surface* ),
                   void (*file_cb)( const char*, const char*, SDL_Surface* ),
                   SDL_Surface *surf, int x, int y, int arrangement, int emptyFile, int dir_only, int file_type );
void fdlg_delete( FDlg **fdlg );

/*
====================================================================
Диалог рисования файла
====================================================================
*/
void fdlg_hide( FDlg *fdlg, int hide );
void fdlg_get_bkgnd( FDlg *fdlg );
void fdlg_draw_bkgnd( FDlg *fdlg );
void fdlg_draw( FDlg *fdlg );

/*
====================================================================
Изменить настройки диалогового окна файла
====================================================================
*/
void fdlg_set_surface( FDlg *fdlg, SDL_Surface *surf );
void fdlg_move( FDlg *fdlg, int x, int y );

/*
====================================================================
Добавить кнопку. Графика взята из conf_buttons.
====================================================================
*/
void fdlg_add_button( FDlg *fdlg, int id, char **lock_info, const char *tooltip );

/*
====================================================================
Показать диалог с файлом в корне каталога.
====================================================================
*/
void fdlg_open( FDlg *fdlg, const char *root, const char *subdir );

/*
====================================================================
handle_motion обновляет фокус кнопок
====================================================================
*/
int fdlg_handle_motion( FDlg *fdlg, int cx, int cy );
/*
====================================================================
handle_button
====================================================================
*/
int fdlg_handle_button( FDlg *fdlg, int button_id, int cx, int cy, Button **button );


/*
====================================================================
Диалог настройки
====================================================================
*/
typedef struct {
    LBox *list;
    Group *ctrl;
    Group *module;
    Group *confirm;
    void (*select_cb)(void*); /* призвал при выборе элемента */
    int sel_id; /* id выбранной записи */
} SDlg;

/*
====================================================================
Создайте диалог настройки.
====================================================================
*/
SDlg *sdlg_create( SDL_Surface *list_frame, SDL_Surface *list_buttons,
                   int list_button_w, int list_button_h, int cell_h,
                   SDL_Surface *ctrl_frame, SDL_Surface *ctrl_buttons,
                   int ctrl_button_w, int ctrl_button_h, int id_ctrl,
                   SDL_Surface *mod_frame, SDL_Surface *mod_buttons,
                   int mod_button_w, int mod_button_h, int id_mod,
                   SDL_Surface *conf_frame, SDL_Surface *conf_buttons,
                   int conf_button_w, int conf_button_h, int id_conf,
                   void (*list_render_cb)(void*,SDL_Surface*),
                   void (*list_select_cb)(void*),
                   SDL_Surface *surf, int x, int y );
SDlg *sdlg_camp_create( SDL_Surface *conf_frame, SDL_Surface *conf_buttons,
                   int conf_button_w, int conf_button_h, int id_conf,
                   void (*list_render_cb)(void*,SDL_Surface*),
                   void (*list_select_cb)(void*),
                   SDL_Surface *surf, int x, int y );
void sdlg_delete( SDlg **sdlg );

/*
====================================================================
Нарисуйте диалог настройки.
====================================================================
*/
void sdlg_hide( SDlg *sdlg, int hide );
void sdlg_get_bkgnd( SDlg *sdlg );
void sdlg_draw_bkgnd( SDlg *sdlg );
void sdlg_draw( SDlg *sdlg );

/*
====================================================================
Диалог настройки сценария
====================================================================
*/
void sdlg_set_surface( SDlg *sdlg, SDL_Surface *surf );
void sdlg_move( SDlg *sdlg, int x, int y );

/*
====================================================================
handle_motion обновляет фокус кнопок
====================================================================
*/
int sdlg_handle_motion( SDlg *sdlg, int cx, int cy );
/*
====================================================================
handle_button
====================================================================
*/
int sdlg_handle_button( SDlg *sdlg, int button_id, int cx, int cy, Button **button );

/*
====================================================================
Диалог выбора:
Список для выбора с помощью кнопок ОК / Отмена
====================================================================
*/
typedef struct {
	Group *button_group; /* кнопки ОК / Отмена */
	LBox *select_lbox;
} SelectDlg;

SelectDlg *select_dlg_create(
	SDL_Surface *lbox_frame,
	SDL_Surface *lbox_buttons, int lbox_button_w, int lbox_button_h,
	int lbox_cell_count, int lbox_cell_w, int lbox_cell_h,
	void (*lbox_render_cb)(void*, SDL_Surface*),
	SDL_Surface *conf_frame,
	SDL_Surface *conf_buttons, int conf_button_w, int conf_button_h,
	int id_ok,
	SDL_Surface *surf, int x, int y );
void select_dlg_delete( SelectDlg **sdlg );
int select_dlg_get_width(SelectDlg *sdlg);
int select_dlg_get_height(SelectDlg *sdlg);
void select_dlg_move( SelectDlg *sdlg, int px, int py);
void select_dlg_hide( SelectDlg *sdlg, int value);
void select_dlg_draw( SelectDlg *sdlg);
void select_dlg_draw_bkgnd( SelectDlg *sdlg);
void select_dlg_get_bkgnd( SelectDlg *sdlg);
int select_dlg_handle_motion( SelectDlg *sdlg, int cx, int cy);
int select_dlg_handle_button( SelectDlg *sdlg, int bid, int cx, int cy,
	Button **pbtn );
#define select_dlg_set_items( sdlg, items ) \
	lbox_set_items( sdlg->select_lbox, items )
#define select_dlg_clear_items( sdlg ) \
	lbox_clear_items( sdlg->select_lbox )
#define select_dlg_get_selected_item( sdlg ) \
	lbox_get_selected_item( sdlg->select_lbox )
#define select_dlg_get_selected_item_index( sdlg ) \
	lbox_get_selected_item_index( sdlg->select_lbox )

#endif
