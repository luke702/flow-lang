#ifndef FLOW_INTERP_H
#define FLOW_INTERP_H

#include "flow_ast.h"
#include <stddef.h>

/*
 * entry_script_path: absolute or normalized path to the running script (used to
 * resolve relative imports). May be NULL (treated as ".").
 */
int flow_interp_run(Program *prog, const char *entry_script_path, char **err_out);

#endif
