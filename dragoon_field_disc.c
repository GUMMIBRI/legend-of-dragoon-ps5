#include "dragoon_field_disc.h"

#include <string.h>

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool find_extent(const DragoonFieldDisc *disc, const char *path,
                        DragoonFieldDiscExtent *extent)
{
    DragoonFieldDiscExtent found = {0};
    if (!disc->find_file(disc->context, path, &found) || !found.size ||
        found.lba >= disc->sector_count ||
        (found.size - 1) / DRAGOON_FIELD_SECTOR_SIZE >= disc->sector_count - found.lba)
        return false;

    *extent = found;
    return true;
}

/* The archive directory and each of its member starts are sector-aligned. */
static bool read_extent(const DragoonFieldDisc *disc, DragoonFieldDiscExtent extent,
                        uint64_t offset, size_t length, uint8_t *data)
{
    if (offset % DRAGOON_FIELD_SECTOR_SIZE || offset > extent.size ||
        length > extent.size - offset)
        return false;

    uint32_t lba = extent.lba + (uint32_t)(offset / DRAGOON_FIELD_SECTOR_SIZE);
    while (length >= DRAGOON_FIELD_SECTOR_SIZE) {
        if (!disc->read_sector(disc->context, lba++, data))
            return false;
        data += DRAGOON_FIELD_SECTOR_SIZE;
        length -= DRAGOON_FIELD_SECTOR_SIZE;
    }
    if (length) {
        uint8_t tail[DRAGOON_FIELD_SECTOR_SIZE];
        if (!disc->read_sector(disc->context, lba, tail))
            return false;
        memcpy(data, tail, length);
    }
    return true;
}

bool dragoon_field_disc_bundle(const DragoonFieldDisc *disc, DragoonSubmapKey key,
                               uint32_t chapter, uint8_t *scratch, size_t capacity,
                               DragoonFieldDiscExtent *result)
{
    if (!disc || !disc->find_file || !disc->read_sector || !scratch || !result ||
        key.cut >= 0x400 || capacity < DRAGOON_FIELD_SECTOR_SIZE)
        return false;

    DragoonFieldDiscExtent newroot, archive;
    DragoonSubmapLocation location;
    size_t selection_bytes = (key.cut + 1) * 8u;
    if (capacity < selection_bytes ||
        !find_extent(disc, "SUBMAP/NEWROOT.RDT", &newroot) ||
        !read_extent(disc, newroot, 0, selection_bytes, scratch) ||
        !dragoon_submap_locate(scratch, selection_bytes, key.cut, key.archive,
                               chapter, &location) || location.key.archive != 1 ||
        !find_extent(disc, "SECT/DRGN21.BIN", &archive))
        return false;

    size_t first_bytes = archive.size < DRAGOON_FIELD_SECTOR_SIZE
                       ? archive.size : DRAGOON_FIELD_SECTOR_SIZE;
    if (first_bytes < 8 || !read_extent(disc, archive, 0, first_bytes, scratch) ||
        read_le32(scratch) != UINT32_C(0x1a47524d))
        return false;

    uint32_t count = read_le32(scratch + 4);
    if (location.environment_entry >= count || count > (archive.size - 8) / 8 ||
        count > (capacity - 8) / 8)
        return false;

    size_t directory_bytes = 8 + (size_t)count * 8;
    if (directory_bytes > first_bytes &&
        !read_extent(disc, archive, first_bytes, directory_bytes - first_bytes,
                     scratch + first_bytes))
        return false;

    DragoonArchiveSpan span;
    if (!dragoon_environment_span(scratch, directory_bytes, archive.size,
                                  location.environment_entry, &span))
        return false;
    *result = (DragoonFieldDiscExtent){
        .lba = archive.lba + (uint32_t)(span.offset / DRAGOON_FIELD_SECTOR_SIZE),
        .size = span.size,
    };
    return true;
}

bool dragoon_field_disc_layout(const DragoonFieldDisc *disc, DragoonSubmapKey key,
                               uint32_t chapter, uint8_t *scratch, size_t capacity,
                               DragoonFieldLayout *layout)
{
    DragoonFieldDiscExtent bundle;
    DragoonEnvironment environment;
    if (!layout || !dragoon_field_disc_bundle(disc, key, chapter, scratch, capacity,
                                               &bundle) || bundle.size > capacity ||
        !read_extent(disc, bundle, 0, bundle.size, scratch) ||
        !dragoon_environment_open(scratch, bundle.size, &environment))
        return false;
    return dragoon_field_layout(&environment, key, layout);
}
