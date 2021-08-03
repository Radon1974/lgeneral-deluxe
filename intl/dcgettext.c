/* Реализация функции dcgettext (3).
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

#include <sys/types.h>

#ifdef __GNUC__
# define alloca __builtin_alloca
# define HAVE_ALLOCA 1
#else
# if defined HAVE_ALLOCA_H || defined _LIBC
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca
char *alloca ();
#   endif
#  endif
# endif
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif
#ifndef __set_errno
# define __set_errno(val) errno = (val)
#endif

#if defined STDC_HEADERS || defined _LIBC
# include <stdlib.h>
#else
char *getenv ();
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# else
void free ();
# endif
#endif

#if defined HAVE_STRING_H || defined _LIBC
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE	1
# endif
# include <string.h>
#else
# include <strings.h>
#endif
#if !HAVE_STRCHR && !defined _LIBC
# ifndef strchr
#  define strchr index
# endif
#endif

#if defined HAVE_UNISTD_H || defined _LIBC
# include <unistd.h>
#endif

#include "gettext.h"
#include "gettextP.h"
#ifdef _LIBC
# include <libintl.h>
#else
# include "libgettext.h"
#endif
#include "hash-string.h"

/* Внутренние переменные в автономном libintl.a должны иметь разные
   имена, чем внутренние переменные в GNU libc, в противном случае программы
   использование libintl.a не может быть связано статически.  */
#if !defined _LIBC
# define _nl_default_default_domain _nl_default_default_domain__
# define _nl_current_default_domain _nl_current_default_domain__
# define _nl_default_dirname _nl_default_dirname__
# define _nl_domain_bindings _nl_domain_bindings__
#endif

/* @@ конец пролога @@ */

#ifdef _LIBC
/* Переименуйте функции, отличные от ANSI C. Этого требует стандарт
   потому что некоторые функции ANSI C потребуют связи с этим объектом
   файл и пространство имен не должны быть загрязнены.  */
# define getcwd __getcwd
# ifndef stpcpy
#  define stpcpy __stpcpy
# endif
#else
# if !defined HAVE_GETCWD
char *getwd ();
#  define getcwd(buf, max) getwd (buf)
# else
char *getcwd ();
# endif
# ifndef HAVE_STPCPY
static char *stpcpy PARAMS ((char *dest, const char *src));
# endif
#endif

/* Величина увеличения размера буфера при каждой попытке.  */
#define PATH_INCR 32

/* Следующее взято из pathmax.h.  */
/* Системы BSD, отличные от POSIX, могут иметь файл limits.h, который не определяет
   PATH_MAX, но может вызвать предупреждения о переопределении, если sys / param.h
   позже включен (как в MORE / BSD 4.3).  */
#if defined(_POSIX_VERSION) || (defined(HAVE_LIMITS_H) && !defined(__GNUC__))
# include <limits.h>
#endif

#ifndef _POSIX_PATH_MAX
# define _POSIX_PATH_MAX 255
#endif

#if !defined(PATH_MAX) && defined(_PC_PATH_MAX)
# define PATH_MAX (pathconf ("/", _PC_PATH_MAX) < 1 ? 1024 : pathconf ("/", _PC_PATH_MAX))
#endif

/* Не включайте sys / param.h, если он уже был.  */
#if defined(HAVE_SYS_PARAM_H) && !defined(PATH_MAX) && !defined(MAXPATHLEN)
# include <sys/param.h>
#endif

#if !defined(PATH_MAX) && defined(MAXPATHLEN)
# define PATH_MAX MAXPATHLEN
#endif

#ifndef PATH_MAX
# define PATH_MAX _POSIX_PATH_MAX
#endif

/* XPG3 определяет результат setlocale (category, NULL) как:
   `` Направляет `setlocale () 'запрашивать` `категорию' 'и возвращать текущий
     установка `local '.' '
   Однако он не указывает точный формат. И даже хуже: POSIX
   это совсем не определяет. Таким образом, мы можем использовать эту функцию только на выбранных
   системы (например, те, кто использует библиотеку GNU C).  */
#ifdef _LIBC
# define HAVE_LOCALE_NULL
#endif

/* Имя домена по умолчанию, используемого для gettext (3) перед любым вызовом
   текстовый домен (3). Значение по умолчанию для этого - «сообщения».  */
const char _nl_default_default_domain[] = "messages";

/* Значение, используемое в качестве домена по умолчанию для gettext (3).  */
const char *_nl_current_default_domain = _nl_default_default_domain;

/* Содержит расположение каталогов сообщений по умолчанию.  */
const char _nl_default_dirname[] = GNULOCALEDIR;

/* Список с привязками определенных доменов, созданных bindtextdomain ()
   звонки.  */
struct binding *_nl_domain_bindings;

/* Прототипы для локальных функций.  */
static char *find_msg PARAMS ((struct loaded_l10nfile *domain_file,
			       const char *msgid)) internal_function;
static const char *category_to_name PARAMS ((int category)) internal_function;
static const char *guess_category_value PARAMS ((int category,
						 const char *categoryname))
     internal_function;


/* Для тех проигрывающих систем, у которых нет alloca, мы должны добавить
   некоторый дополнительный код, имитирующий его.  */
#ifdef HAVE_ALLOCA
/* Ничего не надо делать.  */
# define ADD_BLOCK(list, address) /* ничего */
# define FREE_BLOCKS(list) /* ничего */
#else
struct block_list
{
  void *address;
  struct block_list *next;
};
# define ADD_BLOCK(list, addr)						      \
  do {									      \
    struct block_list *newp = (struct block_list *) malloc (sizeof (*newp));  \
    /* Если мы не можем получить свободный блок, мы не можем добавить новый элемент в \
       список.  */							      \
    if (newp != NULL) {							      \
      newp->address = (addr);						      \
      newp->next = (list);						      \
      (list) = newp;							      \
    }									      \
  } while (0)
# define FREE_BLOCKS(list)						      \
  do {									      \
    while (list != NULL) {						      \
      struct block_list *old = list;					      \
      list = list->next;						      \
      free (old);							      \
    }									      \
  } while (0)
# undef alloca
# define alloca(size) (malloc (size))
#endif	/* иметь распределение */


/* Имена для функций libintl представляют собой проблему. Они не должны конфликтовать
   с существующими именами, и они должны соответствовать ANSI C. Но этот источник
   код также используется в библиотеке GNU C, где имена имеют __
   приставка. Так что здесь мы должны что-то изменить.  */
#ifdef _LIBC
# define DCGETTEXT __dcgettext
#else
# define DCGETTEXT dcgettext__
#endif

/* Найдите MSGID в каталоге сообщений DOMAINNAME для текущей CATEGORY
   локаль.  */
char *
DCGETTEXT (domainname, msgid, category)
     const char *domainname;
     const char *msgid;
     int category;
{
#ifndef HAVE_ALLOCA
  struct block_list *block_list = NULL;
#endif
  struct loaded_l10nfile *domain;
  struct binding *binding;
  const char *categoryname;
  const char *categoryvalue;
  char *dirname, *xdomainname;
  char *single_locale;
  char *retval;
  int saved_errno = errno;

  /* Если реальный MSGID не указан, верните NULL.  */
  if (msgid == NULL)
    return NULL;

  /* Если DOMAINNAME имеет значение NULL, нас интересует домен по умолчанию. Если
     CATEGORY - это не LC_MESSAGES, это может не иметь большого смысла, но
     определение оставило это неопределенным.  */
  if (domainname == NULL)
    domainname = _nl_current_default_domain;

  /* Сначала найдите соответствующую привязку.  */
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

  if (binding == NULL)
    dirname = (char *) _nl_default_dirname;
  else if (binding->dirname[0] == '/')
    dirname = binding->dirname;
  else
    {
      /* У нас есть относительный путь. Сделайте это сейчас абсолютным.  */
      size_t dirname_len = strlen (binding->dirname) + 1;
      size_t path_max;
      char *ret;

      path_max = (unsigned) PATH_MAX;
      path_max += 2;		/* Документы getcwd говорят об этом.  */

      dirname = (char *) alloca (path_max + dirname_len);
      ADD_BLOCK (block_list, dirname);

      __set_errno (0);
      while ((ret = getcwd (dirname, path_max)) == NULL && errno == ERANGE)
	{
	  path_max += PATH_INCR;
	  dirname = (char *) alloca (path_max + dirname_len);
	  ADD_BLOCK (block_list, dirname);
	  __set_errno (0);
	}

      if (ret == NULL)
	{
	  /* Мы не можем получить текущий рабочий каталог. Не подавайте сигнал
ошибка, но просто верните строку по умолчанию.  */
	  FREE_BLOCKS (block_list);
	  __set_errno (saved_errno);
	  return (char *) msgid;
	}

      stpcpy (stpcpy (strchr (dirname, '\0'), "/"), binding->dirname);
    }

  /* Теперь определите символическое имя CATEGORY и его значение.  */
  categoryname = category_to_name (category);
  categoryvalue = guess_category_value (category, categoryname);

  xdomainname = (char *) alloca (strlen (categoryname)
				 + strlen (domainname) + 5);
  ADD_BLOCK (block_list, xdomainname);

  stpcpy (stpcpy (stpcpy (stpcpy (xdomainname, categoryname), "/"),
		  domainname),
	  ".mo");

  /* Создание рабочей области.  */
  single_locale = (char *) alloca (strlen (categoryvalue) + 1);
  ADD_BLOCK (block_list, single_locale);


  /* Найдите данную строку. Это цикл, потому что мы, возможно,
     получил упорядоченный список языков для рассмотрения для перевода.  */
  while (1)
    {
      /* Сделайте так, чтобы CATEGORYVALUE указывала на следующий элемент списка.  */
      while (categoryvalue[0] != '\0' && categoryvalue[0] == ':')
	++categoryvalue;
      if (categoryvalue[0] == '\0')
	{
	  /* Поиск по всему содержанию CATEGORYVALUE, но
действительная запись не найдена. Решаем эту ситуацию
путем неявного добавления записи "C", т.е. без перевода
состоится.  */
	  single_locale[0] = 'C';
	  single_locale[1] = '\0';
	}
      else
	{
	  char *cp = single_locale;
	  while (categoryvalue[0] != '\0' && categoryvalue[0] != ':')
	    *cp++ = *categoryvalue++;
	  *cp = '\0';
	}

      /* Если текущее значение локали - C (или POSIX), мы не загружаем
домен. Верните MSGID.  */
      if (strcmp (single_locale, "C") == 0
	  || strcmp (single_locale, "POSIX") == 0)
	{
	  FREE_BLOCKS (block_list);
	  __set_errno (saved_errno);
	  return (char *) msgid;
	}


      /* Найдите структуру, описывающую каталог сообщений, соответствующую
ДОМЕННОЕ ИМЯ и КАТЕГОРИЯ.  */
      domain = _nl_find_domain (dirname, single_locale, xdomainname);

      if (domain != NULL)
	{
	  retval = find_msg (domain, msgid);

	  if (retval == NULL)
	    {
	      int cnt;

	      for (cnt = 0; domain->successor[cnt] != NULL; ++cnt)
		{
		  retval = find_msg (domain->successor[cnt], msgid);

		  if (retval != NULL)
		    break;
		}
	    }

	  if (retval != NULL)
	    {
	      FREE_BLOCKS (block_list);
	      __set_errno (saved_errno);
	      return retval;
	    }
	}
    }
  /* НЕ ДОСТУПНЫ */
}

#ifdef _LIBC
/* Псевдоним для имени функции в библиотеке GNU C.  */
weak_alias (__dcgettext, dcgettext);
#endif


static char *
internal_function
find_msg (domain_file, msgid)
     struct loaded_l10nfile *domain_file;
     const char *msgid;
{
  size_t top, act, bottom;
  struct loaded_domain *domain;

  if (domain_file->decided == 0)
    _nl_load_domain (domain_file);

  if (domain_file->data == NULL)
    return NULL;

  domain = (struct loaded_domain *) domain_file->data;

  /* Найдите MSGID и его перевод.  */
  if (domain->hash_size > 2 && domain->hash_tab != NULL)
    {
      /* Используйте хеш-таблицу.  */
      nls_uint32 len = strlen (msgid);
      nls_uint32 hash_val = hash_string (msgid);
      nls_uint32 idx = hash_val % domain->hash_size;
      nls_uint32 incr = 1 + (hash_val % (domain->hash_size - 2));
      nls_uint32 nstr = W (domain->must_swap, domain->hash_tab[idx]);

      if (nstr == 0)
	/* Запись в хеш-таблице пуста.  */
	return NULL;

      if (W (domain->must_swap, domain->orig_tab[nstr - 1].length) == len
	  && strcmp (msgid,
		     domain->data + W (domain->must_swap,
				       domain->orig_tab[nstr - 1].offset)) == 0)
	return (char *) domain->data + W (domain->must_swap,
					  domain->trans_tab[nstr - 1].offset);

      while (1)
	{
	  if (idx >= domain->hash_size - incr)
	    idx -= domain->hash_size - incr;
	  else
	    idx += incr;

	  nstr = W (domain->must_swap, domain->hash_tab[idx]);
	  if (nstr == 0)
	    /* Запись в хеш-таблице пуста.  */
	    return NULL;

	  if (W (domain->must_swap, domain->orig_tab[nstr - 1].length) == len
	      && strcmp (msgid,
			 domain->data + W (domain->must_swap,
					   domain->orig_tab[nstr - 1].offset))
	         == 0)
	    return (char *) domain->data
	      + W (domain->must_swap, domain->trans_tab[nstr - 1].offset);
	}
      /* НЕ ДОСТУПНЫ */
    }

  /* Теперь пробуем метод по умолчанию: бинарный поиск в отсортированном
     массив сообщений.  */
  bottom = 0;
  top = domain->nstrings;
  while (bottom < top)
    {
      int cmp_val;

      act = (bottom + top) / 2;
      cmp_val = strcmp (msgid, domain->data
			       + W (domain->must_swap,
				    domain->orig_tab[act].offset));
      if (cmp_val < 0)
	top = act;
      else if (cmp_val > 0)
	bottom = act + 1;
      else
	break;
    }

  /* Если перевод найден, верните его.  */
  return bottom >= top ? NULL : (char *) domain->data
                                + W (domain->must_swap,
				     domain->trans_tab[act].offset);
}


/* Вернуть строковое представление локали CATEGORY.  */
static const char *
internal_function
category_to_name (category)
     int category;
{
  const char *retval;

  switch (category)
  {
#ifdef LC_COLLATE
  case LC_COLLATE:
    retval = "LC_COLLATE";
    break;
#endif
#ifdef LC_CTYPE
  case LC_CTYPE:
    retval = "LC_CTYPE";
    break;
#endif
#ifdef LC_MONETARY
  case LC_MONETARY:
    retval = "LC_MONETARY";
    break;
#endif
#ifdef LC_NUMERIC
  case LC_NUMERIC:
    retval = "LC_NUMERIC";
    break;
#endif
#ifdef LC_TIME
  case LC_TIME:
    retval = "LC_TIME";
    break;
#endif
#ifdef LC_MESSAGES
  case LC_MESSAGES:
    retval = "LC_MESSAGES";
    break;
#endif
#ifdef LC_RESPONSE
  case LC_RESPONSE:
    retval = "LC_RESPONSE";
    break;
#endif
#ifdef LC_ALL
  case LC_ALL:
    /* Это может не иметь смысла, но, возможно, лучше любого другого
       значение.  */
    retval = "LC_ALL";
    break;
#endif
  default:
    /* Если у вас есть лучшее представление о значении по умолчанию, дайте мне знать.  */
    retval = "LC_XXX";
  }

  return retval;
}

/* Угадайте значение текущей локали по значению переменных окружения.  */
static const char *
internal_function
guess_category_value (category, categoryname)
     int category;
     const char *categoryname;
{
  const char *retval;

  /* Наивысшим приоритетом является среда LANGUAGE.
     переменная. Это расширение GNU.  */
  retval = getenv ("LANGUAGE");
  if (retval != NULL && retval[0] != '\0')
    return retval;

  /* `LANGUAGE 'не установлен. Итак, мы должны перейти к POSIX
     методы поиска LC_ALL, LC_xxx и LANG. На некоторых
     В системах это может быть сделано самой функцией `setlocale '.  */
#if defined HAVE_SETLOCALE && defined HAVE_LC_MESSAGES && defined HAVE_LOCALE_NULL
  return setlocale (category, NULL);
#else
  /* Установка LC_ALL перезаписывает все остальные.  */
  retval = getenv ("LC_ALL");
  if (retval != NULL && retval[0] != '\0')
    return retval;

  /* Далее идет название желаемой категории.  */
  retval = getenv (categoryname);
  if (retval != NULL && retval[0] != '\0')
    return retval;

  /* Последняя возможность - это переменная среды LANG.  */
  retval = getenv ("LANG");
  if (retval != NULL && retval[0] != '\0')
    return retval;

  /* Мы используем C как домен по умолчанию. POSIX говорит, что это реализация
     определено.  */
  return "C";
#endif
}

/* @@ начало эпилога @@ */

/* Мы не хотим, чтобы libintl.a зависела от какой-либо другой библиотеки. Итак, мы
   избегайте нестандартной функции stpcpy. В библиотеке GNU C это
   хотя функция доступна. Также разрешите символ HAVE_STPCPY
   быть определенным.  */
#if !_LIBC && !HAVE_STPCPY
static char *
stpcpy (dest, src)
     char *dest;
     const char *src;
{
  while ((*dest++ = *src++) != '\0')
    /* Ничего не делать. */ ;
  return dest - 1;
}
#endif


#ifdef _LIBC
/* Если мы хотим освободить все ресурсы, нам нужно поработать над
   конец программы.  */
static void __attribute__ ((unused))
free_mem (void)
{
  struct binding *runp;

  for (runp = _nl_domain_bindings; runp != NULL; runp = runp->next)
    {
      free (runp->domainname);
      if (runp->dirname != _nl_default_dirname)
	/* Да, это сравнение указателей.  */
	free (runp->dirname);
    }

  if (_nl_current_default_domain != _nl_default_default_domain)
    /* Да, снова сравнение указателей.  */
    free ((char *) _nl_current_default_domain);
}

text_set_element (__libc_subfreeres, free_mem);
#endif
