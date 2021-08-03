/* �������� ��������� ��� �������������������.
   ��������� ����� (C) 1995, 1996, 1997, 1998 Free Software Foundation, Inc.

   ��� ���������� ����������� �����������; �� ������ �������������� � / ��� ��������
   ��� � ������������ � ��������� ����������� ������������ �������� GNU, ��������������
   ���� ���������� ������������ �����������; ���� ������ 2, ���� (�� ��� �����)
   ����� ����� ������� ������.

   ��� ��������� ���������������� � �������, ��� ��� ����� ��������,
   �� ��� �����-���� ��������; ���� ��� ��������������� ��������
   ������������ �������� ��� ����������� ��� ������������ ����. �������
   ����������� ������������ �������� GNU ��� ����� ��������� ����������.

   �� ������ ���� �������� ����� ����������� ������������ �������� GNU
   ������ � ���� ����������; ���� ���, �������� � Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, ������, ����������� 02111-1307, ���.  */

/* ��������� � ��������� �������� (��������, Solaris) ��� ������ ���������� ��������
   ������� libintl.h, � ����� ���� ���� � ��� ���� ����� �������
   �������� ������ ����. �� ��������� ���������, ��������, �����
   define _LIBINTL_H � ������� �� ������ �������� ����������� �����.  */

#if !defined _LIBINTL_H || !defined _LIBGETTEXT_H
#ifndef _LIBINTL_H
# define _LIBINTL_H	1
#endif
#define _LIBGETTEXT_H	1

/* �� ���������� �������������� ������, ����� �������, ��� �� ���������� GNU
   ���������� gettext.  */
#define __USE_GNU_GETTEXT 1

#include <sys/types.h>

#if HAVE_LOCALE_H
# include <locale.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* @@ ����� ������� @@ */

#ifndef PARAMS
# if __STDC__ || defined __cplusplus
#  define PARAMS(args) args
# else
#  define PARAMS(args) ()
# endif
#endif

#ifndef NULL
# if !defined __cplusplus || defined __GNUC__
#  define NULL ((void *) 0)
# else
#  define NULL (0)
# endif
#endif

#if !HAVE_LC_MESSAGES
/* ��� �������� ���������� ��������� gettext () � dgettext ()
   �������. �� � ��������� �������� ��� �� ����������. ���������� ���
   � �������� �� ���������.  */
# define LC_MESSAGES (-1)
#endif


/* ���������� ��� ���������� gettext-using-catgets. ���������� ��
   Libintl.h ����� ���������.  */
struct _msg_ent
{
  const char *_msg;
  int _msg_number;
};


#if HAVE_CATGETS
/* These two variables are defined in the automatically by po-to-tbl.sed
   generated file `cat-id-tbl.c'.  */
extern const struct _msg_ent _msg_tbl[];
extern int _msg_tbl_length;
#endif


/* ��� ��������������� ���������� ��������� ������ �� �������
   ������� �����. ������ ����� ����������� �������� ���� ������.  */
#define gettext_noop(Str) (Str)

/* ������� MSGID � ������� �������� ��������� �� ��������� ��� ��������
   LC_MESSAGES �������� ��������. ���� �� ������, ���������� ��� MSGID (�� ���������
   �����).  */
extern char *gettext PARAMS ((const char *__msgid));
extern char *gettext__ PARAMS ((const char *__msgid));

/* ������� MSGID � �������� ��������� DOMAINNAME ��� ��������
   LC_MESSAGES �������� ��������.  */
extern char *dgettext PARAMS ((const char *__domainname, const char *__msgid));
extern char *dgettext__ PARAMS ((const char *__domainname,
				 const char *__msgid));

/* ������� MSGID � �������� ��������� DOMAINNAME ��� ������� CATEGORY
   ������.  */
extern char *dcgettext PARAMS ((const char *__domainname, const char *__msgid,
				int __category));
extern char *dcgettext__ PARAMS ((const char *__domainname,
				  const char *__msgid, int __category));


/* ���������� ��� �������� �������� ��������� �� ��������� �������� DOMAINNAME.
   ���� DOMAINNAME ����� �������� null, ������� ������� �������� �� ���������.
   ���� DOMAINNAME - "", �������� �� "���������" �� ���������.  */
extern char *textdomain PARAMS ((const char *__domainname));
extern char *textdomain__ PARAMS ((const char *__domainname));

/* �������, ��� ����� ������ ������� ��������� DOMAINNAME
   � DIRNAME, � �� � ���� ������ ������ �������.  */
extern char *bindtextdomain PARAMS ((const char *__domainname,
				  const char *__dirname));
extern char *bindtextdomain__ PARAMS ((const char *__domainname,
				    const char *__dirname));

#if ENABLE_NLS

/* Solaris 2.3 has the gettext function but dcgettext is missing.
   So we omit this optimization for Solaris 2.3.  BTW, Solaris 2.4
   has dcgettext.  */
# if !HAVE_CATGETS && (!HAVE_GETTEXT || HAVE_DCGETTEXT)

#  define gettext(Msgid)						      \
     dgettext (NULL, Msgid)

#  define dgettext(Domainname, Msgid)					      \
     dcgettext (Domainname, Msgid, LC_MESSAGES)

#  if defined __GNUC__ && ((__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7))
/* This global variable is defined in loadmsgcat.c.  We need a sign,
   whether a new catalog was loaded, which can be associated with all
   translations.  */
extern int _nl_msg_cat_cntr;

#   define dcgettext(Domainname, Msgid, Category)			      \
  (__extension__							      \
   ({									      \
     char *__result;							      \
     if (__builtin_constant_p (Msgid))					      \
       {								      \
	 static char *__translation__;					      \
	 static int __catalog_counter__;				      \
	 if (! __translation__ || __catalog_counter__ != _nl_msg_cat_cntr)    \
	   {								      \
	     __translation__ =						      \
	       dcgettext__ (Domainname, Msgid, Category);		      \
	     __catalog_counter__ = _nl_msg_cat_cntr;			      \
	   }								      \
	 __result = __translation__;					      \
       }								      \
     else								      \
       __result = dcgettext__ (Domainname, Msgid, Category);		      \
     __result;								      \
    }))
#  endif
# endif

#else

# define gettext(Msgid) (Msgid)
# define dgettext(Domainname, Msgid) (Msgid)
# define dcgettext(Domainname, Msgid, Category) (Msgid)
# define textdomain(Domainname) ((char *) Domainname)
# define bindtextdomain(Domainname, Dirname) ((char *) Dirname)

#endif

/* @@ ������ ������� @@ */

#ifdef __cplusplus
}
#endif

#endif
