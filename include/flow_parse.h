#ifndef FLOW_PARSE_H
#define FLOW_PARSE_H

#include "flow_ast.h"
#include <stddef.h>

Program *flow_parse(const char *src, size_t len, char **err_out);

#endif
