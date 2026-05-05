#include <render/font_manager.h>
#include <stdint.h>

#include <freetype/freetype.h>
#include <hashmap.c/hashmap.h>
#include <io/filesystem.h>
#include <io/log.h>
#include <io/paths.h>
#include <render/renderer.h>
#include <window.h>
#include <glad/glad.h>

// Value in range [0.0; 1.0]. Font height (relative to screen height, width is determines automatically).
// This value will be used as the base size but will be scaled when drawing text according to the size of each text widget.
// This value must be equal to the average size of the text, if it's too small big text will be blurry,
// if it will be too big small text will look bad.
#if defined(ENGINE_EDITOR)
#define TE_FONT_HEIGHT_TO_LOAD 0.05f
#else
#define TE_FONT_HEIGHT_TO_LOAD 0.075f
#endif

// Compare function for hashmap.
int
font_manager_glyph_compare(const void* a, const void* b, void* udata) {
    (void)udata;
    const te_font_glyph* glyph1 = a;
    const te_font_glyph* glyph2 = b;
    return glyph1->char_code != glyph2->char_code;
}

// Hash function for hashmap.
uint64_t
font_manager_glyph_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    (void)seed0;
    (void)seed1;
    const te_font_glyph* glyph = item;
    return glyph->char_code;
}

struct te_font_manager {
    // Do not free this pointer.
    te_renderer* renderer;

    FT_Library ft_library;

    // Non-NULL if have a loaded font.
    FT_Face ft_face;

    // Stores loaded glyphs.
    struct hashmap* cached_glyphs;
};

te_font_manager*
prv_font_manager_create(te_renderer* renderer) {
    te_font_manager* manager = malloc(sizeof(te_font_manager));

    manager->renderer = renderer;
    manager->cached_glyphs = hashmap_new(
        sizeof(te_font_glyph), 64, 0, 0, font_manager_glyph_hash, font_manager_glyph_compare,
        NULL, NULL);
    manager->ft_face = NULL;

    const int error_code = FT_Init_FreeType(&manager->ft_library);
    if (error_code != 0) {
        log_error_fmt("failed to init FreeType library, error: %d", error_code);
        abort();
    }

    return manager;
}

void
prv_font_manager_destroy(te_font_manager* manager) {
    int error_code = 0;

    if (manager->ft_face != NULL) {
        error_code = FT_Done_Face(manager->ft_face);
        if (error_code != 0) {
            log_error_fmt("failed to deinit FreeType face, error: %d", error_code);
            abort();
        }
    }

    error_code = FT_Done_FreeType(manager->ft_library);
    if (error_code != 0) {
        log_error_fmt("failed to deinit FreeType library, error: %d", error_code);
        abort();
    }

    size_t iter = 0;
    void* item;
    while (hashmap_iter(manager->cached_glyphs, &iter, &item)) {
        const te_font_glyph* glyph = item;
        glDeleteTextures(1, &glyph->tex_id);
    }
    hashmap_free(manager->cached_glyphs);

    free(manager);
}

void
prv_font_manager_clear_cache(te_font_manager* manager) {
    te_window* window = renderer_get_window(manager->renderer);

    unsigned int window_width = 0;
    unsigned int window_height = 0;
    window_get_size(window, &window_width, &window_height);

    const unsigned int font_height =
        (unsigned int)((float)window_height * TE_FONT_HEIGHT_TO_LOAD);
    FT_Set_Pixel_Sizes(manager->ft_face, 0, font_height);

    size_t iter = 0;
    void* item;
    while (hashmap_iter(manager->cached_glyphs, &iter, &item)) {
        const te_font_glyph* glyph = item;
        glDeleteTextures(1, &glyph->tex_id);
    }
    hashmap_clear(manager->cached_glyphs, true);

    // Cache ASCII.
    font_manager_cache_glyphs(manager, 32, 126);
}

void
font_manager_load_font(te_font_manager* manager, const char* relative_path) {
    int error_code = 0;

    if (manager->ft_face != NULL) {
        // Unload old font.
        error_code = FT_Done_Face(manager->ft_face);
        if (error_code != 0) {
            log_error_fmt("failed to deinit FreeType face, error: %d", error_code);
            abort();
            ;
        }
        manager->ft_face = NULL;
    }

    char* path_to_font = filesystem_prepend_res_to_path(relative_path, NULL);
    if (!filesystem_does_path_exists(path_to_font)) {
        log_error_fmt("the path \"%s\" does not exist", path_to_font);
    }

    // Load new font.
    error_code = FT_New_Face(manager->ft_library, path_to_font, 0, &manager->ft_face);
    if (error_code != 0) {
        log_error_fmt("failed to create FreeType face, error: %d", error_code);
        abort();
    }

    free(path_to_font);

    prv_font_manager_clear_cache(manager);
}

te_font_glyph
font_manager_get_glyph(te_font_manager* manager, unsigned long char_code) {
    const te_font_glyph* glyph =
        hashmap_get(manager->cached_glyphs, &(te_font_glyph){.char_code = char_code});
    if (glyph == NULL) {
        font_manager_cache_glyphs(manager, char_code, char_code);
        glyph = hashmap_get(manager->cached_glyphs, &(te_font_glyph){.char_code = char_code});
    }

    return *glyph;
}

void
font_manager_cache_glyphs(
    te_font_manager* manager, unsigned long char_code_first, unsigned long char_code_last) {
    if (char_code_first > char_code_last) {
        log_error("the specified character code range is invalid");
        abort();
    }

    // Set byte-alignment to 1 because we will create single-channel textures.
    int prev_unpack_alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (unsigned long char_code = char_code_first; char_code <= char_code_last; char_code++) {
        const te_font_glyph* glyph =
            hashmap_get(manager->cached_glyphs, &(te_font_glyph){.char_code = char_code});
        if (glyph != NULL) {
            // Already cached.
            continue;
        }

        // Load glyph.
        int error_code = FT_Load_Char(manager->ft_face, char_code, FT_LOAD_RENDER);
        if (error_code != 0) {
            log_error_fmt(
                "failed to load glyph for character %u, error: %d", char_code, error_code);
            abort();
        }

        // Create texture.
        unsigned int tex_id = 0;
        glGenTextures(1, &tex_id);
        const int gl_format = GL_UNSIGNED_BYTE;
        glBindTexture(GL_TEXTURE_2D, tex_id);
        {
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_LUMINANCE, (int)manager->ft_face->glyph->bitmap.width,
                (int)manager->ft_face->glyph->bitmap.rows, 0, GL_LUMINANCE,
                (unsigned int)gl_format, manager->ft_face->glyph->bitmap.buffer);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        // Save.
        te_font_glyph new_glyph;
        new_glyph.tex_id = tex_id;
        new_glyph.char_code = char_code;
        new_glyph.width = manager->ft_face->glyph->bitmap.width;
        new_glyph.height = manager->ft_face->glyph->bitmap.rows;
        new_glyph.bearing_x = manager->ft_face->glyph->bitmap_left;
        new_glyph.bearing_y = manager->ft_face->glyph->bitmap_top;
        new_glyph.advance = (unsigned int)manager->ft_face->glyph->advance.x;
        hashmap_set(manager->cached_glyphs, &new_glyph);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_alignment);
}

void
prv_font_manager_on_window_size_changed(te_font_manager* manager) {
    if (manager->ft_face != NULL) {
        prv_font_manager_clear_cache(manager);
    }
}

float
prv_font_manager_get_font_height_to_load() {
    return TE_FONT_HEIGHT_TO_LOAD;
}
