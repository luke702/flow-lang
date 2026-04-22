#include "flow_value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void value_free_inner(Value *v) {
  if (!v)
    return;
  switch (v->kind) {
  case V_STRING:
    free(v->as.s);
    break;
  case V_LIST:
    for (size_t i = 0; i < v->as.list.len; i++)
      value_destroy(&v->as.list.items[i]);
    free(v->as.list.items);
    break;
  case V_FUNC:
  case V_NATIVE:
  case V_LAMBDA:
    break;
  default:
    break;
  }
}

void value_destroy(Value *v) {
  value_free_inner(v);
  v->kind = V_NIL;
}

Value value_nil(void) {
  Value v;
  v.kind = V_NIL;
  return v;
}

Value value_bool(bool b) {
  Value v;
  v.kind = V_BOOL;
  v.as.b = b;
  return v;
}

Value value_int(int64_t i) {
  Value v;
  v.kind = V_INT;
  v.as.i = i;
  return v;
}

Value value_float(double f) {
  Value v;
  v.kind = V_FLOAT;
  v.as.f = f;
  return v;
}

Value value_string_owned(char *s) {
  Value v;
  v.kind = V_STRING;
  v.as.s = s;
  return v;
}

Value value_string_copy(const char *s) {
  return value_string_owned(strdup(s));
}

Value value_list(Value *items, size_t len) {
  Value v;
  v.kind = V_LIST;
  v.as.list.items = items;
  v.as.list.len = len;
  return v;
}

void value_print(const Value *v) {
  switch (v->kind) {
  case V_NIL:
    printf("nil");
    break;
  case V_BOOL:
    printf("%s", v->as.b ? "true" : "false");
    break;
  case V_INT:
    printf("%lld", (long long)v->as.i);
    break;
  case V_FLOAT:
    printf("%g", v->as.f);
    break;
  case V_STRING:
    printf("%s", v->as.s ? v->as.s : "");
    break;
  case V_LIST: {
    printf("[");
    for (size_t i = 0; i < v->as.list.len; i++) {
      if (i)
        printf(", ");
      value_print(&v->as.list.items[i]);
    }
    printf("]");
    break;
  }
  case V_FUNC:
    printf("<func>");
    break;
  case V_NATIVE:
    printf("<native>");
    break;
  case V_LAMBDA:
    printf("<lambda>");
    break;
  }
}
