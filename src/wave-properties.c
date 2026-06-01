/*
 * obs-wave-plugin — Settings → Stream → Service UI properties.
 *
 * Three fields exposed in OBS:
 *   · gateway URL  (default srt://api.wave.online)
 *   · stream key   (per-event, from wave.online/console; password-masked)
 *   · codec        (H.264 / HEVC / AV1; AV2 reserved for Wave 2)
 *
 * Copyright (C) 2026  WAVE Online LLC
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <obs-module.h>

#include "wave-output.h"

#define WAVE_DEFAULT_GATEWAY "srt://api.wave.online"
#define WAVE_DEFAULT_CODEC   "h264"

obs_properties_t *wave_output_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "gateway_url",
	                        obs_module_text("WAVE.GatewayURL"),
	                        OBS_TEXT_DEFAULT);

	obs_properties_add_text(props, "stream_key",
	                        obs_module_text("WAVE.StreamKey"),
	                        OBS_TEXT_PASSWORD);

	obs_property_t *codec = obs_properties_add_list(
		props, "codec", obs_module_text("WAVE.Codec"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(codec, "H.264 (AVC)", "h264");
	obs_property_list_add_string(codec, "HEVC (H.265)", "hevc");
	obs_property_list_add_string(codec, "AV1",          "av1");
	/*
	 * TODO(W2 — task #163): expose AV2 once ffmpeg 8.2 lands
	 * `--enable-libavm` (tracked in task #170). Reserved here so the
	 * settings schema is stable.
	 */

	return props;
}

void wave_output_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "gateway_url",
	                            WAVE_DEFAULT_GATEWAY);
	obs_data_set_default_string(settings, "codec", WAVE_DEFAULT_CODEC);
}
