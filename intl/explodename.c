/* Авторские права (C) 1995, 1996, 1997, 1998 Free Software Foundation, Inc.
   Предоставлено Ульрихом Дреппером <drepper@gnu.ai.mit.edu>, 1995 г.

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

#if defined HAVE_STRING_H || defined _LIBC
# include <string.h>
#else
# include <strings.h>
#endif
#include <sys/types.h>

#include "loadinfo.h"

/* В некоторых странных системах определение NULL до сих пор не найдено. Вздох!  */
#ifndef NULL
# if defined __STDC__ && __STDC__
#  define NULL ((void *) 0)
# else
#  define NULL 0
# endif
#endif

/* @@ конец пролога @@ */

int
_nl_explode_name (name, language, modifier, territory, codeset,
		  normalized_codeset, special, sponsor, revision)
     char *name;
     const char **language;
     const char **modifier;
     const char **territory;
     const char **codeset;
     const char **normalized_codeset;
     const char **special;
     const char **sponsor;
     const char **revision;
{
  enum { undecided, xpg, cen } syntax;
  char *cp;
  int mask;

  *modifier = NULL;
  *territory = NULL;
  *codeset = NULL;
  *normalized_codeset = NULL;
  *special = NULL;
  *sponsor = NULL;
  *revision = NULL;

  /* Теперь мы определяем отдельные части имени локали. Первый
     ищи язык. Символами завершения являются `_ 'и` @', если
     мы используем стиль XPG4 и `_ ',` +' и `', если мы используем синтаксис CEN.  */
  mask = 0;
  syntax = undecided;
  *language = cp = name;
  while (cp[0] != '\0' && cp[0] != '_' && cp[0] != '@'
	 && cp[0] != '+' && cp[0] != ',')
    ++cp;

  if (*language == cp)
    /* В этом нет смысла: нужно указать язык. Использовать
       эта запись без взрыва. Возможно, это псевдоним.  */
    cp = strchr (*language, '\0');
  else if (cp[0] == '_')
    {
      /* Далее идет территория.  */
      cp[0] = '\0';
      *territory = ++cp;

      while (cp[0] != '\0' && cp[0] != '.' && cp[0] != '@'
	     && cp[0] != '+' && cp[0] != ',' && cp[0] != '_')
	++cp;

      mask |= TERRITORY;

      if (cp[0] == '.')
	{
	  /* Далее идет набор кодов.  */
	  syntax = xpg;
	  cp[0] = '\0';
	  *codeset = ++cp;

	  while (cp[0] != '\0' && cp[0] != '@')
	    ++cp;

	  mask |= XPG_CODESET;

	  if (*codeset != cp && (*codeset)[0] != '\0')
	    {
	      *normalized_codeset = _nl_normalize_codeset (*codeset,
							   cp - *codeset);
	      if (strcmp (*codeset, *normalized_codeset) == 0)
		free ((char *) *normalized_codeset);
	      else
		mask |= XPG_NORM_CODESET;
	    }
	}
    }

  if (cp[0] == '@' || (syntax != xpg && cp[0] == '+'))
    {
      /* Далее идет модификатор.  */
      syntax = cp[0] == '@' ? xpg : cen;
      cp[0] = '\0';
      *modifier = ++cp;

      while (syntax == cen && cp[0] != '\0' && cp[0] != '+'
	     && cp[0] != ',' && cp[0] != '_')
	++cp;

      mask |= XPG_MODIFIER | CEN_AUDIENCE;
    }

  if (syntax != xpg && (cp[0] == '+' || cp[0] == ',' || cp[0] == '_'))
    {
      syntax = cen;

      if (cp[0] == '+')
	{
 	  /* Далее идет специальное приложение (синтаксис CEN).  */
	  cp[0] = '\0';
	  *special = ++cp;

	  while (cp[0] != '\0' && cp[0] != ',' && cp[0] != '_')
	    ++cp;

	  mask |= CEN_SPECIAL;
	}

      if (cp[0] == ',')
	{
 	  /* Далее идет спонсор (синтаксис CEN)..  */
	  cp[0] = '\0';
	  *sponsor = ++cp;

	  while (cp[0] != '\0' && cp[0] != '_')
	    ++cp;

	  mask |= CEN_SPONSOR;
	}

      if (cp[0] == '_')
	{
 	  /* Далее идет ревизия (синтаксис CEN).  */
	  cp[0] = '\0';
	  *revision = ++cp;

	  mask |= CEN_REVISION;
	}
    }

  /* Для значений синтаксиса CEN может быть важно иметь
     символ разделителя в имени файла, но не для синтаксиса XPG.  */
  if (syntax == xpg)
    {
      if (*territory != NULL && (*territory)[0] == '\0')
	mask &= ~TERRITORY;

      if (*codeset != NULL && (*codeset)[0] == '\0')
	mask &= ~XPG_CODESET;

      if (*modifier != NULL && (*modifier)[0] == '\0')
	mask &= ~XPG_MODIFIER;
    }

  return mask;
}
