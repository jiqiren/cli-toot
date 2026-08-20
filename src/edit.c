#include "edit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Quote `s` for a /bin/sh command line using single quotes. Returns a fresh
 * allocation. */
static char *shell_quote(const char *s) {
  size_t n = strlen(s) * 2 + 3;
  char *out = malloc(n);
  if (out == nullptr) return nullptr;
  char *p = out;
  *p++ = '\'';
  for (const char *c = s; *c != '\0'; c++) {
    if (*c == '\'') {
      *p++ = '\'';
      *p++ = '\\';
      *p++ = '\'';
      *p++ = '\'';
    } else {
      *p++ = *c;
    }
  }
  *p++ = '\'';
  *p = '\0';
  return out;
}

static char *read_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == nullptr) return strdup("");
  long size;
  if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
      fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return strdup("");
  }
  char *buf = malloc((size_t)size + 1);
  if (buf == nullptr) {
    fclose(f);
    return nullptr;
  }
  size_t got = fread(buf, 1, (size_t)size, f);
  buf[got] = '\0';
  fclose(f);
  return buf;
}

static bool is_blank(const char *s) {
  for (const char *c = s; *c != '\0'; c++) {
    if (*c != ' ' && *c != '\t' && *c != '\n' && *c != '\r') return false;
  }
  return true;
}

int edit_open(editor_result *out) {
  out->text = nullptr;
  out->cancelled = false;

  const char *editor = getenv("EDITOR");
  if (editor == nullptr || editor[0] == '\0') editor = getenv("VISUAL");
  const char *fallback = "vi";
  if (editor == nullptr || editor[0] == '\0') editor = fallback;

  const char *tmp = getenv("TMPDIR");
  if (tmp == nullptr || tmp[0] == '\0') tmp = "/tmp";
  size_t n = strlen(tmp) + 32;
  char *path = malloc(n);
  if (path == nullptr) return -1;
  snprintf(path, n, "%s/sloptoot-XXXXXX", tmp);

  int fd = mkstemp(path);
  if (fd < 0) {
    free(path);
    return -1;
  }
  close(fd);

  char *quoted = shell_quote(path);
  if (quoted == nullptr) {
    unlink(path);
    free(path);
    return -1;
  }
  size_t m = strlen(editor) + strlen(quoted) + 2;
  char *cmd = malloc(m);
  if (cmd == nullptr) {
    free(quoted);
    unlink(path);
    free(path);
    return -1;
  }
  snprintf(cmd, m, "%s %s", editor, quoted);
  free(quoted);

  /* Run the editor synchronously; the spacing / flags inside EDITOR are up to
   * the user (e.g. "code --wait"). */
  int rc = system(cmd);
  free(cmd);

  if (rc != 0) {
    unlink(path);
    free(path);
    return -1;
  }

  char *text = read_file(path);
  unlink(path);
  free(path);
  if (text == nullptr) return -1;

  if (is_blank(text)) {
    free(text);
    out->cancelled = true;
    return 0;
  }

  out->text = text;
  return 0;
}