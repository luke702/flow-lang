#include "flow_interp.h"
#include "flow_ast.h"
#include "flow_parse.h"
#include "flow_path.h"
#include "flow_value.h"
#include "flow_token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#if defined(_MSC_VER) && !defined(strdup)
#define strdup _strdup
#endif

typedef struct Binding {
  char *name;
  Value val;
  struct Binding *next;
} Binding;

struct Env {
  Binding *head;
  struct Env *parent;
};

typedef struct LoadStack {
  char **paths;
  size_t len;
  size_t cap;
} LoadStack;

typedef struct Interp {
  Env *env;
  Value ret;
  int has_ret;
  const char *source_path;
  LoadStack *loads;
  char **err_out;
} Interp;

static int push_load(LoadStack *st, const char *abs_path, char **err_out) {
  for (size_t i = 0; i < st->len; i++) {
    if (strcmp(st->paths[i], abs_path) == 0) {
      if (err_out && (!*err_out)) {
        char b[512];
        snprintf(b, sizeof b, "import cycle: %s", abs_path);
        *err_out = strdup(b);
      }
      return -1;
    }
  }
  if (st->len >= st->cap) {
    size_t ncap = st->cap ? st->cap * 2 : 8;
    char **np = realloc(st->paths, ncap * sizeof(char *));
    if (!np)
      return -1;
    st->paths = np;
    st->cap = ncap;
  }
  st->paths[st->len++] = strdup(abs_path);
  return 0;
}

static void pop_load(LoadStack *st) {
  if (st->len == 0)
    return;
  st->len--;
  free(st->paths[st->len]);
}

static Env *env_new(Env *parent) {
  Env *e = calloc(1, sizeof(Env));
  e->parent = parent;
  return e;
}

static void env_free(Env *e) {
  if (!e)
    return;
  for (Binding *b = e->head; b;) {
    Binding *n = b->next;
    free(b->name);
    value_destroy(&b->val);
    free(b);
    b = n;
  }
  free(e);
}

static void env_put(Env *e, const char *name, Value val) {
  Binding *b = malloc(sizeof(Binding));
  b->name = strdup(name);
  b->val = val;
  b->next = e->head;
  e->head = b;
}

static int env_get(Env *e, const char *name, Value *out) {
  for (Env *cur = e; cur; cur = cur->parent) {
    for (Binding *b = cur->head; b; b = b->next) {
      if (strcmp(b->name, name) == 0) {
        *out = b->val;
        return 1;
      }
    }
  }
  return 0;
}

static Value value_clone_shallow(const Value *v) {
  switch (v->kind) {
  case V_STRING:
    return value_string_copy(v->as.s);
  case V_LIST: {
    Value *items = malloc(sizeof(Value) * v->as.list.len);
    for (size_t i = 0; i < v->as.list.len; i++)
      items[i] = value_clone_shallow(&v->as.list.items[i]);
    return value_list(items, v->as.list.len);
  }
  default:
    return *v;
  }
}

static Value native_println(Interp *ip, const Value *args, size_t argc, void *ud) {
  (void)ip;
  (void)ud;
  for (size_t i = 0; i < argc; i++) {
    value_print(&args[i]);
    if (i + 1 < argc)
      printf(" ");
  }
  printf("\n");
  return value_nil();
}

static int url_ok_for_http_get(const char *url) {
  if (!url)
    return 0;
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
    return 0;
  for (const char *p = url; *p; p++) {
    if (*p == '"' || *p == '\r' || *p == '\n')
      return 0;
  }
  return 1;
}

static Value http_get_via_curl(const char *url) {
  char cmd[8192];
  int n = snprintf(cmd, sizeof cmd, "curl -sS -L --max-time 30 \"%s\"", url);
  if (n < 0 || (size_t)n >= sizeof cmd) {
    fprintf(stderr, "flow: http_get: URL too long\n");
    return value_string_copy("");
  }
#if defined(_WIN32)
  FILE *fp = _popen(cmd, "rb");
#else
  FILE *fp = popen(cmd, "r");
#endif
  if (!fp) {
    fprintf(stderr, "flow: http_get: could not run curl (install curl or add it to PATH)\n");
    return value_string_copy("");
  }
  size_t cap = 8192;
  size_t len = 0;
  char *buf = malloc(cap);
  if (!buf) {
#if defined(_WIN32)
    _pclose(fp);
#else
    pclose(fp);
#endif
    return value_string_copy("");
  }
  for (;;) {
    if (len + 4096 > cap) {
      size_t ncap = cap * 2;
      char *nb = realloc(buf, ncap);
      if (!nb) {
        free(buf);
#if defined(_WIN32)
        _pclose(fp);
#else
        pclose(fp);
#endif
        return value_string_copy("");
      }
      buf = nb;
      cap = ncap;
    }
    size_t nread = fread(buf + len, 1, cap - len - 1, fp);
    if (nread == 0)
      break;
    len += nread;
  }
  buf[len] = '\0';
#if defined(_WIN32)
  _pclose(fp);
#else
  pclose(fp);
#endif
  return value_string_owned(buf);
}

static Value native_http_get(Interp *ip, const Value *args, size_t argc, void *ud) {
  (void)ip;
  (void)ud;
  if (argc < 1 || args[0].kind != V_STRING) {
    fprintf(stderr, "flow: http_get expects one string (URL)\n");
    return value_string_copy("");
  }
  const char *url = args[0].as.s;
  if (!url_ok_for_http_get(url)) {
    fprintf(stderr,
            "flow: http_get: URL must start with http:// or https:// "
            "and must not contain quotes or newlines\n");
    return value_string_copy("");
  }
  return http_get_via_curl(url);
}

static void install_builtins(Env *e) {
  Value nv;
  nv.kind = V_NATIVE;
  nv.as.native.userdata = NULL;
  nv.as.native.fn = native_println;
  env_put(e, "println", nv);
  nv.as.native.fn = native_http_get;
  env_put(e, "http_get", nv);
}

static void flow_exec_stmt(Interp *ip, Stmt *s);
static int exec_import(Interp *ip, Stmt *s);
static int validate_exactly_one_main(Program *prog, char **err_out);
static int run_module_body(Interp *ip, Program *sub, int invoke_main, char **err_out);

static Value eval_expr(Interp *ip, Expr *e);

static Value call_user(Interp *ip, const Value *fn, Value *args, size_t argc) {
  Env *closure;
  char **params;
  size_t nparams;
  Stmt *body;

  if (fn->kind == V_FUNC) {
    closure = fn->as.func.env;
    params = fn->as.func.params;
    nparams = fn->as.func.nparams;
    body = fn->as.func.body;
  } else {
    return value_nil();
  }

  if (nparams != argc) {
    fprintf(stderr, "flow: wrong argument count\n");
    return value_nil();
  }

  Env *local = env_new(closure);
  for (size_t i = 0; i < argc; i++)
    env_put(local, params[i], value_clone_shallow(&args[i]));

  Interp sub;
  sub.env = local;
  sub.has_ret = 0;
  sub.ret = value_nil();
  sub.source_path = ip->source_path;
  sub.loads = ip->loads;
  sub.err_out = ip->err_out;

  if (body->kind == STMT_BLOCK) {
    for (size_t i = 0; i < body->as.block.nstmts; i++) {
      flow_exec_stmt(&sub, body->as.block.stmts[i]);
      if (sub.has_ret)
        break;
    }
  } else {
    flow_exec_stmt(&sub, body);
  }

  Value out = sub.has_ret ? sub.ret : value_nil();
  (void)sub.has_ret;
  env_free(local);
  return out;
}

static Value call_lambda(Interp *ip, const Value *lam, Value *args, size_t argc) {
  (void)ip;
  if (lam->kind != V_LAMBDA)
    return value_nil();
  if (lam->as.lambda.nparams != argc) {
    fprintf(stderr, "flow: lambda arity mismatch\n");
    return value_nil();
  }
  Env *local = env_new(lam->as.lambda.env);
  for (size_t i = 0; i < argc; i++)
    env_put(local, lam->as.lambda.params[i], value_clone_shallow(&args[i]));

  Interp sub;
  sub.env = local;
  sub.has_ret = 0;
  sub.ret = value_nil();
  Value r = eval_expr(&sub, lam->as.lambda.body);
  (void)sub.ret;
  env_free(local);
  return r;
}

static Value eval_member_call(Interp *ip, Expr *callee, Expr **args, size_t argc) {
  if (callee->kind != EXPR_MEMBER)
    return value_nil();
  Expr *obj = callee->as.member.obj;
  const char *field = callee->as.member.field;
  Value o = eval_expr(ip, obj);
  if (o.kind == V_LIST && strcmp(field, "map") == 0 && argc == 1) {
    Value fn = eval_expr(ip, args[0]);
    if (fn.kind != V_LAMBDA) {
      fprintf(stderr, "flow: map expects lambda\n");
      value_free_inner(&o);
      value_free_inner(&fn);
      return value_nil();
    }
    size_t n = o.as.list.len;
    Value *out = calloc(n, sizeof(Value));
    for (size_t i = 0; i < n; i++) {
      Value a = o.as.list.items[i];
      Value argarr[1];
      argarr[0] = value_clone_shallow(&a);
      out[i] = call_lambda(ip, &fn, argarr, 1);
      value_free_inner(&argarr[0]);
    }
    value_free_inner(&o);
    value_free_inner(&fn);
    return value_list(out, n);
  }
  value_free_inner(&o);
  (void)field;
  return value_nil();
}

static Value eval_expr(Interp *ip, Expr *e) {
  if (!e)
    return value_nil();
  switch (e->kind) {
  case EXPR_LITERAL:
    switch (e->as.lit.tag) {
    case LIT_NIL:
      return value_nil();
    case LIT_BOOL:
      return value_bool(e->as.lit.bval);
    case LIT_INT:
      return value_int((int64_t)e->as.lit.ival);
    case LIT_FLOAT:
      return value_float(e->as.lit.fval);
    case LIT_STRING:
      return value_string_copy(e->as.lit.sval);
    }
    break;
  case EXPR_VAR: {
    Value v;
    if (!env_get(ip->env, e->as.var.name, &v)) {
      fprintf(stderr, "flow: undefined '%s'\n", e->as.var.name);
      return value_nil();
    }
    return value_clone_shallow(&v);
  }
  case EXPR_BINARY: {
    Value L = eval_expr(ip, e->as.binary.left);
    Value R = eval_expr(ip, e->as.binary.right);
    int op = e->as.binary.op;
    Value out = value_nil();
    if (L.kind == V_INT && R.kind == V_INT) {
      int64_t a = L.as.i, b = R.as.i;
      switch (op) {
      case TOK_PLUS:
        out = value_int(a + b);
        break;
      case TOK_MINUS:
        out = value_int(a - b);
        break;
      case TOK_STAR:
        out = value_int(a * b);
        break;
      case TOK_SLASH:
        out = value_int(b == 0 ? 0 : a / b);
        break;
      case TOK_PERCENT:
        out = value_int(b == 0 ? 0 : a % b);
        break;
      case TOK_EQEQ:
        out = value_bool(a == b);
        break;
      case TOK_BANGEQ:
        out = value_bool(a != b);
        break;
      case TOK_LT:
        out = value_bool(a < b);
        break;
      case TOK_LTE:
        out = value_bool(a <= b);
        break;
      case TOK_GT:
        out = value_bool(a > b);
        break;
      case TOK_GTE:
        out = value_bool(a >= b);
        break;
      case TOK_ANDAND:
        out = value_bool(a && b);
        break;
      case TOK_OROR:
        out = value_bool(a || b);
        break;
      default:
        break;
      }
    } else {
      double x = L.kind == V_INT ? (double)L.as.i : L.as.f;
      double y = R.kind == V_INT ? (double)R.as.i : R.as.f;
      if (L.kind == V_FLOAT)
        x = L.as.f;
      if (R.kind == V_FLOAT)
        y = R.as.f;
      switch (op) {
      case TOK_PLUS:
        out = value_float(x + y);
        break;
      case TOK_MINUS:
        out = value_float(x - y);
        break;
      case TOK_STAR:
        out = value_float(x * y);
        break;
      case TOK_SLASH:
        out = value_float(x / y);
        break;
      default:
        break;
      }
    }
    value_free_inner(&L);
    value_free_inner(&R);
    return out;
  }
  case EXPR_UNARY: {
    Value R = eval_expr(ip, e->as.unary.right);
    if (e->as.unary.op == TOK_BANG) {
      int t = 0;
      if (R.kind == V_BOOL)
        t = !R.as.b;
      value_free_inner(&R);
      return value_bool(t);
    }
    if (e->as.unary.op == TOK_MINUS) {
      if (R.kind == V_INT) {
        int64_t v = -R.as.i;
        value_free_inner(&R);
        return value_int(v);
      }
      if (R.kind == V_FLOAT) {
        double v = -R.as.f;
        value_free_inner(&R);
        return value_float(v);
      }
    }
    value_free_inner(&R);
    return value_nil();
  }
  case EXPR_CALL: {
    Expr *cal = e->as.call.callee;
    if (cal->kind == EXPR_MEMBER) {
      Value v = eval_member_call(ip, cal, e->as.call.args, e->as.call.argc);
      return v;
    }
    Value fn = eval_expr(ip, cal);
    Value *args = calloc(e->as.call.argc, sizeof(Value));
    for (size_t i = 0; i < e->as.call.argc; i++)
      args[i] = eval_expr(ip, e->as.call.args[i]);
    Value ret = value_nil();
    if (fn.kind == V_NATIVE) {
      ret = fn.as.native.fn(ip, args, e->as.call.argc, fn.as.native.userdata);
    } else if (fn.kind == V_FUNC) {
      ret = call_user(ip, &fn, args, e->as.call.argc);
    } else if (fn.kind == V_LAMBDA) {
      ret = call_lambda(ip, &fn, args, e->as.call.argc);
    } else {
      fprintf(stderr, "flow: not callable\n");
    }
    value_free_inner(&fn);
    for (size_t i = 0; i < e->as.call.argc; i++)
      value_free_inner(&args[i]);
    free(args);
    return ret;
  }
  case EXPR_INDEX:
    /* optional */
    break;
  case EXPR_MEMBER:
    break;
  case EXPR_LAMBDA: {
    Value v;
    v.kind = V_LAMBDA;
    v.as.lambda.params = e->as.lambda.params;
    v.as.lambda.nparams = e->as.lambda.nparams;
    v.as.lambda.body = e->as.lambda.body;
    v.as.lambda.env = ip->env;
    return v;
  }
  case EXPR_MATCH: {
    Value s = eval_expr(ip, e->as.match.scrutinee);
    long long key = 0;
    if (s.kind == V_INT)
      key = s.as.i;
    value_free_inner(&s);
    for (size_t i = 0; i < e->as.match.narms; i++) {
      int ok = 0;
      if (e->as.match.pat_kinds[i] == 1)
        ok = 1;
      else if (e->as.match.pat_kinds[i] == 0 && key == e->as.match.pat_ints[i])
        ok = 1;
      if (ok)
        return eval_expr(ip, e->as.match.arm_exprs[i]);
    }
    return value_nil();
  }
  case EXPR_LIST_LIT: {
    size_t n = e->as.list_lit.nitems;
    Value *items = calloc(n, sizeof(Value));
    for (size_t i = 0; i < n; i++)
      items[i] = eval_expr(ip, e->as.list_lit.items[i]);
    return value_list(items, n);
  }
  default:
    break;
  }
  return value_nil();
}

void flow_exec_stmt(Interp *ip, Stmt *s) {
  if (!s)
    return;
  switch (s->kind) {
  case STMT_BLOCK:
    for (size_t i = 0; i < s->as.block.nstmts; i++) {
      flow_exec_stmt(ip, s->as.block.stmts[i]);
      if (ip->has_ret)
        return;
    }
    break;
  case STMT_LET: {
    Value v = eval_expr(ip, s->as.let.init);
    env_put(ip->env, s->as.let.name, v);
    break;
  }
  case STMT_EXPR: {
    Value v = eval_expr(ip, s->as.expr.expr);
    value_free_inner(&v);
    break;
  }
  case STMT_IF: {
    Value c = eval_expr(ip, s->as.if_s.cond);
    int truth = 0;
    if (c.kind == V_BOOL)
      truth = c.as.b ? 1 : 0;
    else if (c.kind == V_INT)
      truth = c.as.i != 0;
    value_free_inner(&c);
    if (truth)
      flow_exec_stmt(ip, s->as.if_s.then_stmt);
    else if (s->as.if_s.else_stmt)
      flow_exec_stmt(ip, s->as.if_s.else_stmt);
    break;
  }
  case STMT_WHILE: {
    for (;;) {
      Value c = eval_expr(ip, s->as.while_s.cond);
      int truth = 0;
      if (c.kind == V_BOOL)
        truth = c.as.b ? 1 : 0;
      else if (c.kind == V_INT)
        truth = c.as.i != 0;
      value_free_inner(&c);
      if (!truth)
        break;
      flow_exec_stmt(ip, s->as.while_s.body);
      if (ip->has_ret)
        return;
    }
    break;
  }
  case STMT_FOR: {
    Value itv = eval_expr(ip, s->as.for_s.iterable);
    if (itv.kind != V_LIST) {
      value_free_inner(&itv);
      break;
    }
    for (size_t i = 0; i < itv.as.list.len; i++) {
      env_put(ip->env, s->as.for_s.iter, value_clone_shallow(&itv.as.list.items[i]));
      flow_exec_stmt(ip, s->as.for_s.body);
      if (ip->has_ret) {
        value_free_inner(&itv);
        return;
      }
    }
    value_free_inner(&itv);
    break;
  }
  case STMT_RETURN: {
    if (s->as.ret.nexprs == 0)
      ip->ret = value_nil();
    else if (s->as.ret.nexprs == 1)
      ip->ret = eval_expr(ip, s->as.ret.exprs[0]);
    else {
      /* tuple not in Value — return first for now */
      ip->ret = eval_expr(ip, s->as.ret.exprs[0]);
    }
    ip->has_ret = 1;
    break;
  }
  case STMT_FUNC: {
    Value fv;
    fv.kind = V_FUNC;
    fv.as.func.params = s->as.func.params;
    fv.as.func.nparams = s->as.func.nparams;
    fv.as.func.body = s->as.func.body;
    fv.as.func.env = ip->env;
    env_put(ip->env, s->as.func.name, fv);
    break;
  }
  case STMT_LIBRARY:
    break;
  case STMT_IMPORT:
    exec_import(ip, s);
    break;
  case STMT_BREAK:
  case STMT_CONTINUE:
    break;
  default:
    break;
  }
}

static int validate_exactly_one_main(Program *prog, char **err_out) {
  size_t n = 0;
  Stmt *main_fn = NULL;
  for (size_t i = 0; i < prog->nitems; i++) {
    Stmt *s = prog->items[i];
    if (s->kind == STMT_FUNC && strcmp(s->as.func.name, "main") == 0) {
      n++;
      main_fn = s;
    }
  }
  if (n != 1) {
    if (err_out && !*err_out) {
      *err_out = strdup(n == 0 ? "expected exactly one function main() in each .flow file"
                               : "multiple main functions are not allowed");
    }
    return -1;
  }
  if (main_fn->as.func.nparams != 0) {
    if (err_out && !*err_out)
      *err_out = strdup("main must take no parameters");
    return -1;
  }
  return 0;
}

static int run_module_body(Interp *ip, Program *sub, int invoke_main, char **err_out) {
  if (validate_exactly_one_main(sub, err_out) != 0)
    return -1;

  for (size_t j = 0; j < sub->nitems; j++) {
    Stmt *st = sub->items[j];
    if (st->kind == STMT_IMPORT)
      flow_exec_stmt(ip, st);
  }
  for (size_t j = 0; j < sub->nitems; j++) {
    Stmt *st = sub->items[j];
    if (st->kind == STMT_IMPORT)
      continue;
    if (st->kind == STMT_EXPR) {
      fprintf(stderr,
              "flow: top-level expressions are not allowed; put code in main()\n");
      if (err_out && !*err_out)
        *err_out = strdup("top-level expression not allowed");
      return -1;
    }
    flow_exec_stmt(ip, st);
  }

  if (!invoke_main)
    return 0;

  Expr *call = expr_call(expr_var(strdup("main")), NULL, 0);
  Value v = eval_expr(ip, call);
  value_free_inner(&v);
  expr_free(call);
  return 0;
}

static int exec_import(Interp *ip, Stmt *s) {
  char resolved[8192];
  if (flow_resolve_import(ip->source_path, s->as.import_s.path, resolved,
                          sizeof resolved) != 0) {
    fprintf(stderr, "flow: cannot resolve import \"%s\"\n", s->as.import_s.path);
    if (ip->err_out && !*ip->err_out)
      *ip->err_out = strdup("import resolve failed");
    return -1;
  }
  if (!ip->loads) {
    fprintf(stderr, "flow: internal: missing load stack\n");
    return -1;
  }
  if (push_load(ip->loads, resolved, ip->err_out) != 0)
    return -1;

  size_t srclen = 0;
  char *src = flow_read_file(resolved, &srclen);
  if (!src) {
    fprintf(stderr, "flow: cannot read \"%s\"\n", resolved);
    pop_load(ip->loads);
    if (ip->err_out && !*ip->err_out)
      *ip->err_out = strdup("import read failed");
    return -1;
  }
  char *perr = NULL;
  Program *sub = flow_parse(src, srclen, &perr);
  free(src);
  if (!sub) {
    fprintf(stderr, "parse error in import: %s\n", perr ? perr : "?");
    free(perr);
    pop_load(ip->loads);
    if (ip->err_out && !*ip->err_out)
      *ip->err_out = strdup("import parse failed");
    return -1;
  }
  free(perr);

  Env *lib = env_new(NULL);
  install_builtins(lib);
  Interp subip;
  subip.env = lib;
  subip.has_ret = 0;
  subip.ret = value_nil();
  subip.source_path = resolved;
  subip.loads = ip->loads;
  subip.err_out = ip->err_out;

  if (run_module_body(&subip, sub, 0, ip->err_out) != 0) {
    pop_load(ip->loads);
    (void)lib;
    (void)sub;
    return -1;
  }

  for (size_t j = 0; j < sub->nitems; j++) {
    Stmt *ts = sub->items[j];
    if (ts->kind == STMT_FUNC && ts->as.func.exported) {
      Value v;
      if (env_get(lib, ts->as.func.name, &v)) {
        Value ign;
        if (env_get(ip->env, ts->as.func.name, &ign))
          fprintf(stderr, "flow: import overwrites \"%s\"\n", ts->as.func.name);
        env_put(ip->env, ts->as.func.name, value_clone_shallow(&v));
      }
    } else if (ts->kind == STMT_LET && ts->as.let.exported) {
      Value v;
      if (env_get(lib, ts->as.let.name, &v)) {
        Value ign;
        if (env_get(ip->env, ts->as.let.name, &ign))
          fprintf(stderr, "flow: import overwrites \"%s\"\n", ts->as.let.name);
        env_put(ip->env, ts->as.let.name, value_clone_shallow(&v));
      }
    }
  }

  /* Do not program_free(sub): V_FUNC values in ip->env still point at this AST. */
  (void)sub;
  (void)lib;
  pop_load(ip->loads);
  return 0;
}

int flow_interp_run(Program *prog, const char *entry_script_path, char **err_out) {
  LoadStack ls = {0};
  Interp ip;
  ip.env = env_new(NULL);
  ip.has_ret = 0;
  ip.ret = value_nil();
  ip.source_path = entry_script_path && entry_script_path[0] ? entry_script_path : ".";
  ip.loads = &ls;
  ip.err_out = err_out;
  if (err_out)
    *err_out = NULL;

  install_builtins(ip.env);

  if (run_module_body(&ip, prog, 1, err_out) != 0) {
    for (size_t i = 0; i < ls.len; i++)
      free(ls.paths[i]);
    free(ls.paths);
    env_free(ip.env);
    return 1;
  }

  for (size_t i = 0; i < ls.len; i++)
    free(ls.paths[i]);
  free(ls.paths);

  env_free(ip.env);
  return 0;
}
