#ifndef DRAGOON_FIELD_ART_H
#define DRAGOON_FIELD_ART_H

#include "submap_key.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct DragoonFieldArt DragoonFieldArt;

/* Load one explicitly selected PNG/JPEG/WebP below an app-contained root.
 * relative_path consists solely of nonempty ordinary path components; caller
 * supplies the app root and ensures it contains no untrusted symlinks. No
 * third-party pack naming or discovery is implied. layer -1 is background;
 * nonnegative layer selects that exact foreground. Images must cover the
 * entire native canvas at an equal, positive integral scale on both axes.
 * The layout must come from the currently verified field observation.
 * On failure returns NULL and writes a diagnostic when error_size is nonzero.
 */
DragoonFieldArt *dragoon_field_art_load(const char *root, const char *relative_path,
    const DragoonFieldLayout *layout, int layer, char *error, size_t error_size);
void dragoon_field_art_free(DragoonFieldArt *art);

/* Borrowed straight-alpha RGBA pixels, valid until art is freed. */
const uint8_t *dragoon_field_art_pixels(const DragoonFieldArt *art,
    uint32_t *width, uint32_t *height);

/* Map a piece index from the exact layout passed to load to a pixel crop.
 * Returns false for a different layer or invalid index. The caller must discard
 * this art on scene/key/chapter changes and only draw while the verified
 * runtime layout remains current. This does not install a renderer hook. */
bool dragoon_field_art_crop(const DragoonFieldArt *art, unsigned piece_index,
    DragoonFieldRect *crop);

/* Renderer-facing optional binding. Configure each selected layer explicitly;
 * root is a trusted app-contained directory and paths use the load contract
 * above. No pack naming convention is assumed. Only background layer -1 is
 * accepted by the experimental binding; foreground visibility and blending
 * are not yet established. Selection replaces that layer;
 * NULL relative_path removes it. All calls are on the emulator/render thread.
 * The host MUST invalidate the trace on scene exit, reset, disc change and
 * save-state restore, before any later draw. If those boundaries are not
 * observable, do not use this API. A matched transfer proves loading, not
 * visibility or ownership; callers must independently establish both. */
const DragoonFieldLayout *dragoon_field_trace_current(uint64_t *generation);
void dragoon_field_trace_invalidate(void);
bool dragoon_field_art_select(const char *root, DragoonSubmapKey key, int layer, const char *relative_path);
void dragoon_field_art_clear(void);
/* Diagnostic selection for a mounted <data>/disc/<image>; cut must be exactly
 * 675 or 676 and image must be a safe path relative to <data>. No I/O occurs
 * until a verified draw requests the image. */
bool dragoon_field_art_select_test(const char *disc_path, const char *cut, const char *image);
/* Report whether a selection matches the current observed layout; generations
 * are returned even when inactive so a renderer can retire stale resources. */
bool dragoon_field_art_active(uint64_t *scene_generation, uint64_t *selection_generation);

typedef struct {
    const uint8_t *rgba; /* borrowed until next selection, trace invalidation or query */
    uint32_t width, height;
    DragoonFieldRect crop;
    uint64_t scene_generation, selection_generation;
} DragoonFieldArtDraw;

/* Exact, unambiguous native primitive only. False means draw the original.
 * Copy/upload pixels before the next call; GPU resource lifetime belongs to
 * the renderer, including in-flight command buffers. Re-query on each draw. */
bool dragoon_field_art_for_draw(uint16_t texture_page, uint16_t clut_x,
    uint16_t clut_y, uint32_t texture_window, const DragoonFieldRect *uv,
    DragoonFieldArtDraw *draw);

#ifdef __cplusplus
}
#endif
#endif

