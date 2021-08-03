/* Загрузите необходимые каталоги сообщений.
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

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined STDC_HEADERS || defined _LIBC
# include <stdlib.h>
#endif

#if defined HAVE_UNISTD_H || defined _LIBC
# include <unistd.h>
#endif

#if (defined HAVE_MMAP && defined HAVE_MUNMAP) || defined _LIBC
# include <sys/mman.h>
#endif

#include "gettext.h"
#include "gettextP.h"

/* @@ конец пролога @@ */

#ifdef _LIBC
/* Переименуйте функции, отличные от ISO C. Этого требует стандарт
   потому что некоторые функции ISO C потребуют связи с этим объектом
   файл и пространство имен не должны быть загрязнены.  */
# define open   __open
# define close  __close
# define read   __read
# define mmap   __mmap
# define munmap __munmap
#endif

/* Нужен знак, загружен ли новый каталог, который можно связать
   со всеми переводами. Это важно, если переводы
   кэшируется одной из функций GCC.  */
int _nl_msg_cat_cntr = 0;


/* Загрузите каталоги сообщений, указанные в FILENAME. Если это недействительно
   каталог сообщений ничего не делает.  */
void
internal_function
_nl_load_domain (domain_file)
     struct loaded_l10nfile *domain_file;
{
  int fd;
  size_t size;
  struct stat st;
  struct mo_file_header *data = (struct mo_file_header *) -1;
#if (defined HAVE_MMAP && defined HAVE_MUNMAP && !defined DISALLOW_MMAP) \
    || defined _LIBC
  int use_mmap = 0;
#endif
  struct loaded_domain *domain;

  domain_file->decided = 1;
  domain_file->data = NULL;

  /* Если запись не соответствует допустимому языку, FILENAME
     может быть NULL. Это может произойти, если в соответствии с данным
     спецификация имя файла локали отличается для XPG и CEN
     синтаксис.  */
  if (domain_file->filename == NULL)
    return;

  /* Попробуйте открыть указанный файл.  */
  fd = open (domain_file->filename, O_RDONLY);
  if (fd == -1)
    return;

  /* Мы должны знать размер файла.  */
  if (fstat (fd, &st) != 0
      || (size = (size_t) st.st_size) != st.st_size
      || size < sizeof (struct mo_file_header))
    {
      /* Что-то пошло не так.  */
      close (fd);
      return;
    }

#if (defined HAVE_MMAP && defined HAVE_MUNMAP && !defined DISALLOW_MMAP) \
    || defined _LIBC
  /* Теперь мы готовы загрузить файл. Если доступен mmap (), мы пробуем
     это первое. Если он недоступен или не удалось, мы пытаемся загрузить его.  */
  data = (struct mo_file_header *) mmap (NULL, size, PROT_READ,
					 MAP_PRIVATE, fd, 0);

  if (data != (struct mo_file_header *) -1)
    {
      /* Вызов mmap () прошел успешно.  */
      close (fd);
      use_mmap = 1;
    }
#endif

  /* Если данные еще не доступны (т.е. mmap'ed), мы пытаемся загрузить
     это вручную.  */
  if (data == (struct mo_file_header *) -1)
    {
      size_t to_read;
      char *read_ptr;

      data = (struct mo_file_header *) malloc (size);
      if (data == NULL)
	return;

      to_read = size;
      read_ptr = (char *) data;
      do
	{
	  long int nb = (long int) read (fd, read_ptr, to_read);
	  if (nb == -1)
	    {
	      close (fd);
	      return;
	    }

	  read_ptr += nb;
	  to_read -= nb;
	}
      while (to_read > 0);

      close (fd);
    }

  /* Используя магическое число, мы можем проверить, действительно ли это сообщение
     файл каталога.  */
  if (data->magic != _MAGIC && data->magic != _MAGIC_SWAPPED)
    {
      /* Неправильный магический номер: это не файл каталога сообщений.  */
#if (defined HAVE_MMAP && defined HAVE_MUNMAP && !defined DISALLOW_MMAP) \
    || defined _LIBC
      if (use_mmap)
	munmap ((caddr_t) data, size);
      else
#endif
	free (data);
      return;
    }

  domain_file->data
    = (struct loaded_domain *) malloc (sizeof (struct loaded_domain));
  if (domain_file->data == NULL)
    return;

  domain = (struct loaded_domain *) domain_file->data;
  domain->data = (char *) data;
#if (defined HAVE_MMAP && defined HAVE_MUNMAP && !defined DISALLOW_MMAP) \
    || defined _LIBC
  domain->use_mmap = use_mmap;
#endif
  domain->mmap_size = size;
  domain->must_swap = data->magic != _MAGIC;

  /* Заполните информацию о доступных таблицах.  */
  switch (W (domain->must_swap, data->revision))
    {
    case 0:
      domain->nstrings = W (domain->must_swap, data->nstrings);
      domain->orig_tab = (struct string_desc *)
	((char *) data + W (domain->must_swap, data->orig_tab_offset));
      domain->trans_tab = (struct string_desc *)
	((char *) data + W (domain->must_swap, data->trans_tab_offset));
      domain->hash_size = W (domain->must_swap, data->hash_tab_size);
      domain->hash_tab = (nls_uint32 *)
	((char *) data + W (domain->must_swap, data->hash_tab_offset));
      break;
    default:
      /* Это незаконная ревизия.  */
#if (defined HAVE_MMAP && defined HAVE_MUNMAP && !defined DISALLOW_MMAP) \
    || defined _LIBC
      if (use_mmap)
	munmap ((caddr_t) data, size);
      else
#endif
	free (data);
      free (domain);
      domain_file->data = NULL;
      return;
    }

  /* Показать, что изменен один домен. Это может сделать некоторые кешированные
     переводы недействительны.  */
  ++_nl_msg_cat_cntr;
}


#ifdef _LIBC
void
internal_function
_nl_unload_domain (domain)
     struct loaded_domain *domain;
{
  if (domain->use_mmap)
    munmap ((caddr_t) domain->data, domain->mmap_size);
  else
    free ((void *) domain->data);

  free (domain);
}
#endif
