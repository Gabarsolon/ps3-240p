#ifndef BOOT_PLUGINS_H
#define BOOT_PLUGINS_H
#include <stddef.h>
/* Line editing for /dev_hdd0/boot_plugins.txt. Pure functions on a caller
 * buffer so the logic is host-testable. A line matches when its text, with
 * surrounding spaces/tabs/CR removed, equals the plugin path. */
int bp_find(const char *text,size_t len,const char *path);
/* Append the path as its own line (no-op if present). Returns the new length,
 * or 0 when the buffer has no room. Keeps the file's CRLF/LF style. */
size_t bp_add(char *text,size_t len,size_t capacity,const char *path);
/* Remove every line equal to the path. Returns the new length. */
size_t bp_remove(char *text,size_t len,const char *path);
#endif
