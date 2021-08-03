/* Реализация функции bindtextdomain (3)
   Авторские права (C) 1995, 1996, 1997, 1998 Free Software Foundation, Inc.

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
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# else
void free ();
# endif
#endif

#if defined HAVE_STRING_H || defined _LIBC
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
#include "gettext.h"
#include "gettextP.h"

/* Внутренние переменные в автономном libintl.a должны иметь разные
   имена, чем внутренние переменные в GNU libc, в противном случае программы
   использование libintl.a не может быть связано статически.  */
#if !defined _LIBC
# define _nl_default_dirname _nl_default_dirname__
# define _nl_domain_bindings _nl_domain_bindings__
#endif

/* @@ конец пролога @@ */

/* Содержит расположение каталогов сообщений по умолчанию.  */
extern const char _nl_default_dirname[];

/* Список с привязками конкретных доменов.  */
extern struct binding *_nl_domain_bindings;


/* Имена для функций libintl представляют собой проблему. Они не должны конфликтовать
   с существующими именами, и они должны соответствовать ANSI C. Но этот источник
   код также используется в библиотеке GNU C, где имена имеют __
   приставка. Так что здесь мы должны что-то изменить.  */
#ifdef _LIBC
# define BINDTEXTDOMAIN __bindtextdomain
# ifndef strdup
#  define strdup(str) __strdup (str)
# endif
#else
# define BINDTEXTDOMAIN bindtextdomain__
#endif

/* Укажите, что будет найден каталог сообщений DOMAINNAME
   в DIRNAME, а не в базе данных локали системы.  */
char *
BINDTEXTDOMAIN (domainname, dirname)
     const char *domainname;
     const char *dirname;
{
  struct binding *binding;

  /* Некоторые проверки вменяемости.  */
  if (domainname == NULL || domainname[0] == '\0')
    return NULL;

  for (binding = _nl_domain_bindings; binding != NULL; binding = binding->next)
    {
      int compare = strcmp (domainname, binding->domainname);
      if (compare == 0)
	/* Мы нашли это!  */
	break;
      if (compare < 0)
	{
	  /* Его нет в списке.  */
	  binding = NULL;
	  break;
	}
    }

  if (dirname == NULL)
    /* Текущая привязка должна быть возвращена.  */
    return binding == NULL ? (char *) _nl_default_dirname : binding->dirname;

  if (binding != NULL)
    {
      /* Домен уже привязан. Если новое значение и старое
равны мы просто ничего не делаем. В противном случае замените
старый переплет.  */
      if (strcmp (dirname, binding->dirname) != 0)
	{
	  char *new_dirname;

	  if (strcmp (dirname, _nl_default_dirname) == 0)
	    new_dirname = (char *) _nl_default_dirname;
	  else
	    {
#if defined _LIBC || defined HAVE_STRDUP
	      new_dirname = strdup (dirname);
	      if (new_dirname == NULL)
		return NULL;
#else
	      size_t len = strlen (dirname) + 1;
	      new_dirname = (char *) malloc (len);
	      if (new_dirname == NULL)
		return NULL;

	      memcpy (new_dirname, dirname, len);
#endif
	    }

	  if (binding->dirname != _nl_default_dirname)
	    free (binding->dirname);

	  binding->dirname = new_dirname;
	}
    }
  else
    {
      /* Нам нужно создать новую привязку.  */
#if !defined _LIBC && !defined HAVE_STRDUP
      size_t len;
#endif
      struct binding *new_binding =
	(struct binding *) malloc (sizeof (*new_binding));

      if (new_binding == NULL)
	return NULL;

#if defined _LIBC || defined HAVE_STRDUP
      new_binding->domainname = strdup (domainname);
      if (new_binding->domainname == NULL)
	return NULL;
#else
      len = strlen (domainname) + 1;
      new_binding->domainname = (char *) malloc (len);
      if (new_binding->domainname == NULL)
	return NULL;
      memcpy (new_binding->domainname, domainname, len);
#endif

      if (strcmp (dirname, _nl_default_dirname) == 0)
	new_binding->dirname = (char *) _nl_default_dirname;
      else
	{
#if defined _LIBC || defined HAVE_STRDUP
	  new_binding->dirname = strdup (dirname);
	  if (new_binding->dirname == NULL)
	    return NULL;
#else
	  len = strlen (dirname) + 1;
	  new_binding->dirname = (char *) malloc (len);
	  if (new_binding->dirname == NULL)
	    return NULL;
	  memcpy (new_binding->dirname, dirname, len);
#endif
	}

      /* Теперь поставьте его в очередь.  */
      if (_nl_domain_bindings == NULL
	  || strcmp (domainname, _nl_domain_bindings->domainname) < 0)
	{
	  new_binding->next = _nl_domain_bindings;
	  _nl_domain_bindings = new_binding;
	}
      else
	{
	  binding = _nl_domain_bindings;
	  while (binding->next != NULL
		 && strcmp (domainname, binding->next->domainname) > 0)
	    binding = binding->next;

	  new_binding->next = binding->next;
	  binding->next = new_binding;
	}

      binding = new_binding;
    }

  return binding->dirname;
}

#ifdef _LIBC
/* Псевдоним для имени функции в библиотеке GNU C.  */
weak_alias (__bindtextdomain, bindtextdomain);
#endif
