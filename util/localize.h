/*
    Translation utilities.
    Copyright (C) 2005 Leo Savernik  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
    OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef UTIL_LOCALIZE_H
#define UTIL_LOCALIZE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libintl.h>

#ifdef ENABLE_NLS
/** ���������� ��� ��������� �������� ������ �� ��������� */
#  define tr(s) gettext (s)
/** ���������� ��� ��������� �������� ���������� ������ */
static const char *trd(const char *dom, const char *s) { return *(s) ? dgettext ((dom), (s)) : ""; }
/** ���������� ��� ����������� �������� ������ �� ��������� */
#  define TR_NOOP(s) (s)

#else
#  define tr(s) (s)
#  define trd(dom, s) (s)
#  define TR_NOOP(s) (s)
#endif

/**
 * ��������� ��� �������� ������� ������, ����� ��� �����
 * �������������� �� ���� ����������.
 * @param �������� ��� ������. ��� ����� ��� ������ ����
 * ������ ��� ��������� ������ \ c ��� ������� dcgettext.
 * @param �������� ���������������, ������ ���� 0.
 */
int locale_load_domain(const char *domain, const char *translations);

/**
 * ���������� � buf ���������� ������������� ����� \ c
 * ��������� ������� ������������� ����.
 * @param ����� buf ��� ������
 * @param n ������������ ���������� �������� ��� ������ (�������
 * �������� 0)
 * @param ����� �����, ������� ����� ��������� ��� ����������
 */
void locale_write_ordinal_number(char *buf, unsigned n, int number);

/**
 * ��������������� i18n ��� ���� ������������ ������ lgeneral.
 * @param lang iso-code ����� ��� 0 ��� ��������������� �����������
 */
void locale_init(const char *lang);

#endif /*UTIL_LOCALIZE_H*/

/* kate: tab-indents on; space-indent on; replace-tabs off; indent-width 2; dynamic-word-wrap off; inden(t)-mode cstyle */
