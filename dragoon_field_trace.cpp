#include "dragoon_field_runtime.h"
#include "dragoon_field_art.h"
#include "cdrom.h"
#include "iso_reader.h"
#include "dma_gpu_ll.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <exception>
#include <memory>

extern "C" uint32_t psx_read_word(uint32_t address);

namespace {
struct OpeningFieldTrace {
    PS1::ISOReader image;
    DragoonFieldDisc disc{};
    DragoonFieldOpeningDiagnostic diagnostic{};
    std::array<uint8_t, 512 * 1024> scratch;
};

std::unique_ptr<OpeningFieldTrace> trace;
bool test_art_selected = false;
uint64_t scene_generation = 1;
std::atomic<bool> packet_enabled{false};
std::atomic<uint64_t> accepted_presents{0};
struct PacketLog {
    uint64_t generation = 0, present = 0;
    uint32_t node = 0, count = 0, seen = 0, words[16]{};
    unsigned logged = 0, scene_logged = 0;
    uint64_t budget_generation = 0;
    bool open = false;
    bool producer_match = false;
    bool claimed = false;
    uint32_t producer_words[6]{};
} packet_log;
struct ProducerRecord { uint32_t node; uint64_t generation, present_epoch; bool valid, ambiguous; uint32_t words[6]; };
ProducerRecord producers[64]{};
unsigned producer_next = 0, producer_logged = 0, unload_logged = 0, transition_logged = 0;
uint64_t next_command_token = 1;

/* Raw linked-list nodes, not inferred GP0 packets. They can contain multiple
 * commands, incomplete commands, unrelated sprites or non-visible draws. */
void packet_observe(void *, unsigned event, uint32_t node, uint32_t count,
                    uint32_t index, uint32_t, uint32_t word) {
    if (!packet_enabled.load(std::memory_order_relaxed)) return;
    if (event == DMA_GPU_LL_OBSERVE_BEGIN) {
        uint64_t generation = 0;
        const auto *layout = dragoon_field_trace_current(&generation);
        if (packet_log.budget_generation != generation) {
            packet_log.budget_generation = generation;
            packet_log.scene_logged = 0;
        }
        /* Logging is bounded below; it must never disable draw identity. */
        packet_log.open = layout && count <= 255;
        packet_log.node = node; packet_log.count = count; packet_log.seen = 0;
        packet_log.generation = generation;
        packet_log.present = accepted_presents.load(std::memory_order_relaxed);
        packet_log.producer_match = false;
        packet_log.claimed = false;
        for (auto &p : producers) {
            if (p.valid && p.generation == generation && p.present_epoch == packet_log.present && p.node == (node & 0x1fffff)) {
                packet_log.producer_match = count >= 6 && !p.ambiguous;
                std::memcpy(packet_log.producer_words, p.words, sizeof p.words);
                p.valid = false; break;
            }
        }
        return;
    }
    if (!packet_log.open || node != packet_log.node || count != packet_log.count) return;
    if (event == DMA_GPU_LL_OBSERVE_ABORT) { packet_log.open = false; return; }
    if (event == DMA_GPU_LL_OBSERVE_WORD) {
        if (index != packet_log.seen || index >= count) { packet_log.open = false; return; }
        if (index < 6 && packet_log.producer_match && packet_log.producer_words[index] != word)
            packet_log.producer_match = false;
        if (index < 16) packet_log.words[index] = word;
        ++packet_log.seen;
        return;
    }
    uint64_t end_generation = 0;
    if (event != DMA_GPU_LL_OBSERVE_END || packet_log.seen != count ||
        !dragoon_field_trace_current(&end_generation) || end_generation != packet_log.generation) {
        packet_log.open = false; return;
    }
    if (packet_log.logged < 256 && packet_log.scene_logged < 32) {
    std::fprintf(stderr, "Dragoon packet node=%06x words=%u scene_gen=%llu prior_present=%llu candidate_producer=%d prefix=",
                 node, count, (unsigned long long)packet_log.generation,
                 (unsigned long long)packet_log.present, packet_log.producer_match ? 1 : 0);
    for (unsigned i = 0; i < count && i < 16; ++i)
        std::fprintf(stderr, "%s%08x", i ? "," : "", packet_log.words[i]);
    std::fprintf(stderr, "%s\n", count > 16 ? ",..." : "");
    ++packet_log.logged;
    ++packet_log.scene_logged;
    }
    packet_log.open = false;
}

bool find_extent(void *context, const char *name, DragoonFieldDiscExtent *extent) noexcept try {
    PS1::ISOFileEntry file;
    if (!static_cast<PS1::ISOReader*>(context)->FindFile(name, file) || file.is_directory)
        return false;
    *extent = {file.lba, file.size};
    return true;
} catch (const std::exception&) {
    return false;
}

bool read_sector(void *context, uint32_t lba, uint8_t *data) noexcept try {
    return static_cast<PS1::ISOReader*>(context)->ReadSector(lba, data);
} catch (const std::exception&) {
    return false;
}

bool read_guest(void*, uint32_t address, uint32_t *value) {
    *value = psx_read_word(address);
    return true;
}

bool read_chapter(void*, uint32_t *chapter) {
    // US retail GameState base + chapter index. DMA content and the mounted
    // archive directory must also match before any scene signal is emitted.
    *chapter = psx_read_word(0x800bac60);
    return *chapter <= 3;
}

void report_match(void *context, uint32_t lba, uint32_t address,
                  const DragoonFieldLayout *layout) {
    std::fprintf(stdout, "Dragoon: native field transfer archive=%u cut=%u lba=%u "
                         "ram=%08x canvas=%ux%u pieces=%u\n",
                 layout->key.archive, layout->key.cut, lba, address,
                 layout->canvas.width, layout->canvas.height, layout->count);
    (void)context;
    ++scene_generation;
}
void observe_dma(void *context, uint32_t lba, uint32_t address, const uint8_t *data) {
    auto *diag = static_cast<DragoonFieldOpeningDiagnostic*>(context);
    if (lba != diag->first_sector[0] && lba != diag->first_sector[1]) return;
    dragoon_field_runtime_invalidate(&diag->runtime);
    ++scene_generation;
    diag->reported_mask = 0;
    dragoon_field_runtime_opening_dma(context, lba, address, data);
}
}

extern "C" void dragoon_field_trace_invalidate(void) {
    if (trace) dragoon_field_runtime_invalidate(&trace->diagnostic.runtime);
    packet_log.open = false;
    for (auto &p : producers) p.valid = false;
    ++scene_generation;
}

extern "C" void dragoon_field_packet_observer_start(void) {
    const char *value = std::getenv("DRAGOON_FIELD_PACKET_TRACE");
    if (!value || std::strcmp(value, "1") != 0) return;
    packet_log = {};
    for (auto &p : producers) p.valid = false;
    producer_next = producer_logged = unload_logged = transition_logged = 0;
    next_command_token = 1;
    accepted_presents.store(0, std::memory_order_relaxed);
    packet_enabled.store(true, std::memory_order_relaxed);
    dma_gpu_ll_set_observer(packet_observe, nullptr);
    std::fprintf(stderr, "Dragoon: bounded raw packet observation enabled; present is queue acceptance only\n");
}
extern "C" uint64_t dragoon_field_claim_command(uint32_t node, uint32_t node_words,
    uint32_t offset, const uint32_t *words, uint32_t count, uint64_t *generation) {
    if (generation) *generation = 0;
    uint64_t current = 0;
    /* Only the final child of the witnessed six-word node can be certified:
     * earlier children would depend on future, unread DMA words. */
    if (!packet_enabled.load(std::memory_order_relaxed) || !generation || !words ||
        !packet_log.open || packet_log.claimed || !packet_log.producer_match ||
        packet_log.node != node || packet_log.count != node_words || node_words != 6 ||
        count == 0 || offset >= 6 || count != 6 - offset || packet_log.seen != 5 ||
        packet_log.present != accepted_presents.load(std::memory_order_relaxed) ||
        !dragoon_field_trace_current(&current) || current != packet_log.generation ||
        std::memcmp(packet_log.words, packet_log.producer_words, 5 * sizeof(uint32_t)) != 0 ||
        std::memcmp(words, packet_log.producer_words + offset, count * sizeof(uint32_t)) != 0)
        return 0;
    packet_log.claimed = true;
    *generation = current;
    uint64_t token = next_command_token++;
    if (!token) token = next_command_token++;
    return token;
}
extern "C" void dragoon_field_packet_observer_stop(void) {
    packet_enabled.store(false, std::memory_order_relaxed);
    dma_gpu_ll_set_observer(nullptr, nullptr);
    packet_log.open = false;
}
extern "C" void dragoon_field_packet_present_accepted(void) {
    if (!packet_enabled.load(std::memory_order_relaxed)) return;
    uint64_t n = accepted_presents.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n <= 64) std::fprintf(stderr, "Dragoon present accepted=%llu (not visibility proof)\n", (unsigned long long)n);
}
extern "C" void dragoon_field_packet_producer(uint32_t pc, uint32_t instruction, uint32_t packet) {
    if (!packet_enabled.load(std::memory_order_relaxed) || (pc & 0x1fffffffu) != 0x000e7a3cu ||
        instruction != 0x0c004d5fu || psx_read_word(0x800e7a38) != 0x00822021u ||
        psx_read_word(0x800e7a40) != 0x02002821u || psx_read_word(0x800e7a44) != 0x26520001u ||
        psx_read_word(0x800e519c) != 0x27bdff88u || psx_read_word(0x800e51a0) != 0xafbf0070u ||
        psx_read_word(0x800e51a4) != 0xafb3006cu || (packet & 3u) ||
        (packet & 0x1fffffffu) > 0x1fffe4u) return;
    uint64_t generation = 0;
    if (!dragoon_field_trace_current(&generation)) return;
    const uint64_t epoch = accepted_presents.load(std::memory_order_relaxed);
    for (auto &p : producers) {
        if (!p.valid || p.node != (packet & 0x1fffffu)) continue;
        if (p.generation == generation && p.present_epoch == epoch) {
            /* Reuse before consumption is ambiguous, even with equal bytes. */
            p.ambiguous = true;
            return;
        }
        p.valid = false;
    }
    auto &entry = producers[producer_next];
    entry.node = packet & 0x1fffffu; entry.generation = generation; entry.present_epoch = epoch;
    entry.valid = true; entry.ambiguous = false;
    for (unsigned i=0; i<6; ++i) entry.words[i] = psx_read_word(packet + 4u + i*4u);
    producer_next = (producer_next + 1) % 64;
    if (producer_logged++ < 64)
        std::fprintf(stderr, "Dragoon producer pc=%08x packet=%06x scene_gen=%llu (enqueue only)\n",
                     pc, packet & 0x1fffffu, (unsigned long long)generation);
}
extern "C" void dragoon_field_overlay_instruction(uint32_t pc, uint32_t instruction, uint32_t s0) {
    if (!packet_enabled.load(std::memory_order_relaxed)) return;
    uint32_t address = pc & 0x1fffffffu;
    if (address == 0x000e7a3cu) {
        dragoon_field_packet_producer(pc, instruction, s0);
    } else if (address == 0x000e5534u && instruction == 0x27bdffe0u &&
               psx_read_word(0x800e5538) == 0xafbf001cu &&
               psx_read_word(0x800e553c) == 0xafb20018u &&
               psx_read_word(0x800e5540) == 0xafb10014u &&
               psx_read_word(0x800e519c) == 0x27bdff88u &&
               psx_read_word(0x800e51a0) == 0xafbf0070u &&
               psx_read_word(0x800e51a4) == 0xafb3006cu &&
               psx_read_word(0x800e7a3c) == 0x0c004d5fu &&
               psx_read_word(0x800e7a40) == 0x02002821u) {
        /* Entry can reject a transition. Invalidate even in that case: native
         * art is safer until a later verified load, including same-cut reloads.
         * This only covers interpreted execution, not every possible path. */
        dragoon_field_trace_invalidate();
        if (transition_logged++ < 64)
            std::fprintf(stderr, "Dragoon: guarded SMAP transition attempt; field observation invalidated\n");
    } else if (address == 0x000e2220u && instruction == 0x27bdffd8u &&
               psx_read_word(0x800e2224) == 0x00002821u &&
               psx_read_word(0x800e2228) == 0x3c02800cu &&
               psx_read_word(0x800e222c) == 0x8c446740u &&
               psx_read_word(0x800e519c) == 0x27bdff88u &&
               psx_read_word(0x800e51a0) == 0xafbf0070u &&
               psx_read_word(0x800e51a4) == 0xafb3006cu &&
               psx_read_word(0x800e7a3c) == 0x0c004d5fu &&
               psx_read_word(0x800e7a40) == 0x02002821u) {
        /* US overlay unload entry: conservative invalidation, not a claim
         * that every unload/reload reaches this interpreter path. */
        dragoon_field_trace_invalidate();
        if (unload_logged++ < 64)
            std::fprintf(stderr, "Dragoon: guarded SMAP unload entry; field observation invalidated\n");
    }
}
extern "C" const DragoonFieldLayout *dragoon_field_trace_current(uint64_t *generation) {
    uint32_t chapter;
    const DragoonFieldLayout *layout = nullptr;
    if (trace) {
        bool was_valid = trace->diagnostic.runtime.valid;
        if (read_chapter(nullptr, &chapter))
            layout = dragoon_field_runtime_current(&trace->diagnostic.runtime, read_guest, nullptr, chapter);
        else
            dragoon_field_runtime_invalidate(&trace->diagnostic.runtime);
        if (was_valid && !trace->diagnostic.runtime.valid) ++scene_generation;
    }
    if (generation) *generation = scene_generation;
    return layout;
}

extern "C" void dragoon_field_trace_stop(void) {
    dragoon_field_trace_invalidate();
    cdrom_set_cooked_dma_observer(nullptr, nullptr);
    trace.reset();
    if (test_art_selected) {
        dragoon_field_art_clear();
        test_art_selected = false;
    }
}

extern "C" void dragoon_field_trace_start(const char *disc_path) {
    dragoon_field_trace_stop();
    const char *enabled = std::getenv("DRAGOON_FIELD_PACKET_TRACE");
    if (!enabled || std::strcmp(enabled, "1") != 0) return;
    if (!disc_path || !*disc_path)
        return;
    try {
        auto next = std::make_unique<OpeningFieldTrace>();
        if (!next->image.Open(disc_path) || next->image.GetVolumeID() != "SCUS94491")
            return;
        next->disc = { &next->image, next->image.GetSectorCount(), find_extent, read_sector };
        auto& diagnostic = next->diagnostic;
        diagnostic.disc = &next->disc;
        diagnostic.read = read_guest;
        diagnostic.read_chapter = read_chapter;
        diagnostic.scratch = next->scratch.data();
        diagnostic.capacity = next->scratch.size();
        diagnostic.context = next.get();
        diagnostic.on_match = report_match;
        for (uint32_t i = 0; i < 2; ++i) {
            DragoonFieldDiscExtent extent;
            if (!dragoon_field_disc_bundle(&next->disc, {1, 675 + i}, 0,
                                            diagnostic.scratch, diagnostic.capacity, &extent))
                return;
            diagnostic.first_sector[i] = extent.lba;
        }
        trace = std::move(next);
        cdrom_set_cooked_dma_observer(observe_dma, &trace->diagnostic);
        /* Explicit diagnostic only. The image is selected relative to the
         * data directory containing disc/, never an inferred pack format.
         * A valid layout and a certified draw are still required by the
         * rendering path; this selection alone proves no scene ownership. */
        const char *cut = std::getenv("DRAGOON_FIELD_TEST_CUT");
        const char *image = std::getenv("DRAGOON_FIELD_TEST_IMAGE");
        if (cut && image && (!std::strcmp(cut, "675") || !std::strcmp(cut, "676"))) {
            test_art_selected = dragoon_field_art_select_test(disc_path, cut, image);
            std::fprintf(stderr, "Dragoon: field background test selection %s for cut %s\n",
                         test_art_selected ? "configured" : "rejected", cut);
        }
        /* Explicit per-cut paths coexist; when both forms name one cut, the
         * per-cut path takes precedence. No names are discovered implicitly. */
        const char *images[] = {std::getenv("DRAGOON_FIELD_TEST_IMAGE_675"),
                                std::getenv("DRAGOON_FIELD_TEST_IMAGE_676")};
        const char *cuts[] = {"675", "676"};
        for (unsigned i = 0; i < 2; ++i) {
            if (!images[i]) continue;
            /* Invalid explicit overrides fall back to native art, never to a
             * previous selection for this cut. */
            dragoon_field_art_select(nullptr, {1, 675u + i}, -1, nullptr);
            bool selected = dragoon_field_art_select_test(disc_path, cuts[i], images[i]);
            test_art_selected = selected || test_art_selected;
            std::fprintf(stderr, "Dragoon: field background test selection %s for cut %s\n",
                         selected ? "configured" : "rejected", cuts[i]);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Dragoon: optional field observation unavailable: %s\n", error.what());
    }
}
