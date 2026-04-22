#ifndef FLOW_VALUE_H
#define FLOW_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Env Env;
typedef struct Interp Interp;
typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef enum {
  V_NIL,
  V_BOOL,
  V_INT,
  V_FLOAT,
  V_STRING,
  V_LIST,
  V_FUNC,   /* user function */
  V_NATIVE, /* builtin */
  V_LAMBDA, /* closure over expr body */
} ValKind;

typedef struct Value Value;

typedef Value (*NativeFn)(Interp *ip, const Value *args, size_t argc, void *userdata);

struct Value {
  ValKind kind;
  union {
    bool b;
    int64_t i;
    double f;
    char *s;
    struct {
      Value *items;
      size_t len;
    } list;
    struct {
      char **params;
      size_t nparams;
      Stmt *body; /* block stmt */
      Env *env;
    } func;
    struct {
      NativeFn fn;
      void *userdata;
    } native;
    struct {
      char **params;
      size_t nparams;
      Expr *body;
      Env *env;
    } lambda;
  } as;
};

void value_free_inner(Value *v);
void value_destroy(Value *v);

void value_print(const Value *v);

Value value_nil(void);
Value value_bool(bool b);
Value value_int(int64_t i);
Value value_float(double f);
Value value_string_owned(char *s);
Value value_string_copy(const char *s);
Value value_list(Value *items, size_t len);

#endif
