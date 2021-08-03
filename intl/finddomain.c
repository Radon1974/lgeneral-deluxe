/* Обрабатывать список необходимых каталогов сообщений
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
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

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

/* @@ конец пролога @@ */
/* Список уже загруженных доменов.  */
static struct loaded_l10nfile *_nl_loaded_domains;


/* Вернуть структуру данных, описывающую каталог сообщений, описанный
   параметры DOMAINNAME и CATEGORY относительно текущего
   Установлены привязки.  */
struct loaded_l10nfile *
internal_function
_nl_find_domain (dirname, locale, domainname)
     const char *dirname;
     char *locale;
     const char *domainname;
{
  struct loaded_l10nfile *retval;
  const char *language;
  const char *modifier;
  const char *territory;
  const char *codeset;
  const char *normalized_codeset;
  const char *special;
  const char *sponsor;
  const char *revision;
  const char *alias_value;
  int mask;

  /* LOCALE может состоять из четырех распознаваемых частей синтаксиса XPG:

язык [_territory [.codeset]] [@ модификатор]

     и шесть частей синтаксиса CEN:

язык [_territory] [+ аудитория] [+ особый] [, [спонсор] [_ версия]]

     Кроме первой части все они могут отсутствовать. Если
     полный указанный языковой стандарт не найден, менее конкретный
     искал. Различные части будут удалены в соответствии с
     в следующем порядке:
(1) редакция
(2) спонсор
(3) специальные
(4) набор кодов
(5) нормализованный кодовый набор
(6) территория
(7) аудитория / модификатор
   */

  /* Если мы уже тестировали эту запись локали, необходимо
     быть одним набором данных в списке загруженных доменов.  */
  retval = _nl_make_l10nflist (&_nl_loaded_domains, dirname,
			       strlen (dirname) + 1, 0, locale, NULL, NULL,
			       NULL, NULL, NULL, NULL, NULL, domainname, 0);
  if (retval != NULL)
    {
      /* Мы кое-что знаем об этой местности.  */
      int cnt;

      if (retval->decided == 0)
	_nl_load_domain (retval);

      if (retval->data != NULL)
	return retval;

      for (cnt = 0; retval->successor[cnt] != NULL; ++cnt)
	{
	  if (retval->successor[cnt]->decided == 0)
	    _nl_load_domain (retval->successor[cnt]);

	  if (retval->successor[cnt]->data != NULL)
	    break;
	}
      return cnt >= 0 ? retval : NULL;
      /* НЕ ДОСТУПНЫ */
    }

  /* Посмотрите, является ли значение локали псевдонимом. Если да, его значение
     * перезаписывает * псевдоним. Тест на исходное значение не проводится.
     сделанный.  */
  alias_value = _nl_expand_alias (locale);
  if (alias_value != NULL)
    {
#if defined _LIBC || defined HAVE_STRDUP
      locale = strdup (alias_value);
      if (locale == NULL)
	return NULL;
#else
      size_t len = strlen (alias_value) + 1;
      locale = (char *) malloc (len);
      if (locale == NULL)
	return NULL;

      memcpy (locale, alias_value, len);
#endif
    }

  /* Теперь мы определяем отдельные части имени локали. Первый
     ищи язык. Символами завершения являются `_ 'и` @', если
     мы используем стиль XPG4 и `_ ',` +' и `', если мы используем синтаксис CEN.  */
  mask = _nl_explode_name (locale, &language, &modifier, &territory,
			   &codeset, &normalized_codeset, &special,
			   &sponsor, &revision);

  /* Создайте все возможные записи локали, которые могут быть интересны
     обобщение.  */
  retval = _nl_make_l10nflist (&_nl_loaded_domains, dirname,
			       strlen (dirname) + 1, mask, language, territory,
			       codeset, normalized_codeset, modifier, special,
			       sponsor, revision, domainname, 1);
  if (retval == NULL)
    /* Это означает, что мы вне ядра.  */
    return NULL;

  if (retval->decided == 0)
    _nl_load_domain (retval);
  if (retval->data == NULL)
    {
      int cnt;
      for (cnt = 0; retval->successor[cnt] != NULL; ++cnt)
	{
	  if (retval->successor[cnt]->decided == 0)
	    _nl_load_domain (retval->successor[cnt]);
	  if (retval->successor[cnt]->data != NULL)
	    break;
	}
    }

  /* Место для псевдонима выделялось динамически. Освободи это сейчас.  */
  if (alias_value != NULL)
    free (locale);

  return retval;
}


#ifdef _LIBC
static void __attribute__ ((unused))
free_mem (void)
{
  struct loaded_l10nfile *runp = _nl_loaded_domains;

  while (runp != NULL)
    {
      struct loaded_l10nfile *here = runp;
      if (runp->data != NULL)
	_nl_unload_domain ((struct loaded_domain *) runp->data);
      runp = runp->next;
      free (here);
    }
}

text_set_element (__libc_subfreeres, free_mem);
#endif
