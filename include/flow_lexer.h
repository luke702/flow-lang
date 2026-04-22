#ifndef FLOW_LEXER_H
#define FLOW_LEXER_H

#include "flow_token.h"
#include <stddef.h>

typedef struct {
  const char *src;
  size_t len;
  size_t pos;
  long line;
  long col;
} FlowLexer;

void flow_lexer_init(FlowLexer *L, const char *src, size_t len);
Token flow_lexer_next(FlowLexer *L);

#endif
