/*
 * obs-wave-plugin — module entry points.
 *
 * Copyright (C) 2026  WAVE Online LLC
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * libobs is GPL-2; this derivative work inherits the license. See LICENSE.
 */

#include <obs-module.h>

#include "wave-output.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("wave-output", "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return "WAVE Streaming Service";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Push OBS encoded output directly to api.wave.online via SRT — "
	       "no re-stream relay. See https://github.com/wave-av/obs-wave-plugin.";
}

/*
 * obs_module_load() runs on plugin load (OBS startup).
 *
 * Registers the wave-output output type with libobs so it appears under
 * Settings → Stream → Service as "WAVE".
 */
bool obs_module_load(void)
{
	blog(LOG_INFO, "[wave-output] loading plugin v0.1.0");

	wave_output_register();

	blog(LOG_INFO, "[wave-output] registered output type 'wave-output'");
	return true;
}

/*
 * obs_module_unload() runs on plugin unload (OBS shutdown).
 *
 * No global state to tear down yet — every wave_output_data instance owns
 * its own lifetime through the obs_output_info::create / destroy callbacks.
 */
void obs_module_unload(void)
{
	blog(LOG_INFO, "[wave-output] unloading plugin");
}
