#include "dragoon_field_art.h"
#include "framework/runtime/include/texture_image_decode.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

struct DragoonFieldArt {
    DecodedTextureImage image;
    DragoonFieldLayout layout;
    uint32_t scale;
    int layer;
};

static void set_error(char *out, size_t size, const char *message) {
    if (out && size) std::snprintf(out, size, "%s", message);
}

static bool safe_relative(const char *path) {
    if (!path || !*path) return false;
    const char *part = path;
    for (const char *p = path;; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c == '/' || c == '\0') {
            size_t length = static_cast<size_t>(p - part);
            if (!length || (length == 1 && part[0] == '.') ||
                (length == 2 && part[0] == '.' && part[1] == '.')) return false;
            if (!c) return true;
            part = p + 1;
        } else if (c == '\\' || c == ':' || c < 32 || c == 127) return false;
    }
}

extern "C" DragoonFieldArt *dragoon_field_art_load(const char *root,
    const char *relative_path, const DragoonFieldLayout *layout, int layer,
    char *error, size_t error_size) {
    set_error(error, error_size, "");
    if (!root || !*root || !safe_relative(relative_path) || !layout ||
        layer < -1 || !layout->canvas.width || !layout->canvas.height ||
        !layout->count || layout->count > DRAGOON_MAX_FIELD_PIECES) {
        set_error(error, error_size, "invalid field art request"); return nullptr;
    }
    bool has_layer = false;
    for (unsigned i = 0; i < layout->count; ++i) {
        const DragoonFieldPiece &p = layout->pieces[i];
        if (p.foreground != layer) continue;
        has_layer = true;
        if (p.art.x < 0 || p.art.y < 0 || !p.art.width || !p.art.height ||
            static_cast<uint64_t>(p.art.x) + p.art.width > layout->canvas.width ||
            static_cast<uint64_t>(p.art.y) + p.art.height > layout->canvas.height) {
            set_error(error, error_size, "field piece outside canvas"); return nullptr;
        }
    }
    if (!has_layer) { set_error(error, error_size, "layer absent from field"); return nullptr; }
    try {
        std::string path(root);
        if (path.back() != '/' && path.back() != '\\') path += '/';
        path += relative_path;
        FILE *file = std::fopen(path.c_str(), "rb");
        if (!file) { set_error(error, error_size, "cannot open field art"); return nullptr; }
        bool valid = std::fseek(file, 0, SEEK_END) == 0;
        long length = valid ? std::ftell(file) : -1;
        valid = length > 0 && static_cast<unsigned long>(length) <= TEXTURE_IMAGE_MAX_ENCODED_BYTES &&
                std::fseek(file, 0, SEEK_SET) == 0;
        if (!valid) { std::fclose(file); set_error(error, error_size, "invalid field art size"); return nullptr; }
        std::vector<uint8_t> bytes;
        try { bytes.resize(static_cast<size_t>(length)); }
        catch (...) { std::fclose(file); throw; }
        size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
        std::fclose(file);
        if (read != bytes.size()) { set_error(error, error_size, "cannot read field art"); return nullptr; }
        TextureImageProbe probe;
        std::string why;
        if (!texture_image_probe(bytes.data(), bytes.size(), TEXTURE_IMAGE_DEFAULT_DECODED_BUDGET, probe, why)) {
            set_error(error, error_size, why.c_str()); return nullptr;
        }
        if (probe.width % layout->canvas.width || probe.height % layout->canvas.height ||
            probe.width / layout->canvas.width == 0 ||
            probe.width / layout->canvas.width != probe.height / layout->canvas.height) {
            set_error(error, error_size, "field art dimensions do not match canvas scale"); return nullptr;
        }
        DragoonFieldArt *art = new DragoonFieldArt();
        try {
            if (!texture_image_decode(bytes.data(), bytes.size(), TEXTURE_IMAGE_DEFAULT_DECODED_BUDGET, art->image, why)) {
                delete art; set_error(error, error_size, why.c_str()); return nullptr;
            }
            art->layout = *layout;
            art->scale = probe.width / layout->canvas.width;
            art->layer = layer;
            return art;
        } catch (...) { delete art; throw; }
    } catch (const std::bad_alloc &) {
        set_error(error, error_size, "field art allocation failed"); return nullptr;
    }
}

extern "C" void dragoon_field_art_free(DragoonFieldArt *art) { delete art; }
extern "C" const uint8_t *dragoon_field_art_pixels(const DragoonFieldArt *art,
    uint32_t *width, uint32_t *height) {
    if (!art) return nullptr;
    if (width) *width = art->image.width;
    if (height) *height = art->image.height;
    return art->image.rgba.data();
}
extern "C" bool dragoon_field_art_crop(const DragoonFieldArt *art, unsigned index,
    DragoonFieldRect *crop) {
    if (!art || !crop || index >= art->layout.count) return false;
    const DragoonFieldPiece &piece = art->layout.pieces[index];
    if (piece.foreground != art->layer) return false;
    crop->x = static_cast<int32_t>(static_cast<uint32_t>(piece.art.x) * art->scale);
    crop->y = static_cast<int32_t>(static_cast<uint32_t>(piece.art.y) * art->scale);
    crop->width = piece.art.width * art->scale;
    crop->height = piece.art.height * art->scale;
    return true;
}


namespace {
struct Selection {
    std::string root, path;
    DragoonSubmapKey key{};
    DragoonFieldArt *art = nullptr;
    uint64_t scene = 0;
};
/* Each background selection belongs to its own archive/cut. The bounded
 * table permits multiple scenes without treating one layer as global. */
Selection selections[64];
uint64_t selection_generation = 1;
void clear_cached(Selection &s) { dragoon_field_art_free(s.art); s.art = nullptr; s.scene = 0; }
}

extern "C" bool dragoon_field_art_select(const char *root, DragoonSubmapKey key, int layer, const char *path) {
    /* The runtime has no evidence yet that foregrounds are visible or that
     * their native mask reproduces authored alpha. Do not enable them. */
    if (layer != -1) return false;
    if (path && (!root || !*root || !safe_relative(path))) return false;
    try {
        std::string new_root(path ? root : ""), new_path(path ? path : "");
        Selection *slot = nullptr, *empty = nullptr;
        for (Selection &candidate : selections) {
            if (!candidate.path.empty() && candidate.key.archive == key.archive && candidate.key.cut == key.cut) { slot = &candidate; break; }
            if (!empty && candidate.path.empty()) empty = &candidate;
        }
        if (!slot) {
            if (!path) return true;
            slot = empty;
        }
        if (!slot) return false;
        Selection &s = *slot;
        clear_cached(s);
        s.root.swap(new_root); s.path.swap(new_path); s.key = key;
        ++selection_generation;
        return true;
    } catch (const std::bad_alloc &) { return false; }
}
extern "C" void dragoon_field_art_clear(void) {
    for (Selection &s : selections) { clear_cached(s); s.root.clear(); s.path.clear(); }
    ++selection_generation;
}
extern "C" bool dragoon_field_art_select_test(const char *disc_path, const char *cut, const char *image) {
    if (!disc_path || !cut || !image || (std::strcmp(cut, "675") && std::strcmp(cut, "676"))) return false;
    try {
        std::string disc(disc_path);
        size_t last = disc.find_last_of('/');
        if (last == std::string::npos || last == 0 || last + 1 == disc.size()) return false;
        size_t prior = disc.find_last_of('/', last - 1);
        if (prior == std::string::npos || prior == 0 || disc.substr(prior + 1, last - prior - 1) != "disc") return false;
        std::string root = disc.substr(0, prior);
        DragoonSubmapKey key = {1, static_cast<uint32_t>(std::strcmp(cut, "675") ? 676 : 675)};
        return dragoon_field_art_select(root.c_str(), key, -1, image);
    } catch (const std::bad_alloc &) { return false; }
}
extern "C" bool dragoon_field_art_active(uint64_t *scene_out, uint64_t *selection_out) {
    uint64_t scene = 0;
    const DragoonFieldLayout *layout = dragoon_field_trace_current(&scene);
    if (scene_out) *scene_out = scene;
    if (selection_out) *selection_out = selection_generation;
    if (!layout) return false;
    for (const Selection &s : selections)
        if (!s.path.empty() && s.key.archive == layout->key.archive && s.key.cut == layout->key.cut)
            return true;
    return false;
}
extern "C" bool dragoon_field_art_for_draw(uint16_t page, uint16_t clut_x,
    uint16_t clut_y, uint32_t window, const DragoonFieldRect *uv, DragoonFieldArtDraw *draw) {
    if (!draw || !uv) return false;
    *draw = {};
    uint64_t scene = 0;
    const DragoonFieldLayout *layout = dragoon_field_trace_current(&scene);
    if (!layout) return false;
    const DragoonFieldPiece *piece = dragoon_field_piece_for_draw(layout, page, clut_x, clut_y, window, uv);
    if (!piece || piece->foreground != -1) return false;
    Selection *selected = nullptr;
    for (Selection &candidate : selections)
        if (!candidate.path.empty() && candidate.key.archive == layout->key.archive && candidate.key.cut == layout->key.cut) { selected = &candidate; break; }
    if (!selected) return false;
    Selection &s = *selected;
    if (s.scene != scene) {
        for (Selection &candidate : selections) if (&candidate != &s) clear_cached(candidate);
        clear_cached(s);
        s.art = dragoon_field_art_load(s.root.c_str(), s.path.c_str(), layout, piece->foreground, nullptr, 0);
        s.scene = scene;
    }
    if (!s.art || !dragoon_field_art_crop(s.art, static_cast<unsigned>(piece - layout->pieces), &draw->crop)) return false;
    draw->rgba = dragoon_field_art_pixels(s.art, &draw->width, &draw->height);
    draw->scene_generation = scene;
    draw->selection_generation = selection_generation;
    return draw->rgba != nullptr;
}
