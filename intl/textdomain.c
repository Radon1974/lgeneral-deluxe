/* Реализация функции textdomain (3).
   Авторские права (C) 1995, 1996, 1997, 1998 Free Software Foundation, Inc.
   Написано Ульрихом Дреппером <drepper@gnu.ai.mit.edu>, 1995 г.

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

#if defined STDC_HEADERS || defined _LIBC
# include <stdlib.h>
#endif

#if defined STDC_HEADERS || defined HAVE_STRING_H || defined _LIBC
# include <string.h>
#else
# include <strings.h>
# ifndef memcpy
#  define memcpy(Dst, Src, Num) bcopy (Src, Dst, Num)
# endif
#endif

#ifdef _LIBC
# include <libintl.h>
#else
# include "libgettext.h"
#endif

/* Внутренние переменные в автономном libintl.a должны иметь разные
   имена, чем внутренние переменные в GNU libc, в противном случае программы
   использование libintl.a не может быть связано статически.  */
#if !defined _LIBC
# define _nl_default_default_domain _nl_default_default_domain__
# define _nl_current_default_domain _nl_current_default_domain__
#endif

/* @@ конец пролога @@ */

/* Имя текстового домена по умолчанию.  */
extern const char _nl_default_default_domain[];

/* Текстовый домен по умолчанию, в котором должны быть найдены записи для gettext (3).  */
extern const char *_nl_current_default_domain;


/* Имена для функций libintl представляют собой проблему. Они не должны конфликтовать
   с существующими именами, и они должны соответствовать ANSI C. Но этот источник
   код также используется в библиотеке GNU C, где имена имеют __
   приставка. Так что здесь мы должны что-то изменить.  */
#ifdef _LIBC
# define TEXTDOMAIN __textdomain
# ifndef strdup
#  define strdup(str) __strdup (str)
# endif
#else
# define TEXTDOMAIN textdomain__
#endif

/* Установите для текущего каталога сообщений по умолчанию значение DOMAINNAME.
   Если DOMAINNAME имеет значение null, вернуть текущее значение по умолчанию.
   Если DOMAINNAME - "", сбросить на "сообщения" по умолчанию.  */
char *
TEXTDOMAIN (domainname)
     const char *domainname;
{
  char *old;

  /* Указатель NULL запрашивает текущую настройку.  */
  if (domainname == NULL)
    return (char *) _nl_current_default_domain;

  old = (char *) _nl_current_default_domain;

  /* Если имя домена представляет собой пустую строку, устанавливается значение домена по умолчанию «messages».  */
  if (domainname[0] == '\0'
      || strcmp (domainname, _nl_default_default_domain) == 0)
    _nl_current_default_domain = _nl_default_default_domain;
  else
    {
      /* Если следующий malloc не работает `_nl_current_default_domain '
будет NULL. Это значение будет возвращено и сигнализирует о том, что мы
вне ядра.  */
#if defined _LIBC || defined HAVE_STRDUP
      _nl_current_default_domain = strdup (domainname);
#else
      size_t len = strlen (domainname) + 1;
      char *cp = (char *) malloc (len);
      if (cp != NULL)
	memcpy (cp, domainname, len);
      _nl_current_default_domain = cp;
#endif
    }

  if (old != _nl_default_default_domain)
    free (old);

  return (char *) _nl_current_default_domain;
}

#ifdef _LIBC
/* Псевдоним для имени функции в библиотеке GNU C.  */
weak_alias (__textdomain, textdomain);
#endif
