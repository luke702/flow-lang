#include "flow_path.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <limits.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

char *flow_read_file(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  char *buf = malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t n = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[n] = '\0';
  if (out_len)
    *out_len = n;
  return buf;
}

static int is_abs_path(const char *p) {
  if (!p || !p[0])
    return 0;
#if defined(_WIN32)
  if (p[0] == '/' || p[0] == '\\')
    return 1;
  if (isalpha((unsigned char)p[0]) && p[1] == ':' &&
      (p[2] == '/' || p[2] == '\\'))
    return 1;
  return 0;
#else
  return p[0] == '/';
#endif
}

int flow_path_absolute(const char *path, char *out, size_t outsz) {
  if (!path || !out || outsz < 2)
    return -1;
#if defined(_WIN32)
  {
    DWORD n = GetFullPathNameA(path, (DWORD)outsz, out, NULL);
    if (n == 0 || n >= (DWORD)outsz)
      return -1;
    return 0;
  }
#else
  {
    char *rp = realpath(path, NULL);
    if (rp) {
      if (strlen(rp) >= outsz) {
        free(rp);
        return -1;
      }
      memcpy(out, rp, strlen(rp) + 1);
      free(rp);
      return 0;
    }
    /* realpath may fail if path does not exist yet — fall back to cwd join */
    char cwd[4096];
    if (!getcwd(cwd, sizeof cwd))
      return -1;
    if (is_abs_path(path)) {
      if (strlen(path) >= outsz)
        return -1;
      memcpy(out, path, strlen(path) + 1);
      return 0;
    }
    char tmp[8192];
    if (snprintf(tmp, sizeof tmp, "%s/%s", cwd, path) >= (int)sizeof tmp)
      return -1;
    rp = realpath(tmp, NULL);
    if (rp) {
      if (strlen(rp) >= outsz) {
        free(rp);
        return -1;
      }
      memcpy(out, rp, strlen(rp) + 1);
      free(rp);
      return 0;
    }
    if (strlen(tmp) >= outsz)
      return -1;
    memcpy(out, tmp, strlen(tmp) + 1);
    return 0;
  }
#endif
}

int flow_path_dirname(const char *path, char *out, size_t outsz) {
  if (!path || !out || outsz < 2)
    return -1;
  char abs[4096];
  if (flow_path_absolute(path, abs, sizeof abs) != 0)
    return -1;
  char tmp[4096];
  memcpy(tmp, abs, strlen(abs) + 1);
  char *slash = strrchr(tmp, '/');
#if defined(_WIN32)
  {
    char *bs = strrchr(tmp, '\\');
    if (!slash || (bs && bs > slash))
      slash = bs;
  }
#endif
  if (slash)
    *slash = '\0';
  else {
    tmp[0] = '.';
    tmp[1] = '\0';
  }
  if (strlen(tmp) >= outsz)
    return -1;
  memcpy(out, tmp, strlen(tmp) + 1);
  return 0;
}

int flow_path_join(const char *a, const char *b, char *out, size_t outsz) {
  if (!a || !b || !out)
    return -1;
  size_t la = strlen(a), lb = strlen(b);
  int need_sep = (la > 0 && a[la - 1] != '/' && a[la - 1] != '\\');
  size_t need = la + lb + (need_sep ? 1 : 0) + 1;
  if (need > outsz)
    return -1;
  memcpy(out, a, la);
  size_t pos = la;
  if (need_sep)
    out[pos++] = PATH_SEP;
  memcpy(out + pos, b, lb + 1);
  return 0;
}

static int file_readable(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  fclose(f);
  return 1;
}

int flow_resolve_import(const char *script_path, const char *import_rel, char *out,
                        size_t outsz) {
  if (!script_path || !import_rel || !out || outsz < 8)
    return -1;

  if (is_abs_path(import_rel)) {
    if (!file_readable(import_rel))
      return -1;
    return flow_path_absolute(import_rel, out, outsz);
  }

  char script_abs[4096];
  if (flow_path_absolute(script_path, script_abs, sizeof script_abs) != 0)
    return -1;
  char dir[4096];
  if (flow_path_dirname(script_abs, dir, sizeof dir) != 0)
    return -1;

  char cand[8192];
  if (flow_path_join(dir, import_rel, cand, sizeof cand) != 0)
    return -1;
  if (file_readable(cand))
    return flow_path_absolute(cand, out, outsz);

  const char *fp = getenv("FLOW_PATH");
  if (!fp)
    return -1;

  char buf[8192];
  memcpy(buf, fp, strlen(fp) + 1);
  for (char *p = buf;;) {
    char *q = p;
#if defined(_WIN32)
    char *semi = strchr(q, ';');
#else
    char *semi = strchr(q, ':');
#endif
    if (semi)
      *semi = 0;
    while (*q == ' ' || *q == '\t')
      q++;
    size_t L = strlen(q);
    while (L > 0 && (q[L - 1] == ' ' || q[L - 1] == '\t'))
      q[--L] = 0;
    if (L > 0) {
      if (flow_path_join(q, import_rel, cand, sizeof cand) == 0 &&
          file_readable(cand))
        return flow_path_absolute(cand, out, outsz);
    }
    if (!semi)
      break;
    p = semi + 1;
  }
  return -1;
}

void flow_print_lib_paths(FILE *fp) {
  const char *fpth = getenv("FLOW_PATH");
  fprintf(fp, "FLOW_PATH is %s\n", fpth ? "set" : "not set");
  if (fpth)
    fprintf(fp, "%s\n", fpth);
  fprintf(fp,
          "Imports resolve relative to the current script directory first, "
          "then each FLOW_PATH entry (joined with the import path).\n");
}
