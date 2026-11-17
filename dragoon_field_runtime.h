#ifndef DRAGOON_FIELD_RUNTIME_H
#define DRAGOON_FIELD_RUNTIME_H

#include "dragoon_field_disc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Single-threaded adapter. The owner must invalidate before a scene exit,
 * disc change, reset, save-state restore, or any loss of observation. It must
 * also call current() before using a layout. No pointers to scratch are kept.
 * The caller is responsible for verifying that disc is US retail Disc 1.
 */
typedef struct {
    DragoonFieldLayout layout;
    uint32_t chapter;
    bool valid;
} DragoonFieldRuntime;

void dragoon_field_runtime_invalidate(DragoonFieldRuntime *runtime);

/* Observe a completed native guest CD DMA containing exactly one complete
 * cooked sector. lba must identify that sector, not merely the stream's
 * Setloc LBA; bytes are the actual guest DMA destination after completion.
 * This boundary is an integration contract, not a hook installed by this
 * module. Call on the emulator thread with no concurrent guest mutation.
 * A positive match proves a transfer of the selected bundle's first sector,
 * not that the field is visible. scratch is bounded caller-owned workspace.
 * On any failure no old layout remains active.
 */
bool dragoon_field_runtime_observe(DragoonFieldRuntime *runtime,
    const DragoonFieldDisc *disc, DragoonGuestRead read, void *guest,
    uint32_t chapter, uint32_t lba, const uint8_t *guest_sector,
    size_t guest_sector_size, uint8_t *scratch, size_t capacity);

/* Returns NULL and invalidates when the sampled key or chapter changes, or
 * when guest state cannot be sampled. Call once per use, on emulator thread. */
const DragoonFieldLayout *dragoon_field_runtime_current(DragoonFieldRuntime *runtime,
    DragoonGuestRead read, void *guest, uint32_t chapter);

/* Optional, bounded opening-sequence member diagnostic. Register
 * dragoon_field_runtime_opening_dma as the CD cooked DMA observer and supply
 * a zero-initialized context for the lifetime of that registration. No log
 * or renderer is enabled here; on_match is called at most once, synchronously,
 * per cut (675 and 676) after the guest sector, key and disc member agree.
 * Disc 1 provenance and a current chapter reader are the caller's duties. */
typedef struct {
    DragoonFieldRuntime runtime;
    const DragoonFieldDisc *disc;
    DragoonGuestRead read;
    void *guest;
    bool (*read_chapter)(void *guest, uint32_t *chapter);
    uint8_t *scratch;
    size_t capacity;
    uint32_t first_sector[2]; /* resolved from the mounted Disc 1 directory */
    void (*on_match)(void *context, uint32_t lba, uint32_t guest_address,
                     const DragoonFieldLayout *layout);
    void *context;
    uint8_t reported_mask;
} DragoonFieldOpeningDiagnostic;

void dragoon_field_runtime_opening_dma(void *context, uint32_t lba,
                                       uint32_t guest_address,
                                       const uint8_t *guest_data);

/* Optional observational witness; this NEVER authorizes replacement. Caller
 * must establish producer and packet addresses against the mounted US game,
 * and call runtime_current before recording a submit. A sector observation
 * is a prerequisite but is not proof that the submitted draw is visible.
 * scene_generation must change on every reset, exit, reload, disc swap and
 * restore, even when returning to the same cut. Tokens must be unique for
 * the lifetime of each scene generation. No production hook is installed.
 */
enum { DRAGOON_FIELD_WITNESS_CAPACITY = 32 };
typedef struct {
    uint64_t token;
    uint32_t producer_pc, packet_address;
    uint16_t texture_page, clut_x, clut_y;
    uint32_t texture_window;
    DragoonFieldRect uv;
    int32_t x, y, width, height;
} DragoonFieldPacket;
typedef struct {
    DragoonFieldPacket packet;
    DragoonSubmapKey key;
    uint64_t scene_generation, frame;
    unsigned piece_index;
    bool presented;
} DragoonFieldPacketRecord;
typedef struct {
    DragoonFieldPacketRecord records[DRAGOON_FIELD_WITNESS_CAPACITY];
    uint64_t scene_generation;
    unsigned count, next;
} DragoonFieldWitness;

void dragoon_field_witness_reset(DragoonFieldWitness *witness);
bool dragoon_field_witness_submit(DragoonFieldWitness *witness,
    const DragoonFieldRuntime *runtime, uint64_t scene_generation,
    const DragoonFieldPacket *packet);
/* Caller supplies actual presentation evidence, not a queued or clipped draw.
 * Returns false when the token was evicted, belongs to another generation,
 * or was already marked. A presentation mark alone is still observational. */
bool dragoon_field_witness_present(DragoonFieldWitness *witness,
    const DragoonFieldRuntime *runtime, uint64_t scene_generation,
    uint64_t token, uint64_t frame);

/* Drop-in host binding for read-only Disc 1 opening-scene evidence. */
void dragoon_field_trace_start(const char *disc_path);
void dragoon_field_trace_stop(void);
/* Diagnostic only. Start before emulation begins, stop after it ends; disabled
 * unless DRAGOON_FIELD_PACKET_TRACE=1. Present denotes queue acceptance only,
 * never visibility or producer ownership. */
void dragoon_field_packet_observer_start(void);
void dragoon_field_packet_observer_stop(void);
void dragoon_field_packet_present_accepted(void);
/* Called only by the optional interpreter probe at the US SMAP enqueue
 * callsite. Correlation is diagnostic, never permission to replace artwork. */
void dragoon_field_packet_producer(uint32_t pc, uint32_t instruction, uint32_t packet);
void dragoon_field_overlay_instruction(uint32_t pc, uint32_t instruction, uint32_t s0);
uint64_t dragoon_field_claim_command(uint32_t node, uint32_t node_words,
    uint32_t offset, const uint32_t *words, uint32_t count, uint64_t *generation);

#ifdef __cplusplus
}
#endif
#endif
