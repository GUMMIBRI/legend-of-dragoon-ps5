#include "submap_key.h"

/* Candidate US retail addresses from the Severed Chains event inputs.
 * Verify their values against the native field-load hook before enabling art.
 */
bool dragoon_submap_key(DragoonGuestRead read, void *context, DragoonSubmapKey *key)
{
    DragoonSubmapKey result;
    if (!read || !key || !read(context, UINT32_C(0x800bc058), &result.archive) ||
        !read(context, UINT32_C(0x80052c30), &result.cut) ||
        !result.archive || result.archive > 8 || result.cut >= 0x400)
        return false;

    *key = result;
    return true;
}

static uint32_t read_le16(const uint8_t *bytes)
{
    return bytes[0] | ((uint32_t)bytes[1] << 8);
}

/* Retail directory layout and selection rules documented by the Severed
 * Chains NewRootStruct and SubmapCutInfo at 0c08c679.
 */
bool dragoon_submap_locate(const uint8_t *table, size_t size, uint32_t cut,
                          uint32_t active_archive, uint32_t chapter,
                          DragoonSubmapLocation *location)
{
    if (!table || !location || cut >= 0x400 || size < (cut + 1) * 8 ||
        active_archive == 0 || active_archive > 8)
        return false;

    const uint8_t *entry = table + cut * 8;
    uint32_t early = read_le16(entry);
    uint32_t late = read_le16(entry + 2);
    uint32_t selected = (early >> 13) + 1 == active_archive || (late >> 13) > chapter
                        ? early : late;
    uint32_t archive = selected >> 13;
    *location = (DragoonSubmapLocation){
        .key = { .archive = archive + 1, .cut = cut },
        .environment_entry = (selected & 0x1fff) * 3 + (archive < 4 ? 4 : 0),
    };
    return true;
}

static uint32_t read_le32(const uint8_t *bytes)
{
    return read_le16(bytes) | (read_le16(bytes + 2) << 16);
}

static bool archive_span(const uint8_t *directory, size_t directory_size,
                         uint64_t archive_size, uint32_t entry,
                         uint32_t unit, DragoonArchiveSpan *span)
{
    if (!directory || !span || directory_size < 8 ||
        read_le32(directory) != UINT32_C(0x1a47524d))
        return false;

    uint32_t count = read_le32(directory + 4);
    if (count > (directory_size - 8) / 8 || entry >= count)
        return false;

    const uint8_t *record = directory + 8 + (size_t)entry * 8;
    uint64_t offset = (uint64_t)read_le32(record) * unit;
    uint32_t size = read_le32(record + 4);
    if (!size || offset < 8 + (uint64_t)count * 8 ||
        offset > archive_size || size > archive_size - offset)
        return false;

    *span = (DragoonArchiveSpan){ .offset = offset, .size = size };
    return true;
}

bool dragoon_environment_span(const uint8_t *directory, size_t directory_size,
                              uint64_t archive_size, uint32_t entry,
                              DragoonArchiveSpan *span)
{
    return archive_span(directory, directory_size, archive_size, entry, 2048, span);
}

bool dragoon_environment_open(const uint8_t *bundle, size_t size,
                              DragoonEnvironment *environment)
{
    DragoonArchiveSpan metadata;
    if (!environment || !archive_span(bundle, size, size, 0, 1, &metadata) ||
        metadata.size < 0x18)
        return false;

    const uint8_t *header = bundle + metadata.offset;
    unsigned count = header[0x14];
    unsigned backgrounds = header[0x15];
    unsigned foregrounds = header[0x16];
    if (backgrounds + foregrounds > count ||
        count > (metadata.size - 0x18) / 0x24)
        return false;

    *environment = (DragoonEnvironment){
        .bundle = bundle,
        .bundle_size = size,
        .records = header + 0x18,
        .count = (uint8_t)count,
        .background_count = (uint8_t)backgrounds,
        .foreground_count = (uint8_t)foregrounds,
    };
    return true;
}

/* TIM page matching follows RetailSubmap.prepareEnv at 0c08c679.
 * The first three bundle entries are field metadata and collision data.
 */
bool dragoon_environment_texture(const DragoonEnvironment *environment,
                                 unsigned index, DragoonArchiveSpan *texture)
{
    DragoonEnvironmentTile tile;
    if (!texture || !dragoon_environment_tile(environment, index, &tile) ||
        !environment->bundle || environment->bundle_size < 8)
        return false;

    uint32_t page_x = (tile.texture_page & 15u) * 64;
    uint32_t page_y = (tile.texture_page & 16u) ? 256 : 0;
    const uint8_t *bundle = environment->bundle;
    size_t bundle_size = environment->bundle_size;
    uint32_t count = read_le32(bundle + 4);
    if (count > (bundle_size - 8) / 8)
        return false;

    for (uint32_t entry = 3; entry < count; ++entry) {
        DragoonArchiveSpan member;
        if (!archive_span(bundle, bundle_size, bundle_size, entry, 1, &member) ||
            member.size < 20)
            continue;

        const uint8_t *tim = bundle + member.offset;
        if (read_le32(tim) != 0x10)
            continue;

        uint32_t image = 8;
        if (read_le32(tim + 4) & 8) {
            uint32_t palette_size = read_le32(tim + image);
            if (palette_size < 12 || palette_size > member.size - image - 12)
                continue;
            image += palette_size;
        }

        const uint8_t *header = tim + image;
        uint32_t block_size = read_le32(header);
        uint32_t x = read_le16(header + 4);
        uint32_t y = read_le16(header + 6);
        uint32_t width = read_le16(header + 8);
        uint32_t height = read_le16(header + 10);
        if (block_size < 12 || block_size > member.size - image ||
            (uint64_t)width * height * 2 != block_size - 12)
            continue;

        if (page_x >= x && page_x < x + width && page_y >= y && page_y < y + height) {
            *texture = member;
            return true;
        }
    }
    return false;
}

static int32_t read_signed_le16(const uint8_t *bytes)
{
    int32_t value = (int32_t)read_le16(bytes);
    return value < 0x8000 ? value : value - 0x10000;
}

bool dragoon_environment_tile(const DragoonEnvironment *environment, unsigned index,
                              DragoonEnvironmentTile *tile)
{
    if (!environment || !environment->records || !tile ||
        index >= environment->count)
        return false;

    const uint8_t *record = environment->records + index * 0x24;
    *tile = (DragoonEnvironmentTile){
        .world_x = read_signed_le16(record),
        .world_y = read_signed_le16(record + 2),
        .world_z = read_signed_le16(record + 4),
        .u = read_signed_le16(record + 8),
        .v = read_signed_le16(record + 0x0a),
        .texture_x = read_signed_le16(record + 0x10),
        .texture_y = read_signed_le16(record + 0x12),
        .width = (uint16_t)read_le16(record + 0x0c),
        .height = (uint16_t)read_le16(record + 0x0e),
        .texture_page = (uint16_t)read_le16(record + 0x20),
        .palette_y = read_signed_le16(record + 0x22),
        .foreground = index >= environment->background_count,
    };
    return true;
}

/* RetailSubmap.prepareEnv uses the bounds of every TIM-backed field rectangle
 * as the shared canvas for the background and every foreground. Use its packet
 * alignment corrections for the art crop while retaining the retail draw key.
 */
bool dragoon_field_layout(const DragoonEnvironment *environment, DragoonSubmapKey key,
                          DragoonFieldLayout *layout)
{
    if (!environment || !layout || !key.archive || key.archive > 8 || key.cut >= 0x400 ||
        environment->count > DRAGOON_MAX_FIELD_PIECES ||
        environment->background_count + environment->foreground_count > environment->count)
        return false;

    DragoonFieldLayout result = { .key = key };
    unsigned rendered = environment->background_count + environment->foreground_count;
    int32_t left = INT32_MAX, top = INT32_MAX, right = INT32_MIN, bottom = INT32_MIN;
    for (unsigned i = 0; i < environment->count; ++i) {
        DragoonFieldPiece piece = {0};
        if (!dragoon_environment_tile(environment, i, &piece.tile) ||
            !piece.tile.width || !piece.tile.height ||
            piece.tile.width > INT16_MAX || piece.tile.height > INT16_MAX ||
            !dragoon_environment_texture(environment, i, &piece.original_tim))
            continue;

        piece.art = (DragoonFieldRect){
            .x = piece.tile.texture_x,
            .y = piece.tile.texture_y,
            .width = piece.tile.width,
            .height = piece.tile.height,
        };
        if ((key.cut == 111 && i == 8) || (key.cut == 288 && i == 17) ||
            (key.cut == 595 && i == 5))
            ++piece.art.y;
        if (key.cut == 642 && i == 2 && piece.art.width > 1)
            --piece.art.width;

        int32_t x2 = piece.art.x + (int32_t)piece.art.width;
        int32_t y2 = piece.art.y + (int32_t)piece.art.height;
        if (piece.art.x < left) left = piece.art.x;
        if (piece.art.y < top) top = piece.art.y;
        if (x2 > right) right = x2;
        if (y2 > bottom) bottom = y2;
        if (i >= rendered)
            continue;

        piece.foreground = piece.tile.foreground ? (int)i - environment->background_count : -1;
        int32_t clut_y = piece.tile.palette_y < 0 ? -piece.tile.palette_y : piece.tile.palette_y;
        if (piece.foreground < 0 && (clut_y < 496 || clut_y >= 512))
            clut_y = 496 + (int32_t)i;
        piece.clut_y = (uint16_t)(clut_y & 511);
        result.pieces[result.count++] = piece;
    }

    if (!result.count)
        return false;

    result.canvas = (DragoonFieldRect){
        .x = left,
        .y = top,
        .width = (uint32_t)(right - left),
        .height = (uint32_t)(bottom - top),
    };
    for (unsigned i = 0; i < result.count; ++i) {
        result.pieces[i].art.x -= left;
        result.pieces[i].art.y -= top;
    }
    *layout = result;
    return true;
}

const DragoonFieldPiece *dragoon_field_piece_for_draw(
    const DragoonFieldLayout *layout, uint16_t texture_page, uint16_t clut_x,
    uint16_t clut_y, uint32_t texture_window, const DragoonFieldRect *uv)
{
    /* GPU coordinates express the CLUT X in pixels, not GP0's units of 16. */
    if (!layout || !uv || clut_x != 0x300 || (texture_window & 0x3ffu) != 0)
        return NULL;

    const uint16_t texture_address_mask = 0x19f; /* Page position and format. */
    const DragoonFieldPiece *match = NULL;
    for (unsigned i = 0; i < layout->count; ++i) {
        const DragoonFieldPiece *piece = &layout->pieces[i];
        if (((texture_page ^ piece->tile.texture_page) & texture_address_mask) != 0 ||
            clut_y != piece->clut_y || uv->x != piece->tile.u || uv->y != piece->tile.v ||
            uv->width != piece->tile.width || uv->height != piece->tile.height)
            continue;

        if (match)
            return NULL;
        match = piece;
    }
    return match;
}
