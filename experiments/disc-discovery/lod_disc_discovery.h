#ifndef LOD_DISC_DISCOVERY_H
#define LOD_DISC_DISCOVERY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 1 match, 0 no match, -1 IO/invalid, -2 multiple matches, -3 inaccessible directory. */
int lod_disc_find_in_directory(const char*,char*,size_t,char*,size_t);
#ifdef __cplusplus
}
#endif
#endif
