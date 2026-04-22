#include "flow_interp.h"
#include "flow_parse.h"
#include "flow_ast.h"
#include "flow_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(strdup)
#define strdup _strdup
#endif

static char *read_file(const char *path, size_t *out_len) {
  return flow_read_file(path, out_len);
}

static void usage(FILE *fp) {
  fprintf(fp,
          "usage:\n"
          "  flow <script.flow>   Run a script\n"
          "  flow libs              Show FLOW_PATH and import search rules\n"
          "  flow --help            Show this help\n");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(stderr);
    return 1;
  }
  if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    usage(stdout);
    return 0;
  }
  if (strcmp(argv[1], "libs") == 0) {
    flow_print_lib_paths(stdout);
    return 0;
  }

  const char *path = argv[1];

  size_t len = 0;
  char *src = read_file(path, &len);
  if (!src)
    return 1;

  char *perr = NULL;
  Program *prog = flow_parse(src, len, &perr);
  free(src);
  if (!prog) {
    fprintf(stderr, "parse error: %s\n", perr ? perr : "unknown");
    free(perr);
    return 1;
  }
  free(perr);

  char absbuf[8192];
  const char *runpath = path;
  if (flow_path_absolute(path, absbuf, sizeof absbuf) == 0)
    runpath = absbuf;

  char *ierr = NULL;
  int rc = flow_interp_run(prog, runpath, &ierr);
  if (ierr) {
    fprintf(stderr, "runtime: %s\n", ierr);
    free(ierr);
  }
  program_free(prog);
  return rc;
}
