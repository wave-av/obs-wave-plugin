/*
 * obs-wave-plugin — wave-output output type, public surface.
 *
 * Copyright (C) 2026  WAVE Online LLC
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Registers the wave-output type with libobs. Called once from
 * obs_module_load(). Internally fills an obs_output_info and hands it to
 * obs_register_output().
 */
void wave_output_register(void);

/*
 * Per-instance state held in obs_output_info::create's return pointer and
 * threaded back to every callback. Kept opaque to plugin-main.c.
 *
 * TODO(W2 — task #163): once the SRT push lands, this struct grows the
 * libsrt socket handle, the gateway JWT, and the protobuf encoder arena.
 */
struct wave_output_data;

#ifdef __cplusplus
}
#endif
