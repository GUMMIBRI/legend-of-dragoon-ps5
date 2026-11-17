#ifndef DRAGOON_SUBMAP_KEY_H
#define DRAGOON_SUBMAP_KEY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t archive;
    uint32_t cut;
} DragoonSubmapKey;

/* Reads a little-endian guest word. False means the address is unavailable. */
typedef bool (*DragoonGuestRead)(void *context, uint32_t address, uint32_t *word);

/* Sample only at the field asset-load boundary, when both fields are stable.
 * The archive index is the pack event's disk key, not the mounted CD number.
 */
bool dragoon_submap_key(DragoonGuestRead read, void *context, DragoonSubmapKey *key);

typedef struct {
    DragoonSubmapKey key;
    uint32_t environment_entry;
} DragoonSubmapLocation;

/* Resolve a cut through the original SUBMAP/NEWROOT.RDT directory.
 * active_archive uses the game's one-based DRGN2 archive numbering.
 */
bool dragoon_submap_locate(const uint8_t *table, size_t size, uint32_t cut,
                          uint32_t active_archive, uint32_t chapter,
                          DragoonSubmapLocation *location);

typedef struct {
    uint64_t offset;
    uint32_t size;
} DragoonArchiveSpan;

/* Resolve a nonempty member of a sector-aligned DRGN2 archive. */
bool dragoon_environment_span(const uint8_t *directory, size_t directory_size,
                              uint64_t archive_size, uint32_t entry,
                              DragoonArchiveSpan *span);

typedef struct {
    const uint8_t *bundle;
    size_t bundle_size;
    const uint8_t *records;
    uint8_t count;
    uint8_t background_count;
    uint8_t foreground_count;
} DragoonEnvironment;

/* Open the environment directory in a loaded DRGN2 field bundle. The
 * returned view borrows the bundle bytes for the lifetime of the field.
 */
bool dragoon_environment_open(const uint8_t *bundle, size_t size,
                              DragoonEnvironment *environment);

typedef struct {
    int32_t world_x, world_y, world_z;
    int32_t u, v;
    /* Placement in the original field, measured from the authored origin. */
    int32_t texture_x, texture_y;
    uint16_t width, height, texture_page;
    int32_t palette_y;
    bool foreground;
} DragoonEnvironmentTile;

bool dragoon_environment_tile(const DragoonEnvironment *environment, unsigned index,
                              DragoonEnvironmentTile *tile);

/* Find the original TIM whose image rectangle contains the tile page origin.
 * The returned span is relative to the field bundle, in bytes.
 */
bool dragoon_environment_texture(const DragoonEnvironment *environment,
                                 unsigned index, DragoonArchiveSpan *texture);

typedef struct {
    int32_t x, y;
    uint32_t width, height;
} DragoonFieldRect;

enum { DRAGOON_MAX_FIELD_PIECES = 32 };

typedef struct {
    DragoonEnvironmentTile tile;
    DragoonFieldRect art;
    DragoonArchiveSpan original_tim;
    /* -1 selects the stitched background, otherwise the zero-based cutout. */
    int foreground;
    uint16_t clut_y;
} DragoonFieldPiece;

typedef struct {
    DragoonSubmapKey key;
    /* All SC environment layers share these native canvas dimensions. */
    DragoonFieldRect canvas;
    unsigned count;
    DragoonFieldPiece pieces[DRAGOON_MAX_FIELD_PIECES];
} DragoonFieldLayout;

/* Build a scene's authored layer placement while the original bundle is
 * available. Piece art rectangles are relative to the resulting canvas.
 * The layout is independent of the bundle's lifetime. */
bool dragoon_field_layout(const DragoonEnvironment *environment, DragoonSubmapKey key,
                          DragoonFieldLayout *layout);

/* Match a complete field primitive during that scene's lifetime. x/y are
 * intentionally absent: the original packet already follows camera and script
 * motion. uv describes the native sample rectangle, independent of the
 * destination size. Absent, partial or ambiguous matches retain the original
 * draw. */
const DragoonFieldPiece *dragoon_field_piece_for_draw(
    const DragoonFieldLayout *layout, uint16_t texture_page, uint16_t clut_x,
    uint16_t clut_y, uint32_t texture_window, const DragoonFieldRect *uv);

#ifdef __cplusplus
}
#endif

#endif
