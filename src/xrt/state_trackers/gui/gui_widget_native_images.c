// Copyright 2019-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Swapchain rendering helper code.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup gui
 */

#include "ogl/ogl_api.h"
#include "ogl/ogl_helpers.h"

#include "util/u_misc.h"
#include "util/u_logging.h"
#include "util/u_native_images_debug.h"

#include "gui_ogl.h"
#include "gui_imgui.h"
#include "gui_widget_native_images.h"

#include <assert.h>


/*
 *
 * Helpers.
 *
 */

static void
destroy_resources(struct gui_widget_native_images *gwni)
{
	uint32_t texture_count = gwni->texture_count;
	if (texture_count == 0) {
		return;
	}

	glDeleteTextures(texture_count, gwni->textures);
	glDeleteMemoryObjectsEXT(texture_count, gwni->memories);

	U_ZERO_ARRAY(gwni->textures);
	U_ZERO_ARRAY(gwni->memories);
	gwni->texture_count = 0;
}

static void
recreate_locked(struct gui_widget_native_images *gwni, struct u_native_images_debug *unid)
{
	// Clear old resources.
	destroy_resources(gwni);

	if (unid->native_images == NULL) {
		return;
	}

	struct ogl_import_results results;
	bool bret = ogl_import_from_native( //
	    unid->native_images,            //
	    unid->native_image_count,       //
	    unid->xscci,                    //
	    &results);                      //
	if (!bret) {
		return;
	}

	for (uint32_t i = 0; i < results.image_count; i++) {
		gwni->textures[i] = results.textures[i];
	}
	gwni->texture_count = results.image_count;
	gwni->width = unid->xscci->width;
	gwni->height = unid->xscci->height;
	gwni->flip_y = unid->flip_y;
}

/*
 *
 * Misc helpers and interface functions.
 *
 */

static void
window_draw_misc(struct gui_widget_native_images *gwni)
{
	igSliderFloat("", &gwni->scale, 20.0, 300.f, "Scale %f%%", ImGuiSliderFlags_None);

	static ImVec2 button_dims = {0, 0};

	igSameLine(0.0f, 4.0f);
	bool minus = igButton("-", button_dims);

	igSameLine(0.0f, 4.0f);
	bool plus = igButton("+", button_dims);

	static const double scales[] = {
	    25.0, 50.0, 100.0, 200.0, 300.0,
	};

	if (plus) {
		for (uint32_t i = 0; i < ARRAY_SIZE(scales); i++) {
			if (gwni->scale >= scales[i]) {
				continue;
			}
			gwni->scale = scales[i];
			break;
		}
	} else if (minus) {
		// Initial length always greater then 0 so safe.
		for (uint32_t i = ARRAY_SIZE(scales); i-- > 0;) {
			if (gwni->scale <= scales[i]) {
				continue;
			}
			gwni->scale = scales[i];
			break;
		}
	}

	igSameLine(0, 30);

	igCheckbox("Rotate 180 degrees", &gwni->rotate_180);
}

/*
 *
 * 'Exported' widget functions.
 *
 */

void
gui_widget_native_images_init(struct gui_widget_native_images *gwni)
{
	U_ZERO(gwni);
	gwni->active_index = GUI_WIDGET_SWAPCHAIN_INVALID_INDEX;
	gwni->scale = 50.0f;
}

void
gui_widget_native_images_update(struct gui_widget_native_images *gwni, struct u_native_images_debug *unid)
{
	u_native_images_debug_lock(unid);

	xrt_limited_unique_id_t id = unid->limited_unique_id;
	if (gwni->cache_id.data != id.data) {
		U_LOG_D("Updating imported textures (%" PRIu64 " !=  %" PRIu64 ")", gwni->cache_id.data, id.data);
		recreate_locked(gwni, unid);
		gwni->cache_id = id;
	}

	if (id.data == 0 || gwni->texture_count == 0) {
		gwni->active_index = GUI_WIDGET_SWAPCHAIN_INVALID_INDEX;
		gwni->flip_y = false;
	} else {
		gwni->active_index = unid->active_index;
		gwni->flip_y = unid->flip_y;
	}

	u_native_images_debug_unlock(unid);
}

void
gui_widget_native_images_render(struct gui_widget_native_images *gwni, struct gui_program *p)
{
	if (gwni->active_index == GUI_WIDGET_SWAPCHAIN_INVALID_INDEX) {
		return;
	}

	igPushID_Ptr(gwni);

	window_draw_misc(gwni);

	uint32_t tex_id = gwni->textures[gwni->active_index];

	gui_ogl_draw_image(       //
	    gwni->width,          // width
	    gwni->height,         // height
	    tex_id,               // tex_id
	    gwni->scale / 100.0f, // scale
	    gwni->rotate_180,     // rotate_180
	    gwni->flip_y);        // flip_y

	igPopID();
}

void
gui_widget_native_images_to_background(struct gui_widget_native_images *gwni, struct gui_program *p)
{
	if (gwni->active_index == GUI_WIDGET_SWAPCHAIN_INVALID_INDEX) {
		return;
	}

	uint32_t tex_id = gwni->textures[gwni->active_index];

	gui_ogl_draw_background( //
	    gwni->width,         // width
	    gwni->height,        // height
	    tex_id,              // tex_id
	    false,               // rotate_180
	    gwni->flip_y);       // flip_y
}

void
gui_widget_native_images_close(struct gui_widget_native_images *gwni)
{
	destroy_resources(gwni);
	U_ZERO(gwni);
}


/*
 *
 * 'Exported' storage functions.
 *
 */

struct gui_widget_native_images *
gui_widget_native_images_storage_ensure(struct gui_widget_native_images_storage *gwnis,
                                        struct u_native_images_debug *unid)
{
	assert(unid != NULL);

	for (uint32_t i = 0; i < ARRAY_SIZE(gwnis->records); i++) {
		if (gwnis->records[i].ptr == unid) {
			return &gwnis->records[i].gwni;
		}
	}

	for (uint32_t i = 0; i < ARRAY_SIZE(gwnis->records); i++) {
		if (gwnis->records[i].ptr != NULL) {
			continue;
		}

		gui_widget_native_images_init(&gwnis->records[i].gwni);
		gwnis->records[i].ptr = unid;

		return &gwnis->records[i].gwni;
	}

	return NULL;
}
