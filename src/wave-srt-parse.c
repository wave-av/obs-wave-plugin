/*
 * obs-wave-plugin — wave-srt URL parser + error-string helpers.
 *
 * Split from wave-srt.c so the parser + strerror are stdlib-only and
 * unit-testable without libsrt or libobs. The libsrt-dependent open /
 * push / close lives in wave-srt.c.
 *
 * Copyright (C) 2026  WAVE Online LLC
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wave-srt.h"

#include <stdlib.h>
#include <string.h>

int wave_srt_parse_url(const char *url, struct wave_srt_url *out, char *scratch, size_t scratch_len)
{
	if (!url || !out || !scratch || scratch_len < 8)
		return WAVE_SRT_E_BAD_URL;

	/* Accept wave://… and srt://… — anything else fails. We deliberately
	 * do NOT accept bare host:port to avoid silent protocol guessing. */
	const char *rest = NULL;
	if (strncmp(url, "wave://", 7) == 0)
		rest = url + 7;
	else if (strncmp(url, "srt://", 6) == 0)
		rest = url + 6;
	else
		return WAVE_SRT_E_BAD_URL;

	/* Copy authority + query into scratch so we can null-terminate
	 * components without mutating the caller's buffer. */
	size_t rest_len = strlen(rest);
	if (rest_len + 1 > scratch_len)
		return WAVE_SRT_E_BAD_URL;
	memcpy(scratch, rest, rest_len + 1);

	out->host = scratch;
	out->port = 6000;
	out->streamid = NULL;

	/* Split on '?' to peel off the query. We support a single `streamid=`
	 * pair anywhere in the query string. The value is terminated at the
	 * next `&` (or end-of-string) so multi-param URLs like
	 * `wave://host?streamid=foo&mode=live` don't bleed the trailing
	 * params into the streamid passed to libsrt. (CR PR#5) */
	char *q = strchr(scratch, '?');
	if (q) {
		*q = '\0';
		char *params = q + 1;
		char *sid = strstr(params, "streamid=");
		if (sid) {
			out->streamid = sid + 9;
			char *amp = strchr(out->streamid, '&');
			if (amp)
				*amp = '\0';
		}
	}

	/* Split on ':' for the port. IPv6 (bracketed) is not yet supported —
	 * task #173 documented that pattern for wave-desktop; the next-cycle
	 * port over here will reuse it. */
	char *colon = strchr(scratch, ':');
	if (colon) {
		*colon = '\0';
		long p = strtol(colon + 1, NULL, 10);
		if (p < 1 || p > 65535)
			return WAVE_SRT_E_BAD_URL;
		out->port = (uint16_t)p;
	}

	if (out->host[0] == '\0')
		return WAVE_SRT_E_BAD_URL;

	return WAVE_SRT_OK;
}

const char *wave_srt_strerror(int code)
{
	switch (code) {
	case WAVE_SRT_OK:           return "ok";
	case WAVE_SRT_E_BAD_URL:    return "invalid URL or arguments";
	case WAVE_SRT_E_CONNECT:    return "connect failed";
	case WAVE_SRT_E_WOULDBLOCK: return "send buffer full — frame dropped";
	case WAVE_SRT_E_CLOSED:     return "socket not open";
	case WAVE_SRT_E_LIBSRT:     return "libsrt internal error";
	default:                    return "unknown";
	}
}
