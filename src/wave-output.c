/*
 * obs-wave-plugin — wave-output output type.
 *
 * Lifecycle:
 *   create  → bzalloc ctx
 *   start   → read settings (gateway_url, stream_key) and open libsrt
 *             via wave_srt_open(). On success, call
 *             obs_output_begin_data_capture() so libobs starts forwarding
 *             encoded packets.
 *   encoded_packet → push to libsrt via wave_srt_push(). On
 *             WAVE_SRT_E_WOULDBLOCK drop the frame; on any other negative
 *             code, stop the output with OBS_OUTPUT_DISCONNECTED so the
 *             reconnect logic in libobs surfaces it to the operator.
 *   stop    → wave_srt_close() + obs_output_end_data_capture()
 *
 * Threading: OBS calls encoded_packet on its encoder thread; wave_srt
 * uses non-blocking sends so we never stall that thread.
 *
 * Copyright (C) 2026  WAVE Online LLC
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <obs-module.h>
#include <util/bmem.h>

#include "wave-output.h"
#include "wave-srt.h"

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
static void wave_output_encoded_packet(void *data, struct encoder_packet *packet);
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
	/* Defensive — destroy is supposed to be called only after stop, but
	 * close is idempotent and cheaper than a leaked socket. */
	if (ctx->active)
		wave_srt_close();
	bfree(ctx);
}

static bool wave_output_start(void *data)
{
	struct wave_output_data *ctx = data;
	obs_data_t *settings = obs_output_get_settings(ctx->output);
	const char *gateway = obs_data_get_string(settings, "gateway_url");
	const char *stream_key = obs_data_get_string(settings, "stream_key");

	if (!stream_key || stream_key[0] == '\0') {
		obs_output_set_last_error(
			ctx->output,
			obs_module_text("WAVE.Error.MissingStreamKey"));
		obs_data_release(settings);
		blog(LOG_ERROR, "[wave-output] start refused: empty stream key");
		return false;
	}

	int rc = wave_srt_open(gateway, stream_key);
	obs_data_release(settings);
	if (rc != WAVE_SRT_OK) {
		obs_output_set_last_error(ctx->output, wave_srt_strerror(rc));
		blog(LOG_ERROR, "[wave-output] start failed: %s",
		     wave_srt_strerror(rc));
		return false;
	}

	ctx->active = true;
	if (!obs_output_can_begin_data_capture(ctx->output, 0)) {
		blog(LOG_ERROR,
		     "[wave-output] obs_output_can_begin_data_capture refused");
		wave_srt_close();
		ctx->active = false;
		return false;
	}
	obs_output_begin_data_capture(ctx->output, 0);
	blog(LOG_INFO, "[wave-output] live");
	return true;
}

static void wave_output_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(ts);
	struct wave_output_data *ctx = data;
	if (!ctx->active)
		return;
	ctx->active = false;
	wave_srt_close();
	obs_output_end_data_capture(ctx->output);
	blog(LOG_INFO, "[wave-output] stopped");
}

static void wave_output_encoded_packet(void *data, struct encoder_packet *packet)
{
	struct wave_output_data *ctx = data;
	if (!ctx->active || !packet || !packet->data || packet->size == 0)
		return;

	int rc = wave_srt_push(packet->data, packet->size);
	switch (rc) {
	case WAVE_SRT_OK:
		return;
	case WAVE_SRT_E_WOULDBLOCK:
		/* Send buffer full — drop the frame, log infrequently. The
		 * encoder thread MUST keep moving; backpressure into libobs
		 * here would stall the capture pipeline. */
		blog(LOG_WARNING,
		     "[wave-output] backpressure — dropped %zu-byte packet",
		     packet->size);
		return;
	default:
		/* Hard error — surface to OBS so its reconnect logic / UI
		 * shows the failure rather than streaming into the void. */
		blog(LOG_ERROR, "[wave-output] push failed: %s",
		     wave_srt_strerror(rc));
		ctx->active = false;
		obs_output_signal_stop(ctx->output, OBS_OUTPUT_DISCONNECTED);
		wave_srt_close();
		return;
	}
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
	info.encoded_packet = wave_output_encoded_packet;
	obs_register_output(&info);
}
