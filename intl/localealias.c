/* Обрабатывать псевдонимы для имен локалей.
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

#include <ctype.h>
#include <stdio.h>
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
# ifndef memcpy
#  define memcpy(Dst, Src, Num) bcopy (Src, Dst, Num)
# endif
#endif
#if !HAVE_STRCHR && !defined _LIBC
# ifndef strchr
#  define strchr index
# endif
#endif

#include "gettext.h"
#include "gettextP.h"

/* @@ конец пролога @@ */

#ifdef _LIBC
/* Переименуйте функции, отличные от ANSI C. Этого требует стандарт
   потому что некоторые функции ANSI C потребуют связи с этим объектом
   файл и пространство имен не должны быть загрязнены.  */
# define strcasecmp __strcasecmp

# define mempcpy __mempcpy
# define HAVE_MEMPCPY	1

/* Здесь нам нужна блокировка, так как нас можно вызывать из разных мест.  */
# include <bits/libc-lock.h>

__libc_lock_define_initialized (static, lock);
#endif


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


struct alias_map
{
  const char *alias;
  const char *value;
};


static char *string_space = NULL;
static size_t string_space_act = 0;
static size_t string_space_max = 0;
static struct alias_map *map;
static size_t nmap = 0;
static size_t maxmap = 0;


/* Прототипы для локальных функций.  */
static size_t read_alias_file PARAMS ((const char *fname, int fname_len))
     internal_function;
static void extend_alias_table PARAMS ((void));
static int alias_compare PARAMS ((const struct alias_map *map1,
				  const struct alias_map *map2));


const char *
_nl_expand_alias (name)
    const char *name;
{
  static const char *locale_alias_path = LOCALE_ALIAS_PATH;
  struct alias_map *retval;
  const char *result = NULL;
  size_t added;

#ifdef _LIBC
  __libc_lock_lock (lock);
#endif

  do
    {
      struct alias_map item;

      item.alias = name;

      if (nmap > 0)
	retval = (struct alias_map *) bsearch (&item, map, nmap,
					       sizeof (struct alias_map),
					       (int (*) PARAMS ((const void *,
								 const void *))
						) alias_compare);
      else
	retval = NULL;

      /* Мы действительно нашли псевдоним. Вернуть значение.  */
      if (retval != NULL)
	{
	  result = retval->value;
	  break;
	}

      /* Возможно, мы сможем найти другой файл псевдонимов.  */
      added = 0;
      while (added == 0 && locale_alias_path[0] != '\0')
	{
	  const char *start;

	  while (locale_alias_path[0] == ':')
	    ++locale_alias_path;
	  start = locale_alias_path;

	  while (locale_alias_path[0] != '\0' && locale_alias_path[0] != ':')
	    ++locale_alias_path;

	  if (start < locale_alias_path)
	    added = read_alias_file (start, locale_alias_path - start);
	}
    }
  while (added != 0);

#ifdef _LIBC
  __libc_lock_unlock (lock);
#endif

  return result;
}


static size_t
internal_function
read_alias_file (fname, fname_len)
     const char *fname;
     int fname_len;
{
#ifndef HAVE_ALLOCA
  struct block_list *block_list = NULL;
#endif
  FILE *fp;
  char *full_fname;
  size_t added;
  static const char aliasfile[] = "/locale.alias";

  full_fname = (char *) alloca (fname_len + sizeof aliasfile);
  ADD_BLOCK (block_list, full_fname);
#ifdef HAVE_MEMPCPY
  mempcpy (mempcpy (full_fname, fname, fname_len),
	   aliasfile, sizeof aliasfile);
#else
  memcpy (full_fname, fname, fname_len);
  memcpy (&full_fname[fname_len], aliasfile, sizeof aliasfile);
#endif

  fp = fopen (full_fname, "r");
  if (fp == NULL)
    {
      FREE_BLOCKS (block_list);
      return 0;
    }

  added = 0;
  while (!feof (fp))
    {
      /* Здесь разумно использовать буфер исправлений, потому что
а) нас интересуют только первые два поля
б) эти поля должны использоваться как имена файлов и поэтому не должны
будь таким длинным
       */
      unsigned char buf[BUFSIZ];
      unsigned char *alias;
      unsigned char *value;
      unsigned char *cp;

      if (fgets (buf, sizeof buf, fp) == NULL)
	/* EOF достигнут.  */
	break;

      /* Возможно, в буфер помещается не вся строка. Игнорировать
остальная часть строки.  */
      if (strchr (buf, '\n') == NULL)
	{
	  char altbuf[BUFSIZ];
	  do
	    if (fgets (altbuf, sizeof altbuf, fp) == NULL)
	      /* Убедитесь, что внутренняя петля останется. Внешний цикл
выйдет из теста feof.  */
	      break;
	  while (strchr (altbuf, '\n') == NULL);
	}

      cp = buf;
      /* Игнорировать начальные пробелы.  */
      while (isspace (cp[0]))
	++cp;

      /* Знак "#" в начале означает строку комментария.  */
      if (cp[0] != '\0' && cp[0] != '#')
	{
	  alias = cp++;
	  while (cp[0] != '\0' && !isspace (cp[0]))
	    ++cp;
	  /* Завершить псевдоним.  */
	  if (cp[0] != '\0')
	    *cp++ = '\0';

	  /* Теперь поищите начало значения.  */
	  while (isspace (cp[0]))
	    ++cp;

	  if (cp[0] != '\0')
	    {
	      size_t alias_len;
	      size_t value_len;

	      value = cp++;
	      while (cp[0] != '\0' && !isspace (cp[0]))
		++cp;
	      /* Завершить значение.  */
	      if (cp[0] == '\n')
		{
		  /* Это необходимо сделать, чтобы провести следующий тест
для конца строки возможно. Мы ищем
завершающий '\ n', который здесь не перезаписывается.  */
		  *cp++ = '\0';
		  *cp = '\n';
		}
	      else if (cp[0] != '\0')
		*cp++ = '\0';

	      if (nmap >= maxmap)
		extend_alias_table ();

	      alias_len = strlen (alias) + 1;
	      value_len = strlen (value) + 1;

	      if (string_space_act + alias_len + value_len > string_space_max)
		{
		  /* Увеличьте размер пула памяти.  */
		  size_t new_size = (string_space_max
				     + (alias_len + value_len > 1024
					? alias_len + value_len : 1024));
		  char *new_pool = (char *) realloc (string_space, new_size);
		  if (new_pool == NULL)
		    {
		      FREE_BLOCKS (block_list);
		      return added;
		    }
		  string_space = new_pool;
		  string_space_max = new_size;
		}

	      map[nmap].alias = memcpy (&string_space[string_space_act],
					alias, alias_len);
	      string_space_act += alias_len;

	      map[nmap].value = memcpy (&string_space[string_space_act],
					value, value_len);
	      string_space_act += value_len;

	      ++nmap;
	      ++added;
	    }
	}
    }

  /* Стоит ли тестировать на ferror ()? Я думаю, мы должны молча игнорировать
     ошибки.  --drepper  */
  fclose (fp);

  if (added > 0)
    qsort (map, nmap, sizeof (struct alias_map),
	   (int (*) PARAMS ((const void *, const void *))) alias_compare);

  FREE_BLOCKS (block_list);
  return added;
}


static void
extend_alias_table ()
{
  size_t new_size;
  struct alias_map *new_map;

  new_size = maxmap == 0 ? 100 : 2 * maxmap;
  new_map = (struct alias_map *) realloc (map, (new_size
						* sizeof (struct alias_map)));
  if (new_map == NULL)
    /* Просто не расширяйтесь: у нас больше нет ядра.  */
    return;

  map = new_map;
  maxmap = new_size;
}


#ifdef _LIBC
static void __attribute__ ((unused))
free_mem (void)
{
  if (string_space != NULL)
    free (string_space);
  if (map != NULL)
    free (map);
}
text_set_element (__libc_subfreeres, free_mem);
#endif


static int
alias_compare (map1, map2)
     const struct alias_map *map1;
     const struct alias_map *map2;
{
#if defined _LIBC || defined HAVE_STRCASECMP
  return strcasecmp (map1->alias, map2->alias);
#else
  const unsigned char *p1 = (const unsigned char *) map1->alias;
  const unsigned char *p2 = (const unsigned char *) map2->alias;
  unsigned char c1, c2;

  if (p1 == p2)
    return 0;

  do
    {
      /* Я знаю, что это кажется странным, но функция tolower () в
некоторые системы libc не могут обрабатывать символы, отличные от алфавита.  */
      c1 = isupper (*p1) ? tolower (*p1) : *p1;
      c2 = isupper (*p2) ? tolower (*p2) : *p2;
      if (c1 == '\0')
	break;
      ++p1;
      ++p2;
    }
  while (c1 == c2);

  return c1 - c2;
#endif
}
