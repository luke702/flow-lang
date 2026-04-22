#ifndef FLOW_TOKEN_H
#define FLOW_TOKEN_H

#include <stddef.h>

typedef enum {
  TOK_EOF,
  TOK_ERROR,

  TOK_IDENT,
  TOK_INT,
  TOK_FLOAT,
  TOK_STRING,

  TOK_LET,
  TOK_FUNC,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_FOR,
  TOK_IN,
  TOK_MATCH,
  TOK_RETURN,
  TOK_BREAK,
  TOK_CONTINUE,
  TOK_GO,
  TOK_MAKE,
  TOK_CLOSE,
  TOK_IMPORT,
  TOK_EXPORT,
  TOK_LIBRARY,
  TOK_STRUCT,

  TOK_TRUE,
  TOK_FALSE,
  TOK_NIL,

  TOK_INT_TYPE,
  TOK_FLOAT_TYPE,
  TOK_BOOL_TYPE,
  TOK_STRING_TYPE,
  TOK_CHAN,
  TOK_LIST,
  TOK_DICT,

  TOK_LPAREN,
  TOK_RPAREN,
  TOK_LBRACE,
  TOK_RBRACE,
  TOK_LBRACKET,
  TOK_RBRACKET,
  TOK_COMMA,
  TOK_DOT,
  TOK_SEMI,
  TOK_COLON,

  TOK_PLUS,
  TOK_MINUS,
  TOK_STAR,
  TOK_SLASH,
  TOK_PERCENT,

  TOK_EQ,
  TOK_EQEQ,
  TOK_BANG,
  TOK_BANGEQ,
  TOK_LT,
  TOK_LTE,
  TOK_GT,
  TOK_GTE,

  TOK_ANDAND,
  TOK_OROR,

  TOK_FATARROW, /* => */
  TOK_QMARK,    /* ? */

  TOK_BACKTICK_CMD,
} TokKind;

typedef struct {
  TokKind kind;
  const char *start;
  size_t len;
  long line;
  long col;
} Token;

#endif
