#pragma once

#include <stdbool.h>

typedef struct {
  char *text;      /* content read back from the editor, or nullptr */
  bool cancelled;  /* true when the buffer was left empty */
} editor_result;

/* Open $EDITOR (falls back to VISUAL, then vi) on a temp file. Returns 0 on
 * success (cancelled may be set), or nonzero if the editor could not be run.
 * On success, `out->text` is a fresh allocation the caller frees. */
int edit_open(editor_result *out);