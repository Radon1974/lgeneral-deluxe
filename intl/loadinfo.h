/* Авторское право (C) 1996, 1997 Free Software Foundation, Inc.
   Этот файл является частью библиотеки GNU C.
   Предоставлено Ульрихом Дреппером <drepper@cygnus.com>, 1996 г.

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

#ifndef PARAMS
# if __STDC__
#  define PARAMS(args) args
# else
#  define PARAMS(args) ()
# endif
#endif

/* Кодирование частей названия локали.  */
#define CEN_REVISION		1
#define CEN_SPONSOR		2
#define CEN_SPECIAL		4
#define XPG_NORM_CODESET	8
#define XPG_CODESET		16
#define TERRITORY		32
#define CEN_AUDIENCE		64
#define XPG_MODIFIER		128

#define CEN_SPECIFIC	(CEN_REVISION|CEN_SPONSOR|CEN_SPECIAL|CEN_AUDIENCE)
#define XPG_SPECIFIC	(XPG_CODESET|XPG_NORM_CODESET|XPG_MODIFIER)


struct loaded_l10nfile
{
  const char *filename;
  int decided;

  const void *data;

  struct loaded_l10nfile *next;
  struct loaded_l10nfile *successor[1];
};


extern const char *_nl_normalize_codeset PARAMS ((const unsigned char *codeset,
						  size_t name_len));

extern struct loaded_l10nfile *
_nl_make_l10nflist PARAMS ((struct loaded_l10nfile **l10nfile_list,
			    const char *dirlist, size_t dirlist_len, int mask,
			    const char *language, const char *territory,
			    const char *codeset,
			    const char *normalized_codeset,
			    const char *modifier, const char *special,
			    const char *sponsor, const char *revision,
			    const char *filename, int do_allocate));


extern const char *_nl_expand_alias PARAMS ((const char *name));

extern int _nl_explode_name PARAMS ((char *name, const char **language,
				     const char **modifier,
				     const char **territory,
				     const char **codeset,
				     const char **normalized_codeset,
				     const char **special,
				     const char **sponsor,
				     const char **revision));
