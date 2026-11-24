#ifndef LOD_DISC_RESOLVER_H
#define LOD_DISC_RESOLVER_H
#include <stddef.h>
typedef void (*lod_attempt_log)(const char*, const char*, int, void*);
int lod_find_disc(const char *app, const char *data, const char *const usb[8], const char *explicit_dir, char *out, size_t out_size, lod_attempt_log log, void *ctx);
int lod_validate_explicit(const char*, int, char*, size_t, lod_attempt_log, void*);
#endif
