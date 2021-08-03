/* Реализация функции gettext (3).
   Авторское право (C) 1995, 1997 Free Software Foundation, Inc.

   Это бесплатное программное обеспечение; вы можете распространять и / или изменять
   это в соответствии с условиями Стандартной общественной лицензии GNU, опубликованной
   Фонд свободного программного обеспечения; либо версия 2, либо (на ваш выбор)
   любая более поздняя версия.

   Эта программа распространяется в надежде, что она будет полезной,
   но БЕЗ КАКИХ-ЛИБО ГАРАНТИЙ; даже без подразумеваемой гарантии
   КОММЕРЧЕСКАЯ ЦЕННОСТЬ или ПРИГОДНОСТЬ ДЛЯ ОПРЕДЕЛЕННОЙ ЦЕЛИ. Увидеть
   Стандартная общественная лицензия GNU для более подробной информации.

   Вы должны были получить копию Стандартной общественной лицензии GNU
   вместе с этой программой; если нет, напишите в Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Бостон, Массачусетс 02111-1307, США.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _LIBC
# define __need_NULL
# include <stddef.h>
#else
# ifdef STDC_HEADERS
#  include <stdlib.h>		/* Просто для NULL.  */
# else
#  ifdef HAVE_STRING_H
#   include <string.h>
#  else
#   define NULL ((void *) 0)
#  endif
# endif
#endif

#ifdef _LIBC
# include <libintl.h>
#else
# include "libgettext.h"
#endif

/* @@ конец пролога @@ */

/* Имена для функций libintl представляют собой проблему. Они не должны конфликтовать
   с существующими именами, и они должны соответствовать ANSI C. Но этот источник
   код также используется в библиотеке GNU C, где имена имеют __
   приставка. Так что здесь мы должны что-то изменить.  */
#ifdef _LIBC
# define GETTEXT __gettext
# define DGETTEXT __dgettext
#else
# define GETTEXT gettext__
# define DGETTEXT dgettext__
#endif

/* Найдите MSGID в текущем каталоге сообщений по умолчанию для текущего
   LC_MESSAGES языковой стандарт. Если не найден, возвращает сам MSGID (по умолчанию
   текст).  */
char *
GETTEXT (msgid)
     const char *msgid;
{
  return DGETTEXT (NULL, msgid);
}

#ifdef _LIBC
/* Псевдоним для имени функции в библиотеке GNU C.  */
weak_alias (__gettext, gettext);
#endif
