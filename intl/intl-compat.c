/* intl-compat.c - Функции-заглушки для вызова функций gettext из GNU gettext
   Библиотека.
   Авторское право (C) 1995 Software Foundation, Inc.

Это бесплатное программное обеспечение; вы можете распространять и / или изменять
это в соответствии с условиями Стандартной общественной лицензии GNU, опубликованной
Фонд свободного программного обеспечения; либо версия 2, либо (на ваш выбор)
любая более поздняя версия.

Эта программа распространяется в надежде, что она будет полезной,
но БЕЗ КАКИХ-ЛИБО ГАРАНТИЙ; даже без подразумеваемой гарантии
КОММЕРЧЕСКАЯ ЦЕННОСТЬ или ПРИГОДНОСТЬ ДЛЯ ОПРЕДЕЛЕННОЙ ЦЕЛИ. Увидеть
Стандартная общественная лицензия GNU для более подробной информации.

Вы должны были получить копию Стандартной общественной лицензии GNU
вместе с этой программой; если нет, напишите в Бесплатное ПО
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, США.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libgettext.h"

/* @@ конец пролога @@ */


#undef gettext
#undef dgettext
#undef dcgettext
#undef textdomain
#undef bindtextdomain


char *
bindtextdomain (domainname, dirname)
     const char *domainname;
     const char *dirname;
{
  return bindtextdomain__ (domainname, dirname);
}


char *
dcgettext (domainname, msgid, category)
     const char *domainname;
     const char *msgid;
     int category;
{
  return dcgettext__ (domainname, msgid, category);
}


char *
dgettext (domainname, msgid)
     const char *domainname;
     const char *msgid;
{
  return dgettext__ (domainname, msgid);
}


char *
gettext (msgid)
     const char *msgid;
{
  return gettext__ (msgid);
}


char *
textdomain (domainname)
     const char *domainname;
{
  return textdomain__ (domainname);
}
