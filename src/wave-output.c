/*
 * obs-wave-plugin — wave-output output type.
 *
 * Stubbed lifecycle (create / destroy / start / stop). Frame callbacks
 * (raw_video / raw_audio / encoded_packet) are NULL until Wave 2 — task
 * #163 wires the real SRT push + gateway JWT bearer.
 *
 * Copyright (C) 2026  WAVE Online LLC
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <obs-module.h>
#include <util/bmem.h>

#include "wave-output.h"

struct wave_output_data {
	obs_output_t *output;
	bool active;
};

/* forward decls so the obs_output_info table reads top-down */
static const char *wave_output_get_name(void *type_data);
static void *wave_output_create(obs_data_t *settings, obs_output_t *output);
static void wave_output_destroy(void *data);
static bool wave_output_start(void *data);
static void wave_output_stop(void *data, uint64_t ts);
obs_properties_t *wave_output_properties(void *unused);
void wave_output_defaults(obs_data_t *settings);

static const char *wave_output_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("WAVE.OutputName"); /* "WAVE" */
}

static void *wave_output_create(obs_data_t *settings, obs_output_t *output)
{
	UNUSED_PARAMETER(settings);
	struct wave_output_data *ctx = bzalloc(sizeof(*ctx));
	ctx->output = output;
	ctx->active = false;
	blog(LOG_INFO, "[wave-output] create");
	return ctx;
}

static void wave_output_destroy(void *data)
{
	struct wave_output_data *ctx = data;
	blog(LOG_INFO, "[wave-output] destroy (was_active=%d)", ctx->active);
	bfree(ctx);
}

static bool wave_output_start(void *data)
{
	struct wave_output_data *ctx = data;
	/*
	 * TODO(W2 — task #163): exchange the operator's stream key for a
	 * short-lived gateway JWT, open the SRT socket to
	 * srt://api.wave.online with the JWT in the streamid header, and
	 * begin pushing encoded packets. For W1 this is a no-op so the
	 * service appears in the OBS UI but does not transmit.
	 */
	ctx->active = true;
	blog(LOG_WARNING,
	     "[wave-output] start called — Wave 1 scaffold: no transport yet (see task #163)");
	return true;
}

static void wave_output_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(ts);
	struct wave_output_data *ctx = data;
	ctx->active = false;
	blog(LOG_INFO, "[wave-output] stop");
	obs_output_end_data_capture(ctx->output);
}

void wave_output_register(void)
{
	struct obs_output_info info = {0};
	info.id = "wave-output";
	info.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_SERVICE;
	info.get_name = wave_output_get_name;
	info.create = wave_output_create;
	info.destroy = wave_output_destroy;
	info.start = wave_output_start;
	info.stop = wave_output_stop;
	info.get_properties = wave_output_properties;
	info.get_defaults = wave_output_defaults;
	/*
	 * TODO(W2 — task #163): wire info.raw_video / info.raw_audio (or
	 * the encoded_packet variant for OBS_OUTPUT_ENCODED) once libsrt
	 * push is implemented.
	 */
	obs_register_output(&info);
}
