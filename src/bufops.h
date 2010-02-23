#ifndef BUFOPS_H
#define BUFOPS_H

#include <stdbool.h>

#include "l2o.h"

void fillbuf (buf_t *buf, FILE *ifl, bool keep);
void init_buf (buf_t *buf);
bool isnum (char *word);
void putword (char *word, buf_t *buf, size_t len);
void skip_space (buf_t *buf);
void skip_word (buf_t *buf);
int wordlen (char *word);
void writeword (char *word);
char *xstrchr (char *s, int c);
bool xstrcmp (char *s1, char *s2, size_t len);

#endif
