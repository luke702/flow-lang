#ifndef FLOW_PATH_H
#define FLOW_PATH_H

#include <stddef.h>
#include <stdio.h>

/* Read entire file (NUL-terminated). Caller frees. */
char *flow_read_file(const char *path, size_t *out_len);

/* Normalize to absolute path (best effort). Returns 0 on success. */
int flow_path_absolute(const char *path, char *out, size_t outsz);

/* Parent directory of path into out. Returns 0 on success. */
int flow_path_dirname(const char *path, char *out, size_t outsz);

/* Join a and b with one separator into out. Returns 0 on success. */
int flow_path_join(const char *a, const char *b, char *out, size_t outsz);

/*
 * Resolve import path: relative imports are from script_path's directory;
 * absolute paths accepted. Then searches FLOW_PATH (Windows: ';', else ':').
 */
int flow_resolve_import(const char *script_path, const char *import_rel, char *out,
                        size_t outsz);

/* List directories searched after the script directory (FLOW_PATH + notes). */
void flow_print_lib_paths(FILE *fp);

#endif
