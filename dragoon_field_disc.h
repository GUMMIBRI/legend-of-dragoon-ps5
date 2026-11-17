#ifndef DRAGOON_FIELD_DISC_H
#define DRAGOON_FIELD_DISC_H

#include "submap_key.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { DRAGOON_FIELD_SECTOR_SIZE = 2048 };

typedef struct {
    uint32_t lba;
    uint32_t size;
} DragoonFieldDiscExtent;

typedef struct {
    void *context;
    uint32_t sector_count;
    /* Look up a contiguous ISO file. Paths are relative to the volume root. */
    bool (*find_file)(void *context, const char *path, DragoonFieldDiscExtent *extent);
    /* Read one cooked data sector without changing guest CD controller state. */
    bool (*read_sector)(void *context, uint32_t lba,
                        uint8_t data[DRAGOON_FIELD_SECTOR_SIZE]);
} DragoonFieldDisc;

/* Resolve the selected bundle's physical first sector and byte length. */
bool dragoon_field_disc_bundle(const DragoonFieldDisc *disc, DragoonSubmapKey key,
                               uint32_t chapter, uint8_t *scratch, size_t capacity,
                               DragoonFieldDiscExtent *bundle);

/* Call with the verified US Disc 1 and values sampled at the confirmed field
 * asset-load event. Only fields selected from its DRGN21 archive are supported.
 * Scratch must accommodate the field bundle and is overwritten. False leaves
 * layout unchanged. The returned layout does not borrow scratch or disc data.
 */
bool dragoon_field_disc_layout(const DragoonFieldDisc *disc, DragoonSubmapKey key,
                               uint32_t chapter, uint8_t *scratch, size_t capacity,
                               DragoonFieldLayout *layout);

#ifdef __cplusplus
}
#endif

#endif
