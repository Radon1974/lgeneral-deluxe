/*
    Data-structures for extraction.
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

#ifndef LTREXTRACT_UTIL_H
#define LTREXTRACT_UTIL_H

struct Translateables;
struct TranslateablesIterator;
struct PData;

/** Создает новый набор переводимых */
struct Translateables *translateables_create(void);

/** Удаляет набор переводимых */
void translateables_delete(struct Translateables *);

/**
 * Возвращает комментарий, если данный переводимый объект уже содержится.
 * @param xl набор переводимых файлов
 * @param ключ переводится, чтобы искать
 * @param len, если не ноль, будет записано в длину возвращаемого
 * строка (без завершающего нуля). Не будет инициализирован, если ключ не найден.
 *
 * Примечание: если комментарий не был добавлен, функция вернет
 * пустая строка, если она есть. Если и только если переводимый не
 * содержится, возвращается 0.
 */
const char *translateables_get(struct Translateables *xl, const char *key, unsigned *len);

/** 
 * Добавляет переводимый в набор переводимых.
 *
 * @param xl набор переводимых файлов
 * @param key переводимая строка, которая будет служить ключом для gettext
 * @param комментарий к любому комментарию, описывающему ключ \ c. Прилагается дословно
 * если ключ \ c уже существует. Может быть 0 без комментариев.
 */
void translateables_add(struct Translateables *xl, const char *key, const char *comment);

/**
 * Добавляет переводимый из заданного значения pdata-value.
 *
 * Это удобная функция. Если переводимого еще нет,
 * добавлено. Контекстная информация добавляется в переводимый
 * как C-комментарий.
 */
void translateables_add_pdata(struct Translateables *xl, struct PData *pd);

/**
 * Устанавливает домен для всех переводимых.
 *
 * Для извлечения учитываются только ресурсы, принадлежащие этому домену.
 */
void translateables_set_domain(struct Translateables *xl, const char *domain);

/**
 * Возвращает домен для всех переводимых.
 */
const char *translateables_get_domain(struct Translateables *xl);

/**
 * Возвращает итератор для заданного набора переводимых объектов.
 */
struct TranslateablesIterator *translateables_iterator(struct Translateables *xl);

/**
 * Возвращает текущий ключ.
 */
const char *translateables_iterator_key(struct TranslateablesIterator *it);

/**
 * Возвращает текущий комментарий.
 */
const char *translateables_iterator_comment(struct TranslateablesIterator *it);

/**
 * Переход к следующей записи. Возвращает 0, если достигнут конец набора.
 */
int translateables_iterator_advance(struct TranslateablesIterator *it);

/**
 * Освобождает итератор.
 */
void translateables_iterator_delete(struct TranslateablesIterator *it);

/**
 * Возвращает, соответствует ли ресурс домену переводимых файлов.
 */
int resource_matches_domain(struct PData *pd, struct Translateables *xl);

#endif /*LTREXTRACT_UTIL_H*/

/* kate: tab-indents on; space-indent on; replace-tabs off; indent-width 2; dynamic-word-wrap off; inden(t)-mode cstyle */
