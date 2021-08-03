/* Реализация функции dgettext (3)
   Авторское право (C) 1995, 1996, 1997 Free Software Foundation, Inc.

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

#if defined HAVE_LOCALE_H || defined _LIBC
# include <locale.h>
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
# define DGETTEXT __dgettext
# define DCGETTEXT __dcgettext
#else
# define DGETTEXT dgettext__
# define DCGETTEXT dcgettext__
#endif

/* Найдите MSGID в каталоге сообщений DOMAINNAME текущего
   LC_MESSAGES языковой стандарт.  */
char *
DGETTEXT (domainname, msgid)
     const char *domainname;
     const char *msgid;
{
  return DCGETTEXT (domainname, msgid, LC_MESSAGES);
}

#ifdef _LIBC
/* Псевдоним для имени функции в библиотеке GNU C.  */
weak_alias (__dgettext, dgettext);
#endif
