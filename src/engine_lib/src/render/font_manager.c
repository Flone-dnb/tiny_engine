#include <render/font_manager.h>
#include <stdint.h>

#include <freetype/freetype.h>
#include <misc/num_hashtable.h>
#include <io/filesystem.h>
#include <io/log.h>
#include <io/paths.h>
#include <render/renderer.h>
#include <window.h>
#include <glad/gl.h>

/* value in range [0.0; 1.0]. Font height (relative to screen height, width is determines automatically)
 * this value will be used as the base size but will be scaled when drawing text according to the size of each text widget
 * this value must be equal to the average size of the text, if it's too small big text will be blurry,
 * if it will be too big small text will look bad */
#if defined(ENGINE_EDITOR)
#define TE_FONT_HEIGHT_TO_LOAD 0.05f
#else
#define TE_FONT_HEIGHT_TO_LOAD 0.075f
#endif

struct te_font_manager {
    /* do not free this pointer */
    te_renderer* renderer;

    FT_Library ft_library;

    /* non-NULL if have a loaded font */
    FT_Face ft_face;

    /* stores loaded glyphs */
    te_num_hashtable* cached_glyphs;
};

static void
free_glyph(void* data) {
    te_font_glyph* glyph = data;
    glDeleteTextures(1, &glyph->tex_id);
}

te_font_manager*
prv_font_manager_create(te_renderer* renderer) {
    int error_code;
    te_font_manager* manager = malloc(sizeof(te_font_manager));

    manager->renderer = renderer;
    manager->cached_glyphs = num_hashtable_create(512, sizeof(te_font_glyph), free_glyph);
    manager->ft_face = NULL;

    error_code = FT_Init_FreeType(&manager->ft_library);
    if (error_code != 0) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to init FreeType library, error: %d", error_code);
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
            log_error_fmt(
                __FILE__, __LINE__, "failed to deinit FreeType face, error: %d", error_code);
            abort();
        }
    }

    error_code = FT_Done_FreeType(manager->ft_library);
    if (error_code != 0) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to deinit FreeType library, error: %d", error_code);
        abort();
    }

    num_hashtable_destroy(manager->cached_glyphs);

    free(manager);
}

void
prv_font_manager_clear_cache(te_font_manager* manager) {
    unsigned int font_height;

    te_window* window = renderer_get_window(manager->renderer);

    unsigned int window_width = 0;
    unsigned int window_height = 0;
    window_get_size(window, &window_width, &window_height);

    font_height = (unsigned int)((float)window_height * TE_FONT_HEIGHT_TO_LOAD);
    FT_Set_Pixel_Sizes(manager->ft_face, 0, font_height);

    num_hashtable_clear(manager->cached_glyphs);

    /* cache ASCII */
    font_manager_cache_glyphs(manager, 32, 126);
}

void
font_manager_load_font(te_font_manager* manager, const char* relative_path) {
    char* path_to_font;
    int error_code = 0;

    if (manager->ft_face != NULL) {
        /* unload old font */
        error_code = FT_Done_Face(manager->ft_face);
        if (error_code != 0) {
            log_error_fmt(
                __FILE__, __LINE__, "failed to deinit FreeType face, error: %d", error_code);
            abort();
            ;
        }
        manager->ft_face = NULL;
    }

    path_to_font = filesystem_prepend_res_to_path(relative_path, NULL);
    if (!filesystem_does_path_exists(path_to_font)) {
        log_error_fmt(__FILE__, __LINE__, "the path \"%s\" does not exist", path_to_font);
    }

    /* load new font */
    error_code = FT_New_Face(manager->ft_library, path_to_font, 0, &manager->ft_face);
    if (error_code != 0) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to create FreeType face, error: %d", error_code);
        abort();
    }

    free(path_to_font);

    prv_font_manager_clear_cache(manager);
}

te_font_glyph
font_manager_get_glyph(te_font_manager* manager, unsigned long char_code) {
    te_font_glyph* glyph = num_hashtable_find(manager->cached_glyphs, char_code);
    if (glyph == NULL) {
        font_manager_cache_glyphs(manager, char_code, char_code);
        glyph = num_hashtable_find(manager->cached_glyphs, char_code);
    }

    return *glyph;
}

void
font_manager_cache_glyphs(
    te_font_manager* manager, unsigned long char_code_first, unsigned long char_code_last) {
    te_font_glyph new_glyph;
    te_font_glyph* glyph;
    unsigned long char_code;
    unsigned int tex_id;
    unsigned int gl_type;
    unsigned int gl_format;
    int error_code;
    int prev_unpack_alignment;

    if (char_code_first > char_code_last) {
        log_error(__FILE__, __LINE__, "the specified character code range is invalid");
        abort();
    }
    if (manager->ft_face == NULL) {
        log_error(__FILE__, __LINE__, "font face was not loaded yet");
        abort();
    }

    gl_type = GL_UNSIGNED_BYTE;
#if defined(ENGINE_GLES)
    gl_format = GL_LUMINANCE;
#else
    gl_format = GL_RED;
#endif

    /* set byte-alignment to 1 because we will create single-channel textures */
    prev_unpack_alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (char_code = char_code_first; char_code <= char_code_last; char_code++) {
        glyph = num_hashtable_find(manager->cached_glyphs, char_code);
        if (glyph != NULL) {
            /* already cached */
            continue;
        }

        /* load glyph */
        error_code = FT_Load_Char(manager->ft_face, char_code, FT_LOAD_RENDER);
        if (error_code != 0) {
            log_error_fmt(
                __FILE__, __LINE__, "failed to load glyph for character %u, error: %d",
                char_code, error_code);
            abort();
        }

        /* create texture */
        tex_id = 0;
        glGenTextures(1, &tex_id);

        glBindTexture(GL_TEXTURE_2D, tex_id);
        {
            glTexImage2D(
                GL_TEXTURE_2D, 0, (GLint)gl_format, (int)manager->ft_face->glyph->bitmap.width,
                (int)manager->ft_face->glyph->bitmap.rows, 0, (GLenum)gl_format, gl_type,
                manager->ft_face->glyph->bitmap.buffer);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        /* save */
        new_glyph.tex_id = tex_id;
        new_glyph.char_code = char_code;
        new_glyph.width = manager->ft_face->glyph->bitmap.width;
        new_glyph.height = manager->ft_face->glyph->bitmap.rows;
        new_glyph.bearing_x = manager->ft_face->glyph->bitmap_left;
        new_glyph.bearing_y = manager->ft_face->glyph->bitmap_top;
        new_glyph.advance = (unsigned int)manager->ft_face->glyph->advance.x;

        num_hashtable_insert(manager->cached_glyphs, char_code, &new_glyph);
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
prv_font_manager_get_font_height_to_load(void) {
    return TE_FONT_HEIGHT_TO_LOAD;
}
