#ifndef FLOW_AST_H
#define FLOW_AST_H

#include <stddef.h>
#include <stdbool.h>

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Program Program;

typedef enum {
  EXPR_LITERAL,
  EXPR_VAR,
  EXPR_BINARY,
  EXPR_UNARY,
  EXPR_CALL,
  EXPR_INDEX,
  EXPR_MEMBER,
  EXPR_LAMBDA,
  EXPR_MATCH,
  EXPR_LIST_LIT,
} ExprKind;

typedef enum {
  STMT_BLOCK,
  STMT_LET,
  STMT_EXPR,
  STMT_IF,
  STMT_WHILE,
  STMT_FOR,
  STMT_RETURN,
  STMT_FUNC,
  STMT_BREAK,
  STMT_CONTINUE,
  STMT_IMPORT,
  STMT_LIBRARY,
} StmtKind;

typedef struct {
  Expr **arms; /* parallel patterns + exprs stored in MatchExpr */
} MatchArm;

struct Expr {
  ExprKind kind;
  union {
    struct {
      enum { LIT_INT, LIT_FLOAT, LIT_STRING, LIT_BOOL, LIT_NIL } tag;
      long long ival;
      double fval;
      char *sval;
      bool bval;
    } lit;
    struct {
      char *name;
    } var;
    struct {
      Expr *left;
      int op; /* TokKind */
      Expr *right;
    } binary;
    struct {
      int op;
      Expr *right;
    } unary;
    struct {
      Expr *callee;
      Expr **args;
      size_t argc;
    } call;
    struct {
      Expr *obj;
      Expr *index;
    } index;
    struct {
      Expr *obj;
      char *field;
    } member;
    struct {
      char **params;
      size_t nparams;
      Expr *body; /* single expression body */
    } lambda;
    struct {
      Expr *scrutinee;
      int *pat_kinds; /* 0=int 1=wildcard */
      long long *pat_ints;
      Expr **arm_exprs;
      size_t narms;
    } match;
    struct {
      Expr **items;
      size_t nitems;
    } list_lit;
  } as;
};

struct Stmt {
  StmtKind kind;
  union {
    struct {
      Stmt **stmts;
      size_t nstmts;
    } block;
    struct {
      char *name;
      Expr *init;
      int exported; /* top-level only; ignored inside blocks */
    } let;
    struct {
      Expr *expr;
    } expr;
    struct {
      Expr *cond;
      Stmt *then_stmt;
      Stmt *else_stmt;
    } if_s;
    struct {
      Expr *cond;
      Stmt *body;
    } while_s;
    struct {
      char *iter;
      Expr *iterable;
      Stmt *body;
    } for_s;
    struct {
      Expr **exprs;
      size_t nexprs; /* 0 = bare return */
    } ret;
    struct {
      char *name;
      char **params;
      size_t nparams;
      Stmt *body; /* block */
      int exported; /* top-level only */
    } func;
    struct {
      char *path;
    } import_s;
    struct {
      char *lib_name;
    } library;
  } as;
};

struct Program {
  Stmt **items;
  size_t nitems;
};

Expr *expr_literal_nil(void);
Expr *expr_literal_bool(bool v);
Expr *expr_literal_int(long long v);
Expr *expr_literal_float(double v);
Expr *expr_literal_string(char *s); /* takes ownership */
Expr *expr_var(char *name);
Expr *expr_binary(Expr *l, int op, Expr *r);
Expr *expr_unary(int op, Expr *r);
Expr *expr_call(Expr *callee, Expr **args, size_t argc);
Expr *expr_index(Expr *obj, Expr *idx);
Expr *expr_member(Expr *obj, char *field);
Expr *expr_lambda(char **params, size_t nparams, Expr *body);
Expr *expr_match(Expr *scrutinee, int *pat_kinds, long long *pat_ints, Expr **arm_exprs, size_t narms);
Expr *expr_list_lit(Expr **items, size_t nitems);

Stmt *stmt_block(Stmt **stmts, size_t nstmts);
Stmt *stmt_let(char *name, Expr *init, int exported);
Stmt *stmt_expr(Expr *e);
Stmt *stmt_if(Expr *cond, Stmt *then_s, Stmt *else_s);
Stmt *stmt_while(Expr *cond, Stmt *body);
Stmt *stmt_for(char *iter, Expr *iterable, Stmt *body);
Stmt *stmt_return(Expr **exprs, size_t nexprs);
Stmt *stmt_func(char *name, char **params, size_t nparams, Stmt *body, int exported);
Stmt *stmt_import(char *path);
Stmt *stmt_library(char *lib_name);
Stmt *stmt_break(void);
Stmt *stmt_continue(void);

void program_free(Program *p);
void stmt_free(Stmt *s);
void expr_free(Expr *e);

#endif
