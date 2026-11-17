#include "dragoon_field_runtime.h"

#include <string.h>

void dragoon_field_runtime_invalidate(DragoonFieldRuntime *runtime)
{
    if (runtime) runtime->valid = false;
}

static bool same_key(DragoonSubmapKey a, DragoonSubmapKey b)
{
    return a.archive == b.archive && a.cut == b.cut;
}

bool dragoon_field_runtime_observe(DragoonFieldRuntime *runtime,
    const DragoonFieldDisc *disc, DragoonGuestRead read, void *guest,
    uint32_t chapter, uint32_t lba, const uint8_t *guest_sector,
    size_t guest_sector_size, uint8_t *scratch, size_t capacity)
{
    if (!runtime) return false;
    dragoon_field_runtime_invalidate(runtime);
    DragoonSubmapKey before, after;
    DragoonFieldDiscExtent bundle;
    DragoonFieldLayout layout;
    uint8_t original[DRAGOON_FIELD_SECTOR_SIZE];
    if (!disc || !disc->read_sector || !guest_sector ||
        guest_sector_size != DRAGOON_FIELD_SECTOR_SIZE ||
        !dragoon_submap_key(read, guest, &before) ||
        !dragoon_field_disc_bundle(disc, before, chapter, scratch, capacity, &bundle) ||
        bundle.size < DRAGOON_FIELD_SECTOR_SIZE || bundle.lba != lba ||
        !disc->read_sector(disc->context, lba, original) ||
        memcmp(original, guest_sector, sizeof(original)) != 0 ||
        !dragoon_field_disc_layout(disc, before, chapter, scratch, capacity, &layout) ||
        !dragoon_submap_key(read, guest, &after) || !same_key(before, after) ||
        !same_key(before, layout.key))
        return false;
    runtime->layout = layout;
    runtime->chapter = chapter;
    runtime->valid = true;
    return true;
}

const DragoonFieldLayout *dragoon_field_runtime_current(DragoonFieldRuntime *runtime,
    DragoonGuestRead read, void *guest, uint32_t chapter)
{
    DragoonSubmapKey key;
    if (!runtime || !runtime->valid) return NULL;
    if (chapter != runtime->chapter || !dragoon_submap_key(read, guest, &key) ||
        !same_key(key, runtime->layout.key)) {
        dragoon_field_runtime_invalidate(runtime);
        return NULL;
    }
    return &runtime->layout;
}

void dragoon_field_runtime_opening_dma(void *context, uint32_t lba,
                                       uint32_t guest_address,
                                       const uint8_t *guest_data)
{
    DragoonFieldOpeningDiagnostic *diag = context;
    DragoonSubmapKey key;
    DragoonFieldDiscExtent member;
    uint32_t chapter;
    if (!diag || !diag->disc || !diag->read_chapter ||
        (lba != diag->first_sector[0] && lba != diag->first_sector[1]) ||
        !diag->read_chapter(diag->guest, &chapter) ||
        !dragoon_submap_key(diag->read, diag->guest, &key) ||
        key.archive != 1 || (key.cut != 675 && key.cut != 676) ||
        (diag->reported_mask & (uint8_t)(1u << (key.cut - 675))) ||
        !dragoon_field_disc_bundle(diag->disc, key, chapter, diag->scratch,
                                    diag->capacity, &member) || member.lba != lba)
        return;
    if (!dragoon_field_runtime_observe(&diag->runtime, diag->disc, diag->read,
            diag->guest, chapter, lba, guest_data, DRAGOON_FIELD_SECTOR_SIZE,
            diag->scratch, diag->capacity))
        return;
    diag->reported_mask |= (uint8_t)(1u << (key.cut - 675));
    if (diag->on_match)
        diag->on_match(diag->context, lba, guest_address, &diag->runtime.layout);
}

void dragoon_field_witness_reset(DragoonFieldWitness *witness)
{
    if (witness) memset(witness, 0, sizeof(*witness));
}

bool dragoon_field_witness_submit(DragoonFieldWitness *witness,
    const DragoonFieldRuntime *runtime, uint64_t generation,
    const DragoonFieldPacket *packet)
{
    if (!witness || !runtime || !runtime->valid || !packet || !generation ||
        !packet->token || !packet->producer_pc || !packet->packet_address ||
        packet->width <= 0 || packet->height <= 0) return false;
    const DragoonFieldPiece *piece = dragoon_field_piece_for_draw(&runtime->layout,
        packet->texture_page, packet->clut_x, packet->clut_y,
        packet->texture_window, &packet->uv);
    if (!piece || piece->foreground != -1) return false;
    if (witness->scene_generation != generation) {
        dragoon_field_witness_reset(witness);
        witness->scene_generation = generation;
    }
    for (unsigned i = 0; i < witness->count; ++i)
        if (witness->records[i].packet.token == packet->token) return false;
    DragoonFieldPacketRecord *record = &witness->records[witness->next];
    memset(record, 0, sizeof(*record));
    record->packet = *packet;
    record->key = runtime->layout.key;
    record->scene_generation = generation;
    record->piece_index = (unsigned)(piece - runtime->layout.pieces);
    witness->next = (witness->next + 1) % DRAGOON_FIELD_WITNESS_CAPACITY;
    if (witness->count < DRAGOON_FIELD_WITNESS_CAPACITY) ++witness->count;
    return true;
}

bool dragoon_field_witness_present(DragoonFieldWitness *witness,
    const DragoonFieldRuntime *runtime, uint64_t generation, uint64_t token, uint64_t frame)
{
    if (!witness || !runtime || !runtime->valid || !generation ||
        generation != witness->scene_generation || !token)
        return false;
    for (unsigned i = 0; i < witness->count; ++i) {
        DragoonFieldPacketRecord *record = &witness->records[i];
        if (record->packet.token != token || record->scene_generation != generation ||
            record->key.archive != runtime->layout.key.archive ||
            record->key.cut != runtime->layout.key.cut) continue;
        if (record->presented) return false;
        record->frame = frame;
        record->presented = true;
        return true;
    }
    return false;
}
