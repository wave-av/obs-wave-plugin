/*
 * obs-wave-plugin — stub implementations for the wave-srt symbols that
 * touch libsrt. Linked into the parser-only unit test binary so we don't
 * need a live libsrt on the test host.
 *
 * Only the parser is exercised by wave-srt-test.c; these stubs return
 * benign values for the other entry points so the test binary links.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wave-srt.h"

int wave_srt_open(const char *url, const char *stream_key)
{
	(void)url;
	(void)stream_key;
	return WAVE_SRT_E_LIBSRT;
}

int wave_srt_push(const uint8_t *buf, size_t len)
{
	(void)buf;
	(void)len;
	return WAVE_SRT_E_CLOSED;
}

void wave_srt_close(void)
{
}
