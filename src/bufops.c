#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bufops.h"
#include "l2o.h"

static size_t bufsiz = 1024;

/* Read from input file into buffer, until buffer is full or all of the file
   has been read. */
void
fillbuf (buf_t *buf, FILE *ifl, bool keep)
{
  /* If KEEP is true, shift unused bytes to beginning of buffer.
     Might be overlap in regions, can't use memcpy or strcpy. */
  if (keep && buf->buf != buf->ptr)
    {
      char *bufptr = buf->buf;
      while (buf->ptr < buf->eob)
        *bufptr++ = *buf->ptr++;
      buf->ptr = bufptr;
    }

  /* Ensure buffer is filled. */
  buf->ptr += fread (buf->ptr, sizeof (char), buf->eob - buf->ptr, ifl);
  while (buf->ptr != buf->eob)
    {
      if (ferror (ifl))
        error ("Error reading input file.");
      if (feof (ifl))
        {
          /* Ensure last line in file ends on newline. */
          if (buf->ptr != buf->buf && *(buf->ptr - 1) != '\n')
            *buf->ptr++ = '\n';
          if (buf->ptr < buf->eob)
            *buf->ptr++ = '\0';
          buf->eob = buf->ptr;
          buf->eof = true;
          break;
        }
      buf->ptr += fread (buf->ptr, sizeof (char), buf->eob - buf->ptr, ifl);
    }

  buf->ptr = buf->buf;
}


void
init_buf (buf_t *buf)
{
  buf->buf = calloc (bufsiz, sizeof (char));
  buf->ptr = buf->buf;
  buf->eob = buf->buf + bufsiz;
  buf->eof = false;
}


/* Checks if WORD points at a numerical value. */
bool isnum (char *word)
{
  int point = 0;
  while (*word && !isspace (*word))
    {
      if (*word == '.')
        point++;
      else if (ispunct (*word))
        break;
      if (!isdigit (*word) && *word != '.')
        return false;
      word++;
    }
  return (point <= 1);
}

/* Copy bytes into buf until DELIM is encountered. If WHITESPACE is true, any
   whitespace character will also be a deliminator. */
inline void
putword (char *word, buf_t *buf, size_t len)
{
  while (*word && len--)
    *buf->ptr++ = *word++;
}


/* Increments buffer pointer to next non-whitespace character. Return NULL
   if no non-whitespace character is found. */
//char *
void
skip_space (buf_t *buf)
{
  do
    if (*buf->ptr && !isspace (*buf->ptr))
      return;// buf->ptr;
  while (buf->ptr++ < buf->eob);
  return;// NULL;
}


/* Increment buffer pointer to byte past the next non-whitespace segmeent. */
void
skip_word (buf_t *buf)
{
  /* Skip any space before the word. */
  skip_space (buf);

  /* Skip non-whitespace characters. */
  do
    if (*buf->ptr && isspace (*buf->ptr))
      return;
  while (buf->ptr++ < buf->eob);
}

/* Calculates the length of a word, where a word is defined as consisting of
   alphanumeric characters.

    EXCEPTIONS: '%' and '_' will also be included in the word. */
int
wordlen (char *word)
{
  int ret = 0;
  while (*word && (isalnum (*word) || *word == '_'
                   || *word == '%' || *word == '.' || *word == '$'))
    {
      word++;
      ret++;
    }
  return ret;
}


/* Write a word. */
void
writeword (char *word)
{
  while (*word && !isspace (*word))
    fputc (*word++, stderr);
}

/* Finds occurence of C in S before the next newline character. */
char *
xstrchr (char *s, int c)
{
  if (c != '\n')
    {
      char *iter = s;
      while (*s != c && *s != '\n')
        s++;
      return (*s == '\n')? NULL : s;
    }
  else
    return strchr (s, c);

}

/* Checks two input strings for equality, ignoring case differences, and
   including the string delimiter, which can be a null byte or white space. */
/* TODO: handle case where 1 string is a substring of the other. */

bool
xstrcmp (char *s1, char *s2, size_t len)
{
  /*  Repeat until we run into null byte or white space in either string. */
  while (len--)
    {
      if (tolower (*s1) != tolower (*s2))
        return false;
      s1++;
      s2++;
    }
  return true;
}

