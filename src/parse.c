#include "flow_parse.h"
#include "flow_ast.h"
#include "flow_lexer.h"
#include "flow_token.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  FlowLexer lx;
  Token tok;
  char errbuf[512];
  char *err;
} Parser;

static void p_adv(Parser *p) { p->tok = flow_lexer_next(&p->lx); }

static char *tok_dup(const Token *t) {
  char *s = malloc(t->len + 1);
  if (!s)
    return NULL;
  memcpy(s, t->start, t->len);
  s[t->len] = 0;
  return s;
}

static char *unescape_string(const char *src, size_t len) {
  char *out = malloc(len + 1);
  if (!out)
    return NULL;
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if (src[i] == '\\' && i + 1 < len) {
      i++;
      switch (src[i]) {
      case 'n':
        out[j++] = '\n';
        break;
      case 't':
        out[j++] = '\t';
        break;
      case '\\':
        out[j++] = '\\';
        break;
      case '"':
        out[j++] = '"';
        break;
      default:
        out[j++] = src[i];
        break;
      }
    } else
      out[j++] = src[i];
  }
  out[j] = 0;
  return out;
}

static void parse_err(Parser *p, const char *msg) {
  snprintf(p->errbuf, sizeof p->errbuf, "%s (line %ld)", msg, (long)p->tok.line);
  p->err = p->errbuf;
}

static Expr *parse_expr(Parser *p);
static Stmt *parse_stmt(Parser *p);
static Stmt *parse_block(Parser *p);

/* Semicolons are optional everywhere (only consumed when present). */
static void semi_or_end(Parser *p) {
  if (p->tok.kind == TOK_SEMI)
    p_adv(p);
}

static char *expect_ident(Parser *p) {
  if (p->tok.kind != TOK_IDENT) {
    parse_err(p, "expected identifier");
    return NULL;
  }
  char *s = tok_dup(&p->tok);
  p_adv(p);
  return s;
}

static Expr *parse_primary(Parser *p) {
  Token t = p->tok;
  if (t.kind == TOK_INT) {
    char *tmp = tok_dup(&t);
    p_adv(p);
    long long v = strtoll(tmp, NULL, 10);
    free(tmp);
    return expr_literal_int(v);
  }
  if (t.kind == TOK_FLOAT) {
    char *tmp = tok_dup(&t);
    p_adv(p);
    double v = strtod(tmp, NULL);
    free(tmp);
    return expr_literal_float(v);
  }
  if (t.kind == TOK_STRING) {
    char *raw = tok_dup(&t);
    p_adv(p);
    char *s = unescape_string(raw, strlen(raw));
    free(raw);
    return expr_literal_string(s);
  }
  if (t.kind == TOK_TRUE) {
    p_adv(p);
    return expr_literal_bool(true);
  }
  if (t.kind == TOK_FALSE) {
    p_adv(p);
    return expr_literal_bool(false);
  }
  if (t.kind == TOK_NIL) {
    p_adv(p);
    return expr_literal_nil();
  }
  if (t.kind == TOK_IDENT) {
    char *name = tok_dup(&t);
    p_adv(p);
    if (p->tok.kind == TOK_FATARROW) {
      p_adv(p);
      Expr *body = parse_expr(p);
      char **ps = malloc(sizeof(char *));
      if (!ps) {
        free(name);
        return NULL;
      }
      ps[0] = name;
      return expr_lambda(ps, 1, body);
    }
    return expr_var(name);
  }
  if (t.kind == TOK_LPAREN) {
    p_adv(p);
    Expr *e = parse_expr(p);
    if (p->tok.kind != TOK_RPAREN) {
      parse_err(p, "expected ')'");
      expr_free(e);
      return NULL;
    }
    p_adv(p);
    return e;
  }
  if (t.kind == TOK_LBRACKET) {
    p_adv(p);
    Expr **items = NULL;
    size_t n = 0, cap = 0;
    if (p->tok.kind != TOK_RBRACKET) {
      for (;;) {
        Expr *e = parse_expr(p);
        if (!e) {
          for (size_t i = 0; i < n; i++)
            expr_free(items[i]);
          free(items);
          return NULL;
        }
        if (n >= cap) {
          cap = cap ? cap * 2 : 8;
          items = realloc(items, cap * sizeof(Expr *));
        }
        items[n++] = e;
        if (p->tok.kind == TOK_COMMA) {
          p_adv(p);
          continue;
        }
        break;
      }
    }
    if (p->tok.kind != TOK_RBRACKET) {
      parse_err(p, "expected ']'");
      for (size_t i = 0; i < n; i++)
        expr_free(items[i]);
      free(items);
      return NULL;
    }
    p_adv(p);
    return expr_list_lit(items, n);
  }
  if (t.kind == TOK_MATCH) {
    p_adv(p);
    Expr *scr = parse_expr(p);
    if (!scr)
      return NULL;
    if (p->tok.kind != TOK_LBRACE) {
      parse_err(p, "expected '{' after match");
      expr_free(scr);
      return NULL;
    }
    p_adv(p);
    int *pat_kinds = NULL;
    long long *pat_ints = NULL;
    Expr **arm_exprs = NULL;
    size_t narms = 0, cap = 0;
    while (p->tok.kind != TOK_RBRACE && p->tok.kind != TOK_EOF) {
      int pk;
      long long pi = 0;
      if (p->tok.kind == TOK_INT) {
        char *tmp = tok_dup(&p->tok);
        p_adv(p);
        pk = 0;
        pi = strtoll(tmp, NULL, 10);
        free(tmp);
      } else if (p->tok.kind == TOK_IDENT) {
        char *nm = tok_dup(&p->tok);
        p_adv(p);
        if (strcmp(nm, "_") != 0) {
          parse_err(p, "match pattern must be int or _");
          free(nm);
          goto match_fail;
        }
        free(nm);
        pk = 1;
      } else {
        parse_err(p, "bad match pattern");
        goto match_fail;
      }
      if (p->tok.kind != TOK_FATARROW) {
        parse_err(p, "expected '=>'");
        goto match_fail;
      }
      p_adv(p);
      Expr *ae = parse_expr(p);
      if (!ae)
        goto match_fail;
      if (narms >= cap) {
        cap = cap ? cap * 2 : 4;
        pat_kinds = realloc(pat_kinds, cap * sizeof(int));
        pat_ints = realloc(pat_ints, cap * sizeof(long long));
        arm_exprs = realloc(arm_exprs, cap * sizeof(Expr *));
      }
      pat_kinds[narms] = pk;
      pat_ints[narms] = pi;
      arm_exprs[narms] = ae;
      narms++;
      if (p->tok.kind == TOK_COMMA)
        p_adv(p);
    }
    if (p->tok.kind != TOK_RBRACE) {
      parse_err(p, "expected '}'");
    match_fail:
      expr_free(scr);
      for (size_t i = 0; i < narms; i++)
        expr_free(arm_exprs[i]);
      free(pat_kinds);
      free(pat_ints);
      free(arm_exprs);
      return NULL;
    }
    p_adv(p);
    return expr_match(scr, pat_kinds, pat_ints, arm_exprs, narms);
  }
  parse_err(p, "unexpected token in expression");
  return NULL;
}

static Expr *parse_postfix(Parser *p) {
  Expr *e = parse_primary(p);
  if (!e)
    return NULL;
  for (;;) {
    if (p->tok.kind == TOK_LPAREN) {
      p_adv(p);
      Expr **args = NULL;
      size_t na = 0, cap = 0;
      if (p->tok.kind != TOK_RPAREN) {
        for (;;) {
          Expr *a = parse_expr(p);
          if (!a) {
            for (size_t i = 0; i < na; i++)
              expr_free(args[i]);
            free(args);
            expr_free(e);
            return NULL;
          }
          if (na >= cap) {
            cap = cap ? cap * 2 : 4;
            args = realloc(args, cap * sizeof(Expr *));
          }
          args[na++] = a;
          if (p->tok.kind == TOK_COMMA) {
            p_adv(p);
            continue;
          }
          break;
        }
      }
      if (p->tok.kind != TOK_RPAREN) {
        parse_err(p, "expected ')'");
        for (size_t i = 0; i < na; i++)
          expr_free(args[i]);
        free(args);
        expr_free(e);
        return NULL;
      }
      p_adv(p);
      Expr *call = expr_call(e, args, na);
      e = call;
    } else if (p->tok.kind == TOK_DOT) {
      p_adv(p);
      char *name = expect_ident(p);
      if (!name) {
        expr_free(e);
        return NULL;
      }
      e = expr_member(e, name);
    } else if (p->tok.kind == TOK_LBRACKET) {
      p_adv(p);
      Expr *ix = parse_expr(p);
      if (!ix) {
        expr_free(e);
        return NULL;
      }
      if (p->tok.kind != TOK_RBRACKET) {
        parse_err(p, "expected ']'");
        expr_free(ix);
        expr_free(e);
        return NULL;
      }
      p_adv(p);
      e = expr_index(e, ix);
    } else
      break;
  }
  return e;
}

static Expr *parse_unary(Parser *p) {
  if (p->tok.kind == TOK_BANG || p->tok.kind == TOK_MINUS) {
    int op = p->tok.kind;
    p_adv(p);
    Expr *r = parse_unary(p);
    if (!r)
      return NULL;
    return expr_unary(op, r);
  }
  return parse_postfix(p);
}

static Expr *parse_mul(Parser *p) {
  Expr *e = parse_unary(p);
  if (!e)
    return NULL;
  for (;;) {
    TokKind op = p->tok.kind;
    if (op != TOK_STAR && op != TOK_SLASH && op != TOK_PERCENT)
      break;
    p_adv(p);
    Expr *r = parse_unary(p);
    if (!r) {
      expr_free(e);
      return NULL;
    }
    e = expr_binary(e, op, r);
  }
  return e;
}

static Expr *parse_add(Parser *p) {
  Expr *e = parse_mul(p);
  if (!e)
    return NULL;
  for (;;) {
    TokKind op = p->tok.kind;
    if (op != TOK_PLUS && op != TOK_MINUS)
      break;
    p_adv(p);
    Expr *r = parse_mul(p);
    if (!r) {
      expr_free(e);
      return NULL;
    }
    e = expr_binary(e, op, r);
  }
  return e;
}

static Expr *parse_cmp(Parser *p) {
  Expr *e = parse_add(p);
  if (!e)
    return NULL;
  for (;;) {
    TokKind op = p->tok.kind;
    if (op != TOK_LT && op != TOK_LTE && op != TOK_GT && op != TOK_GTE)
      break;
    p_adv(p);
    Expr *r = parse_add(p);
    if (!r) {
      expr_free(e);
      return NULL;
    }
    e = expr_binary(e, op, r);
  }
  return e;
}

static Expr *parse_eq(Parser *p) {
  Expr *e = parse_cmp(p);
  if (!e)
    return NULL;
  for (;;) {
    TokKind op = p->tok.kind;
    if (op != TOK_EQEQ && op != TOK_BANGEQ)
      break;
    p_adv(p);
    Expr *r = parse_cmp(p);
    if (!r) {
      expr_free(e);
      return NULL;
    }
    e = expr_binary(e, op, r);
  }
  return e;
}

static Expr *parse_and(Parser *p) {
  Expr *e = parse_eq(p);
  if (!e)
    return NULL;
  while (p->tok.kind == TOK_ANDAND) {
    p_adv(p);
    Expr *r = parse_eq(p);
    if (!r) {
      expr_free(e);
      return NULL;
    }
    e = expr_binary(e, TOK_ANDAND, r);
  }
  return e;
}

static Expr *parse_or(Parser *p) {
  Expr *e = parse_and(p);
  if (!e)
    return NULL;
  while (p->tok.kind == TOK_OROR) {
    p_adv(p);
    Expr *r = parse_and(p);
    if (!r) {
      expr_free(e);
      return NULL;
    }
    e = expr_binary(e, TOK_OROR, r);
  }
  return e;
}

static Expr *parse_expr(Parser *p) { return parse_or(p); }

static Stmt *parse_block(Parser *p) {
  if (p->tok.kind != TOK_LBRACE) {
    parse_err(p, "expected '{'");
    return NULL;
  }
  p_adv(p);
  Stmt **stmts = NULL;
  size_t n = 0, cap = 0;
  while (p->tok.kind != TOK_RBRACE && p->tok.kind != TOK_EOF) {
    Stmt *s = parse_stmt(p);
    if (!s) {
      for (size_t i = 0; i < n; i++)
        stmt_free(stmts[i]);
      free(stmts);
      return NULL;
    }
    if (n >= cap) {
      cap = cap ? cap * 2 : 8;
      stmts = realloc(stmts, cap * sizeof(Stmt *));
    }
    stmts[n++] = s;
    if (p->err)
      break;
  }
  if (p->tok.kind != TOK_RBRACE) {
    parse_err(p, "expected '}'");
    for (size_t i = 0; i < n; i++)
      stmt_free(stmts[i]);
    free(stmts);
    return NULL;
  }
  p_adv(p);
  return stmt_block(stmts, n);
}

static Stmt *parse_binding_let(Parser *p, int exported) {
  char *name = expect_ident(p);
  if (!name)
    return NULL;
  if (p->tok.kind != TOK_EQ) {
    parse_err(p, "expected '=' in let");
    free(name);
    return NULL;
  }
  p_adv(p);
  Expr *e = parse_expr(p);
  if (!e) {
    free(name);
    return NULL;
  }
  semi_or_end(p);
  return stmt_let(name, e, exported);
}

static Stmt *parse_stmt(Parser *p) {
  if (p->tok.kind == TOK_LET) {
    p_adv(p);
    return parse_binding_let(p, 0);
  }
  if (p->tok.kind == TOK_RETURN) {
    p_adv(p);
    Expr **es = NULL;
    size_t ne = 0, cap = 0;
    if (p->tok.kind != TOK_SEMI && p->tok.kind != TOK_RBRACE) {
      for (;;) {
        Expr *e = parse_expr(p);
        if (!e) {
          for (size_t i = 0; i < ne; i++)
            expr_free(es[i]);
          free(es);
          return NULL;
        }
        if (ne >= cap) {
          cap = cap ? cap * 2 : 4;
          es = realloc(es, cap * sizeof(Expr *));
        }
        es[ne++] = e;
        if (p->tok.kind == TOK_COMMA) {
          p_adv(p);
          continue;
        }
        break;
      }
    }
    semi_or_end(p);
    return stmt_return(es, ne);
  }
  if (p->tok.kind == TOK_IF) {
    p_adv(p);
    if (p->tok.kind != TOK_LPAREN) {
      parse_err(p, "expected '(' after if");
      return NULL;
    }
    p_adv(p);
    Expr *cond = parse_expr(p);
    if (!cond)
      return NULL;
    if (p->tok.kind != TOK_RPAREN) {
      parse_err(p, "expected ')'");
      expr_free(cond);
      return NULL;
    }
    p_adv(p);
    Stmt *then_s = parse_stmt(p);
    if (!then_s) {
      expr_free(cond);
      return NULL;
    }
    Stmt *else_s = NULL;
    if (p->tok.kind == TOK_ELSE) {
      p_adv(p);
      else_s = parse_stmt(p);
      if (!else_s) {
        expr_free(cond);
        stmt_free(then_s);
        return NULL;
      }
    }
    return stmt_if(cond, then_s, else_s);
  }
  if (p->tok.kind == TOK_WHILE) {
    p_adv(p);
    if (p->tok.kind != TOK_LPAREN) {
      parse_err(p, "expected '(' after while");
      return NULL;
    }
    p_adv(p);
    Expr *cond = parse_expr(p);
    if (!cond)
      return NULL;
    if (p->tok.kind != TOK_RPAREN) {
      parse_err(p, "expected ')'");
      expr_free(cond);
      return NULL;
    }
    p_adv(p);
    Stmt *body = parse_stmt(p);
    if (!body) {
      expr_free(cond);
      return NULL;
    }
    return stmt_while(cond, body);
  }
  if (p->tok.kind == TOK_FOR) {
    p_adv(p);
    if (p->tok.kind != TOK_LPAREN) {
      parse_err(p, "expected '(' after for");
      return NULL;
    }
    p_adv(p);
    char *it = expect_ident(p);
    if (!it)
      return NULL;
    if (p->tok.kind != TOK_IN) {
      parse_err(p, "expected 'in'");
      free(it);
      return NULL;
    }
    p_adv(p);
    Expr *iter = parse_expr(p);
    if (!iter) {
      free(it);
      return NULL;
    }
    if (p->tok.kind != TOK_RPAREN) {
      parse_err(p, "expected ')'");
      free(it);
      expr_free(iter);
      return NULL;
    }
    p_adv(p);
    Stmt *body = parse_stmt(p);
    if (!body) {
      free(it);
      expr_free(iter);
      return NULL;
    }
    return stmt_for(it, iter, body);
  }
  if (p->tok.kind == TOK_BREAK) {
    p_adv(p);
    semi_or_end(p);
    return stmt_break();
  }
  if (p->tok.kind == TOK_CONTINUE) {
    p_adv(p);
    semi_or_end(p);
    return stmt_continue();
  }
  if (p->tok.kind == TOK_LBRACE)
    return parse_block(p);
  Expr *ex = parse_expr(p);
  if (!ex)
    return NULL;
  semi_or_end(p);
  return stmt_expr(ex);
}

static Stmt *parse_func(Parser *p, int exported) {
  char *name = expect_ident(p);
  if (!name)
    return NULL;
  if (p->tok.kind != TOK_LPAREN) {
    parse_err(p, "expected '(' after func name");
    free(name);
    return NULL;
  }
  p_adv(p);
  char **params = NULL;
  size_t np = 0, cap = 0;
  if (p->tok.kind != TOK_RPAREN) {
    for (;;) {
      char *pn = expect_ident(p);
      if (!pn) {
        for (size_t i = 0; i < np; i++)
          free(params[i]);
        free(params);
        free(name);
        return NULL;
      }
      if (np >= cap) {
        cap = cap ? cap * 2 : 4;
        params = realloc(params, cap * sizeof(char *));
      }
      params[np++] = pn;
      if (p->tok.kind == TOK_COMMA) {
        p_adv(p);
        continue;
      }
      break;
    }
  }
  if (p->tok.kind != TOK_RPAREN) {
    parse_err(p, "expected ')'");
    for (size_t i = 0; i < np; i++)
      free(params[i]);
    free(params);
    free(name);
    return NULL;
  }
  p_adv(p);
  Stmt *body = parse_block(p);
  if (!body) {
    for (size_t i = 0; i < np; i++)
      free(params[i]);
    free(params);
    free(name);
    return NULL;
  }
  return stmt_func(name, params, np, body, exported);
}

Program *flow_parse(const char *src, size_t len, char **err_out) {
  Parser p;
  memset(&p, 0, sizeof p);
  flow_lexer_init(&p.lx, src, len);
  p.tok = flow_lexer_next(&p.lx);

  Stmt **items = NULL;
  size_t n = 0, cap = 0;
  while (p.tok.kind != TOK_EOF) {
    Stmt *s = NULL;
    if (p.tok.kind == TOK_LIBRARY) {
      p_adv(&p);
      if (p.tok.kind != TOK_STRING) {
        parse_err(&p, "expected string literal after library");
        s = NULL;
      } else {
        char *raw = tok_dup(&p.tok);
        p_adv(&p);
        char *libn = unescape_string(raw, strlen(raw));
        free(raw);
        semi_or_end(&p);
        s = stmt_library(libn);
      }
    } else if (p.tok.kind == TOK_IMPORT) {
      p_adv(&p);
      if (p.tok.kind != TOK_STRING) {
        parse_err(&p, "expected string path after import");
        s = NULL;
      } else {
        char *raw = tok_dup(&p.tok);
        p_adv(&p);
        char *ipath = unescape_string(raw, strlen(raw));
        free(raw);
        semi_or_end(&p);
        s = stmt_import(ipath);
      }
    } else if (p.tok.kind == TOK_EXPORT) {
      p_adv(&p);
      if (p.tok.kind == TOK_FUNC) {
        p_adv(&p);
        s = parse_func(&p, 1);
      } else if (p.tok.kind == TOK_LET) {
        p_adv(&p);
        s = parse_binding_let(&p, 1);
      } else {
        parse_err(&p, "expected func or let after export");
        s = NULL;
      }
    } else if (p.tok.kind == TOK_FUNC) {
      p_adv(&p);
      s = parse_func(&p, 0);
    } else if (p.tok.kind == TOK_LET) {
      p_adv(&p);
      s = parse_binding_let(&p, 0);
    } else {
      Expr *ex = parse_expr(&p);
      if (!ex) {
        for (size_t i = 0; i < n; i++)
          stmt_free(items[i]);
        free(items);
        if (err_out)
          *err_out = p.err ? strdup(p.err) : strdup("parse error");
        return NULL;
      }
      semi_or_end(&p);
      s = stmt_expr(ex);
    }
    if (!s || p.err) {
      for (size_t i = 0; i < n; i++)
        stmt_free(items[i]);
      free(items);
      stmt_free(s);
      if (err_out)
        *err_out = p.err ? strdup(p.err) : strdup("parse error");
      return NULL;
    }
    if (n >= cap) {
      cap = cap ? cap * 2 : 8;
      items = realloc(items, cap * sizeof(Stmt *));
    }
    items[n++] = s;
  }

  Program *prog = malloc(sizeof(Program));
  if (!prog) {
    for (size_t i = 0; i < n; i++)
      stmt_free(items[i]);
    free(items);
    if (err_out)
      *err_out = strdup("out of memory");
    return NULL;
  }
  prog->items = items;
  prog->nitems = n;
  if (err_out)
    *err_out = NULL;
  return prog;
}
