#include <render/widget_renderer.h>

#include <stdlib.h>
#include <string.h>
#include <io/log.h>
#include <render/gpu_section.h>
#include <render/renderer.h>
#include <render/render_data_array.h>
#include <render/font_manager.h>
#include <window.h>
#include <glad/gl.h>

struct te_widget_renderer {
    te_render_data_array* text_widget_data;
    te_render_data_array* rect_widget_data;

    /* do not free/destroy this pointer */
    te_renderer* renderer;
    te_font_manager* font_manager;
};

te_widget_renderer*
widget_renderer_create(te_renderer* renderer) {
    te_widget_renderer* widget_renderer = malloc(sizeof(te_widget_renderer));
    widget_renderer->renderer = renderer;
    widget_renderer->font_manager = renderer_get_font_manager(renderer);

    widget_renderer->text_widget_data =
        render_data_array_create(sizeof(te_text_widget_render_data), 64, 64);
    widget_renderer->rect_widget_data =
        render_data_array_create(sizeof(te_rect_widget_render_data), 32, 32);

    return widget_renderer;
}

void
widget_renderer_destroy(te_widget_renderer* widget_renderer) {
    if (render_data_array_get_item_count(widget_renderer->text_widget_data) > 0) {
        log_error(
            __FILE__, __LINE__,
            "widget renderer is being destroyed but there are still some text widgets "
            "rendering");
        abort();
    }
    render_data_array_destroy(widget_renderer->text_widget_data);

    if (render_data_array_get_item_count(widget_renderer->rect_widget_data) > 0) {
        log_error(
            __FILE__, __LINE__,
            "widget renderer is being destroyed but there are still some rect widgets "
            "rendering");
        abort();
    }
    render_data_array_destroy(widget_renderer->rect_widget_data);

    free(widget_renderer);
}

unsigned int
widget_renderer_add_text_widget(te_widget_renderer* renderer) {
    const unsigned int handle = render_data_array_add_item(renderer->text_widget_data);

    /* init data */
    te_text_widget_render_data* data =
        render_data_array_get_item_data_tmp(renderer->text_widget_data, handle);
    memset(data, 0, sizeof(te_text_widget_render_data));

    return handle;
}

te_text_widget_render_data*
widget_renderer_get_text_widget_render_data_tmp(
    te_widget_renderer* renderer, unsigned int handle) {
    return render_data_array_get_item_data_tmp(renderer->text_widget_data, handle);
}

te_rect_widget_render_data*
widget_renderer_get_rect_widget_render_data_tmp(
    te_widget_renderer* renderer, unsigned int handle) {
    return render_data_array_get_item_data_tmp(renderer->rect_widget_data, handle);
}

void
widget_renderer_remove_text_widget(te_widget_renderer* renderer, unsigned int handle) {
    /* cleanup */
    te_text_widget_render_data* data =
        render_data_array_get_item_data_tmp(renderer->text_widget_data, handle);
    free(data->glyphs);

    render_data_array_remove_item(renderer->text_widget_data, handle);
}

unsigned int
widget_renderer_add_rect_widget(te_widget_renderer* renderer) {
    const unsigned int handle = render_data_array_add_item(renderer->rect_widget_data);

    /* init data */
    te_rect_widget_render_data* data =
        render_data_array_get_item_data_tmp(renderer->rect_widget_data, handle);
    memset(data, 0, sizeof(te_rect_widget_render_data));

    return handle;
}

void
widget_renderer_remove_rect_widget(te_widget_renderer* renderer, unsigned int handle) {
    render_data_array_remove_item(renderer->rect_widget_data, handle);
}

void
widget_renderer_draw(te_widget_renderer* widget_renderer) {
    unsigned int window_width;
    unsigned int window_height;
    unsigned int rect_widget_count;
    unsigned int text_widget_count;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    window_get_size(
        renderer_get_window(widget_renderer->renderer), &window_width, &window_height);

    /* we want to specify quads in pixels */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, (float)window_width, (float)window_height, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);

    /* draw rect widgets */
    rect_widget_count = render_data_array_get_item_count(widget_renderer->rect_widget_data);
    if (rect_widget_count > 0) {
        te_rect_widget_render_data* data;
        te_vec4 uv;
        unsigned int widget_idx;
        GPU_SECTION_BEGIN("rect");

        data = render_data_array_get_internal_array(widget_renderer->rect_widget_data);
        for (widget_idx = 0; widget_idx < rect_widget_count; widget_idx++) {
            glBindTexture(GL_TEXTURE_2D, data->tex_id);

            vec4_copy(data->clip_rect, uv);
            vec2_add(uv, data->uv_offset, uv);
            uv[2] *= data->uv_size[0];
            uv[3] *= data->uv_size[1];

            glBegin(GL_QUADS);

            glColor4fv(data->color);

            /* left-bottom */
            glTexCoord2fv(uv);
            glVertex2f(
                data->pos_pix[0] + data->size_pix[0] * data->clip_rect[0],
                data->pos_pix[1] + data->size_pix[1] * data->clip_rect[1]);

            /* top-left */
            glTexCoord2f(uv[0], uv[1] + uv[3]);
            glVertex2f(
                data->pos_pix[0] + data->size_pix[0] * data->clip_rect[0],
                data->pos_pix[1] + data->size_pix[1] * data->clip_rect[3]);

            /* top-right */
            glTexCoord2f(uv[0] + uv[2], uv[1] + uv[3]);
            glVertex2f(
                data->pos_pix[0] + data->size_pix[0] * data->clip_rect[2],
                data->pos_pix[1] + data->size_pix[1] * data->clip_rect[3]);

            /* right-bottom */
            glTexCoord2f(uv[0] + uv[2], uv[1]);
            glVertex2f(
                data->pos_pix[0] + data->size_pix[0] * data->clip_rect[2],
                data->pos_pix[1] + data->size_pix[1] * data->clip_rect[1]);

            glEnd();

            data += 1;
        }

        GPU_SECTION_END;
    }

    /* draw text widgets */
    text_widget_count = render_data_array_get_item_count(widget_renderer->text_widget_data);
    if (text_widget_count > 0) {
        te_text_widget_render_data* data;
        te_vec2 pos_pix;
        unsigned int widget_idx;
        unsigned int glyph_idx;

        GPU_SECTION_BEGIN("text");

        glBindTexture(
            GL_TEXTURE_2D, font_manager_get_font_bitmap_id(widget_renderer->font_manager));

        data = render_data_array_get_internal_array(widget_renderer->text_widget_data);

        for (widget_idx = 0; widget_idx < text_widget_count; widget_idx++) {
            glColor4fv(data->color);

            for (glyph_idx = 0; glyph_idx < data->glyph_count; glyph_idx++) {
                te_text_widget_glyph* glyph = &data->glyphs[glyph_idx];
                vec2_add(data->pos_pix, glyph->offset_pix, pos_pix);

                glBegin(GL_QUADS);

                /* left-bottom */
                glTexCoord2fv(glyph->glyph_rel_offset);
                glVertex2fv(pos_pix);

                /* top-left */
                glTexCoord2f(
                    glyph->glyph_rel_offset[0],
                    glyph->glyph_rel_offset[1] + glyph->glyph_rel_size[1]);
                glVertex2f(pos_pix[0], pos_pix[1] + glyph->size_pix[1]);

                /* top-right */
                glTexCoord2f(
                    glyph->glyph_rel_offset[0] + glyph->glyph_rel_size[0],
                    glyph->glyph_rel_offset[1] + glyph->glyph_rel_size[1]);
                glVertex2f(pos_pix[0] + glyph->size_pix[0], pos_pix[1] + glyph->size_pix[1]);

                /* right-bottom */
                glTexCoord2f(
                    glyph->glyph_rel_offset[0] + glyph->glyph_rel_size[0],
                    glyph->glyph_rel_offset[1]);
                glVertex2f(pos_pix[0] + glyph->size_pix[0], pos_pix[1]);

                glEnd();
            }

            data += 1;
        }

        GPU_SECTION_END;
    }

    glDisable(GL_TEXTURE_2D);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
