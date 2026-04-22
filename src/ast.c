#include "flow_ast.h"
#include <stdlib.h>
#include <string.h>

static Expr *expr_alloc(ExprKind k) {
  Expr *e = calloc(1, sizeof(Expr));
  e->kind = k;
  return e;
}

Expr *expr_literal_nil(void) {
  Expr *e = expr_alloc(EXPR_LITERAL);
  e->as.lit.tag = LIT_NIL;
  return e;
}

Expr *expr_literal_bool(bool v) {
  Expr *e = expr_alloc(EXPR_LITERAL);
  e->as.lit.tag = LIT_BOOL;
  e->as.lit.bval = v;
  return e;
}

Expr *expr_literal_int(long long v) {
  Expr *e = expr_alloc(EXPR_LITERAL);
  e->as.lit.tag = LIT_INT;
  e->as.lit.ival = v;
  return e;
}

Expr *expr_literal_float(double v) {
  Expr *e = expr_alloc(EXPR_LITERAL);
  e->as.lit.tag = LIT_FLOAT;
  e->as.lit.fval = v;
  return e;
}

Expr *expr_literal_string(char *s) {
  Expr *e = expr_alloc(EXPR_LITERAL);
  e->as.lit.tag = LIT_STRING;
  e->as.lit.sval = s;
  return e;
}

Expr *expr_var(char *name) {
  Expr *e = expr_alloc(EXPR_VAR);
  e->as.var.name = name;
  return e;
}

Expr *expr_binary(Expr *l, int op, Expr *r) {
  Expr *e = expr_alloc(EXPR_BINARY);
  e->as.binary.left = l;
  e->as.binary.op = op;
  e->as.binary.right = r;
  return e;
}

Expr *expr_unary(int op, Expr *r) {
  Expr *e = expr_alloc(EXPR_UNARY);
  e->as.unary.op = op;
  e->as.unary.right = r;
  return e;
}

Expr *expr_call(Expr *callee, Expr **args, size_t argc) {
  Expr *e = expr_alloc(EXPR_CALL);
  e->as.call.callee = callee;
  e->as.call.args = args;
  e->as.call.argc = argc;
  return e;
}

Expr *expr_index(Expr *obj, Expr *idx) {
  Expr *e = expr_alloc(EXPR_INDEX);
  e->as.index.obj = obj;
  e->as.index.index = idx;
  return e;
}

Expr *expr_member(Expr *obj, char *field) {
  Expr *e = expr_alloc(EXPR_MEMBER);
  e->as.member.obj = obj;
  e->as.member.field = field;
  return e;
}

Expr *expr_lambda(char **params, size_t nparams, Expr *body) {
  Expr *e = expr_alloc(EXPR_LAMBDA);
  e->as.lambda.params = params;
  e->as.lambda.nparams = nparams;
  e->as.lambda.body = body;
  return e;
}

Expr *expr_match(Expr *scrutinee, int *pat_kinds, long long *pat_ints, Expr **arm_exprs,
                 size_t narms) {
  Expr *e = expr_alloc(EXPR_MATCH);
  e->as.match.scrutinee = scrutinee;
  e->as.match.pat_kinds = pat_kinds;
  e->as.match.pat_ints = pat_ints;
  e->as.match.arm_exprs = arm_exprs;
  e->as.match.narms = narms;
  return e;
}

Expr *expr_list_lit(Expr **items, size_t nitems) {
  Expr *e = expr_alloc(EXPR_LIST_LIT);
  e->as.list_lit.items = items;
  e->as.list_lit.nitems = nitems;
  return e;
}

Stmt *stmt_block(Stmt **stmts, size_t nstmts) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_BLOCK;
  s->as.block.stmts = stmts;
  s->as.block.nstmts = nstmts;
  return s;
}

Stmt *stmt_let(char *name, Expr *init, int exported) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_LET;
  s->as.let.name = name;
  s->as.let.init = init;
  s->as.let.exported = exported;
  return s;
}

Stmt *stmt_expr(Expr *e) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_EXPR;
  s->as.expr.expr = e;
  return s;
}

Stmt *stmt_if(Expr *cond, Stmt *then_s, Stmt *else_s) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_IF;
  s->as.if_s.cond = cond;
  s->as.if_s.then_stmt = then_s;
  s->as.if_s.else_stmt = else_s;
  return s;
}

Stmt *stmt_while(Expr *cond, Stmt *body) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_WHILE;
  s->as.while_s.cond = cond;
  s->as.while_s.body = body;
  return s;
}

Stmt *stmt_for(char *iter, Expr *iterable, Stmt *body) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_FOR;
  s->as.for_s.iter = iter;
  s->as.for_s.iterable = iterable;
  s->as.for_s.body = body;
  return s;
}

Stmt *stmt_return(Expr **exprs, size_t nexprs) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_RETURN;
  s->as.ret.exprs = exprs;
  s->as.ret.nexprs = nexprs;
  return s;
}

Stmt *stmt_func(char *name, char **params, size_t nparams, Stmt *body, int exported) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_FUNC;
  s->as.func.name = name;
  s->as.func.params = params;
  s->as.func.nparams = nparams;
  s->as.func.body = body;
  s->as.func.exported = exported;
  return s;
}

Stmt *stmt_import(char *path) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_IMPORT;
  s->as.import_s.path = path;
  return s;
}

Stmt *stmt_library(char *lib_name) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_LIBRARY;
  s->as.library.lib_name = lib_name;
  return s;
}

Stmt *stmt_break(void) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_BREAK;
  return s;
}

Stmt *stmt_continue(void) {
  Stmt *s = calloc(1, sizeof(Stmt));
  s->kind = STMT_CONTINUE;
  return s;
}

static void expr_free_inner(Expr *e);

void expr_free(Expr *e) {
  if (!e) return;
  expr_free_inner(e);
  free(e);
}

static void expr_free_inner(Expr *e) {
  switch (e->kind) {
  case EXPR_LITERAL:
    if (e->as.lit.tag == LIT_STRING && e->as.lit.sval)
      free(e->as.lit.sval);
    break;
  case EXPR_VAR:
    free(e->as.var.name);
    break;
  case EXPR_BINARY:
    expr_free(e->as.binary.left);
    expr_free(e->as.binary.right);
    break;
  case EXPR_UNARY:
    expr_free(e->as.unary.right);
    break;
  case EXPR_CALL:
    expr_free(e->as.call.callee);
    for (size_t i = 0; i < e->as.call.argc; i++)
      expr_free(e->as.call.args[i]);
    free(e->as.call.args);
    break;
  case EXPR_INDEX:
    expr_free(e->as.index.obj);
    expr_free(e->as.index.index);
    break;
  case EXPR_MEMBER:
    expr_free(e->as.member.obj);
    free(e->as.member.field);
    break;
  case EXPR_LAMBDA:
    for (size_t i = 0; i < e->as.lambda.nparams; i++)
      free(e->as.lambda.params[i]);
    free(e->as.lambda.params);
    expr_free(e->as.lambda.body);
    break;
  case EXPR_MATCH:
    expr_free(e->as.match.scrutinee);
    for (size_t i = 0; i < e->as.match.narms; i++)
      expr_free(e->as.match.arm_exprs[i]);
    free(e->as.match.pat_kinds);
    free(e->as.match.pat_ints);
    free(e->as.match.arm_exprs);
    break;
  case EXPR_LIST_LIT:
    for (size_t i = 0; i < e->as.list_lit.nitems; i++)
      expr_free(e->as.list_lit.items[i]);
    free(e->as.list_lit.items);
    break;
  default:
    break;
  }
}

static void stmt_free_inner(Stmt *s);

void stmt_free(Stmt *s) {
  if (!s) return;
  stmt_free_inner(s);
  free(s);
}

static void stmt_free_inner(Stmt *s) {
  switch (s->kind) {
  case STMT_BLOCK:
    for (size_t i = 0; i < s->as.block.nstmts; i++)
      stmt_free(s->as.block.stmts[i]);
    free(s->as.block.stmts);
    break;
  case STMT_LET:
    free(s->as.let.name);
    expr_free(s->as.let.init);
    break;
  case STMT_EXPR:
    expr_free(s->as.expr.expr);
    break;
  case STMT_IF:
    expr_free(s->as.if_s.cond);
    stmt_free(s->as.if_s.then_stmt);
    stmt_free(s->as.if_s.else_stmt);
    break;
  case STMT_WHILE:
    expr_free(s->as.while_s.cond);
    stmt_free(s->as.while_s.body);
    break;
  case STMT_FOR:
    free(s->as.for_s.iter);
    expr_free(s->as.for_s.iterable);
    stmt_free(s->as.for_s.body);
    break;
  case STMT_RETURN:
    for (size_t i = 0; i < s->as.ret.nexprs; i++)
      expr_free(s->as.ret.exprs[i]);
    free(s->as.ret.exprs);
    break;
  case STMT_FUNC:
    free(s->as.func.name);
    for (size_t i = 0; i < s->as.func.nparams; i++)
      free(s->as.func.params[i]);
    free(s->as.func.params);
    stmt_free(s->as.func.body);
    break;
  case STMT_IMPORT:
    free(s->as.import_s.path);
    break;
  case STMT_LIBRARY:
    free(s->as.library.lib_name);
    break;
  case STMT_BREAK:
  case STMT_CONTINUE:
    break;
  default:
    break;
  }
}

void program_free(Program *p) {
  if (!p) return;
  for (size_t i = 0; i < p->nitems; i++)
    stmt_free(p->items[i]);
  free(p->items);
  free(p);
}
