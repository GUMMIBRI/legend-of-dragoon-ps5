#ifndef LOD_DISC_PROBE_H
#define LOD_DISC_PROBE_H
#ifdef __cplusplus
extern "C" {
#endif
enum LodDiscProbeResult { LOD_DISC_VALID=1, LOD_DISC_WRONG=0, LOD_DISC_IO=-1 };
int lod_disc_probe(const char *path, char *error, unsigned error_size);
#ifdef __cplusplus
}
#endif
#endif
