#include "flow_lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void flow_lexer_init(FlowLexer *L, const char *src, size_t len) {
  L->src = src;
  L->len = len;
  L->pos = 0;
  L->line = 1;
  L->col = 1;
}

static char peek(const FlowLexer *L, size_t off) {
  size_t i = L->pos + off;
  return i < L->len ? L->src[i] : '\0';
}

static void advance(FlowLexer *L) {
  if (L->pos < L->len && L->src[L->pos] == '\n') {
    L->line++;
    L->col = 1;
  } else {
    L->col++;
  }
  if (L->pos < L->len)
    L->pos++;
}

static void skip_ws(FlowLexer *L) {
  for (;;) {
    if (L->pos >= L->len)
      return;
    char c = L->src[L->pos];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance(L);
      continue;
    }
    if (c == '/' && peek(L, 1) == '/') {
      while (L->pos < L->len && L->src[L->pos] != '\n')
        advance(L);
      continue;
    }
    if (c == '/' && peek(L, 1) == '*') {
      advance(L);
      advance(L);
      while (L->pos + 1 < L->len) {
        if (L->src[L->pos] == '*' && L->src[L->pos + 1] == '/') {
          advance(L);
          advance(L);
          break;
        }
        advance(L);
      }
      continue;
    }
    return;
  }
}

static Token make_tok(FlowLexer *L, TokKind k, const char *start, size_t len) {
  Token t;
  t.kind = k;
  t.start = start;
  t.len = len;
  t.line = L->line;
  t.col = L->col - (long)len;
  if (t.col < 1)
    t.col = 1;
  return t;
}

static TokKind ident_kind(const char *s, size_t len) {
  static const struct {
    const char *kw;
    TokKind kind;
  } tab[] = {
      {"let", TOK_LET},       {"func", TOK_FUNC},     {"if", TOK_IF},
      {"else", TOK_ELSE},     {"while", TOK_WHILE},   {"for", TOK_FOR},
      {"in", TOK_IN},         {"match", TOK_MATCH},   {"return", TOK_RETURN},
      {"break", TOK_BREAK},   {"continue", TOK_CONTINUE}, {"go", TOK_GO},
      {"make", TOK_MAKE},     {"close", TOK_CLOSE},   {"import", TOK_IMPORT},
      {"export", TOK_EXPORT}, {"library", TOK_LIBRARY},
      {"struct", TOK_STRUCT}, {"true", TOK_TRUE},     {"false", TOK_FALSE},
      {"nil", TOK_NIL},       {"int", TOK_INT_TYPE},  {"float", TOK_FLOAT_TYPE},
      {"bool", TOK_BOOL_TYPE}, {"string", TOK_STRING_TYPE},
      {"chan", TOK_CHAN},     {"list", TOK_LIST},     {"dict", TOK_DICT},
  };
  for (size_t i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
    if (strlen(tab[i].kw) == len && strncmp(tab[i].kw, s, len) == 0)
      return tab[i].kind;
  }
  return TOK_IDENT;
}

static Token lex_string(FlowLexer *L) {
  advance(L);
  const char *a = L->src + L->pos;
  while (L->pos < L->len && L->src[L->pos] != '"') {
    if (L->src[L->pos] == '\\')
      advance(L);
    advance(L);
  }
  const char *end = L->src + L->pos;
  if (L->pos < L->len)
    advance(L);
  return make_tok(L, TOK_STRING, a, (size_t)(end - a));
}

static Token lex_backtick(FlowLexer *L) {
  advance(L);
  const char *a = L->src + L->pos;
  while (L->pos < L->len && L->src[L->pos] != '`')
    advance(L);
  const char *end = L->src + L->pos;
  if (L->pos < L->len)
    advance(L);
  return make_tok(L, TOK_BACKTICK_CMD, a, (size_t)(end - a));
}

static Token lex_number(FlowLexer *L) {
  const char *start = L->src + L->pos;
  int is_float = 0;
  while (L->pos < L->len && isdigit((unsigned char)L->src[L->pos]))
    advance(L);
  if (L->pos < L->len && L->src[L->pos] == '.') {
    is_float = 1;
    advance(L);
    while (L->pos < L->len && isdigit((unsigned char)L->src[L->pos]))
      advance(L);
  }
  if (L->pos < L->len && (L->src[L->pos] == 'e' || L->src[L->pos] == 'E')) {
    is_float = 1;
    advance(L);
    if (L->pos < L->len && (L->src[L->pos] == '+' || L->src[L->pos] == '-'))
      advance(L);
    while (L->pos < L->len && isdigit((unsigned char)L->src[L->pos]))
      advance(L);
  }
  return make_tok(L, is_float ? TOK_FLOAT : TOK_INT, start,
                  (size_t)((L->src + L->pos) - start));
}

static Token lex_ident(FlowLexer *L) {
  const char *start = L->src + L->pos;
  while (L->pos < L->len && (isalnum((unsigned char)L->src[L->pos]) || L->src[L->pos] == '_'))
    advance(L);
  size_t len = (size_t)((L->src + L->pos) - start);
  TokKind k = ident_kind(start, len);
  return make_tok(L, k, start, len);
}

Token flow_lexer_next(FlowLexer *L) {
  skip_ws(L);
  if (L->pos >= L->len)
    return make_tok(L, TOK_EOF, L->src + L->pos, 0);

  if (L->src[L->pos] == '=' && peek(L, 1) == '>') {
    const char *start = L->src + L->pos;
    advance(L);
    advance(L);
    return make_tok(L, TOK_FATARROW, start, 2);
  }

  char c = L->src[L->pos];
  const char *start = L->src + L->pos;

  if (isdigit((unsigned char)c))
    return lex_number(L);

  if (c == '"')
    return lex_string(L);

  if (c == '`')
    return lex_backtick(L);

  if (isalpha((unsigned char)c) || c == '_')
    return lex_ident(L);

  advance(L);
  switch (c) {
  case '(':
    return make_tok(L, TOK_LPAREN, start, 1);
  case ')':
    return make_tok(L, TOK_RPAREN, start, 1);
  case '{':
    return make_tok(L, TOK_LBRACE, start, 1);
  case '}':
    return make_tok(L, TOK_RBRACE, start, 1);
  case '[':
    return make_tok(L, TOK_LBRACKET, start, 1);
  case ']':
    return make_tok(L, TOK_RBRACKET, start, 1);
  case ',':
    return make_tok(L, TOK_COMMA, start, 1);
  case ';':
    return make_tok(L, TOK_SEMI, start, 1);
  case ':':
    return make_tok(L, TOK_COLON, start, 1);
  case '.':
    return make_tok(L, TOK_DOT, start, 1);
  case '+':
    return make_tok(L, TOK_PLUS, start, 1);
  case '-':
    return make_tok(L, TOK_MINUS, start, 1);
  case '*':
    return make_tok(L, TOK_STAR, start, 1);
  case '/':
    return make_tok(L, TOK_SLASH, start, 1);
  case '%':
    return make_tok(L, TOK_PERCENT, start, 1);
  case '?':
    return make_tok(L, TOK_QMARK, start, 1);
  case '=':
    if (peek(L, 0) == '=') {
      advance(L);
      return make_tok(L, TOK_EQEQ, start, 2);
    }
    return make_tok(L, TOK_EQ, start, 1);
  case '!':
    if (peek(L, 0) == '=') {
      advance(L);
      return make_tok(L, TOK_BANGEQ, start, 2);
    }
    return make_tok(L, TOK_BANG, start, 1);
  case '<':
    if (peek(L, 0) == '=') {
      advance(L);
      return make_tok(L, TOK_LTE, start, 2);
    }
    return make_tok(L, TOK_LT, start, 1);
  case '>':
    if (peek(L, 0) == '=') {
      advance(L);
      return make_tok(L, TOK_GTE, start, 2);
    }
    return make_tok(L, TOK_GT, start, 1);
  case '&':
    if (peek(L, 0) == '&') {
      advance(L);
      return make_tok(L, TOK_ANDAND, start, 2);
    }
    break;
  case '|':
    if (peek(L, 0) == '|') {
      advance(L);
      return make_tok(L, TOK_OROR, start, 2);
    }
    break;
  default:
    break;
  }
  return make_tok(L, TOK_ERROR, start, 1);
}
