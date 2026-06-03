/*
 * obs-wave-plugin — SRT push transport (libsrt wrapper, implementation).
 *
 * State machine: there is exactly one active socket at a time, held in a
 * module-private `g_state` struct. wave_srt_open() refuses to clobber a
 * live socket — the caller must close() first. This keeps the OBS output
 * lifecycle (one stream at a time per output instance) honest and means
 * the encoder thread never races against open/close on a different socket.
 *
 * libsrt linkage: we use the C API only (srt.h) — no SRT::SocketGroup,
 * no live-bonding. Those are post-MVP and pull in libsrt's C++ surface.
 *
 * Copyright (C) 2026  WAVE Online LLC
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wave-srt.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <srt/srt.h>

/* OBS log helper — keeps wave-srt.c self-contained for unit tests by
 * falling back to fprintf when obs-module.h isn't included (the test
 * harness defines WAVE_SRT_NO_OBS_LOG). */
#ifndef WAVE_SRT_NO_OBS_LOG
#include <obs-module.h>
#define WSRT_LOG(level, fmt, ...) blog(level, "[wave-srt] " fmt, ##__VA_ARGS__)
#else
#define WSRT_LOG(level, fmt, ...) fprintf(stderr, "[wave-srt] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3
#endif

/* Module-private state. pthread_mutex guards the socket handle against the
 * tiny race where OBS calls stop() while the encoder thread is mid-push. */
static struct {
	SRTSOCKET sock;
	pthread_mutex_t lock;
	bool initialized;
} g_state = {SRT_INVALID_SOCK, PTHREAD_MUTEX_INITIALIZER, false};

static int ensure_srt_startup(void)
{
	if (g_state.initialized)
		return WAVE_SRT_OK;
	if (srt_startup() != 0) {
		WSRT_LOG(LOG_ERROR, "srt_startup failed: %s", srt_getlasterror_str());
		return WAVE_SRT_E_LIBSRT;
	}
	g_state.initialized = true;
	return WAVE_SRT_OK;
}

/* wave_srt_parse_url and wave_srt_strerror live in wave-srt-parse.c so
 * they can be unit-tested without linking libsrt. See that file + the
 * wave-srt-test.c harness. */

int wave_srt_open(const char *url, const char *stream_key)
{
	if (!url || !stream_key)
		return WAVE_SRT_E_BAD_URL;

	int rc = ensure_srt_startup();
	if (rc != WAVE_SRT_OK)
		return rc;

	/* Refuse to clobber a live socket — caller must close() first. */
	pthread_mutex_lock(&g_state.lock);
	if (g_state.sock != SRT_INVALID_SOCK) {
		pthread_mutex_unlock(&g_state.lock);
		WSRT_LOG(LOG_WARNING, "open called with active socket; close first");
		return WAVE_SRT_E_CONNECT;
	}
	pthread_mutex_unlock(&g_state.lock);

	char scratch[1024];
	struct wave_srt_url parsed;
	rc = wave_srt_parse_url(url, &parsed, scratch, sizeof(scratch));
	if (rc != WAVE_SRT_OK) {
		WSRT_LOG(LOG_ERROR, "bad URL: %s", url);
		return rc;
	}

	/* Compose the final streamid: query param wins if present, else
	 * stream_key — but always validated against MAX_STREAMID. */
	char final_streamid[WAVE_SRT_MAX_STREAMID + 1];
	const char *src = parsed.streamid ? parsed.streamid : stream_key;
	if (strlen(src) > WAVE_SRT_MAX_STREAMID) {
		WSRT_LOG(LOG_ERROR, "streamid too long (%zu > %d)",
		         strlen(src), WAVE_SRT_MAX_STREAMID);
		return WAVE_SRT_E_BAD_URL;
	}
	snprintf(final_streamid, sizeof(final_streamid), "%s", src);

	SRTSOCKET s = srt_create_socket();
	if (s == SRT_INVALID_SOCK) {
		WSRT_LOG(LOG_ERROR, "srt_create_socket: %s", srt_getlasterror_str());
		return WAVE_SRT_E_LIBSRT;
	}

	/* Caller mode + non-blocking send + low-latency live preset.
	 * SRTO_LATENCY=120ms matches the wave-desktop encoder default. */
	int latency = 120;
	int payload_size = 1316; /* MPEG-TS 7 packets * 188B */
	bool sndsyn = false;
	srt_setsockflag(s, SRTO_PAYLOADSIZE, &payload_size, sizeof(payload_size));
	srt_setsockflag(s, SRTO_LATENCY, &latency, sizeof(latency));
	srt_setsockflag(s, SRTO_SNDSYN, &sndsyn, sizeof(sndsyn));
	srt_setsockflag(s, SRTO_STREAMID, final_streamid,
	                (int)strlen(final_streamid));

	/* Resolve host. We use libsrt's resolver via srt_connect with a
	 * sockaddr filled from getaddrinfo, so we get DNS for free. */
	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_port = htons(parsed.port);
	/* For simplicity in this MVP, expect dotted-quad or DNS that
	 * resolves to v4. v6 deferred — task #173's bracketed-v6 fix in
	 * wave-desktop is the next-cycle pattern to port here. */
	struct addrinfo hints = {0}, *ai = NULL;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if (getaddrinfo(parsed.host, NULL, &hints, &ai) != 0 || !ai) {
		WSRT_LOG(LOG_ERROR, "DNS failed for %s", parsed.host);
		srt_close(s);
		return WAVE_SRT_E_CONNECT;
	}
	memcpy(&sa.sin_addr, &((struct sockaddr_in *)ai->ai_addr)->sin_addr,
	       sizeof(sa.sin_addr));
	freeaddrinfo(ai);

	if (srt_connect(s, (struct sockaddr *)&sa, sizeof(sa)) == SRT_ERROR) {
		WSRT_LOG(LOG_ERROR, "srt_connect failed: %s", srt_getlasterror_str());
		srt_close(s);
		return WAVE_SRT_E_CONNECT;
	}

	pthread_mutex_lock(&g_state.lock);
	g_state.sock = s;
	pthread_mutex_unlock(&g_state.lock);

	WSRT_LOG(LOG_INFO, "connected to %s:%u (streamid=%zuB)",
	         parsed.host, parsed.port, strlen(final_streamid));
	return WAVE_SRT_OK;
}

int wave_srt_push(const uint8_t *buf, size_t len)
{
	if (!buf || len == 0)
		return WAVE_SRT_E_BAD_URL;

	pthread_mutex_lock(&g_state.lock);
	SRTSOCKET s = g_state.sock;
	pthread_mutex_unlock(&g_state.lock);

	if (s == SRT_INVALID_SOCK)
		return WAVE_SRT_E_CLOSED;

	int sent = srt_send(s, (const char *)buf, (int)len);
	if (sent == SRT_ERROR) {
		int rej = srt_getrejectreason(s);
		int errcode = srt_getlasterror(NULL);
		/* SRT_EASYNCSND maps to our WOULDBLOCK; caller drops the frame. */
		if (errcode == SRT_EASYNCSND)
			return WAVE_SRT_E_WOULDBLOCK;
		WSRT_LOG(LOG_ERROR, "srt_send failed: %s (reject=%d)",
		         srt_getlasterror_str(), rej);
		return WAVE_SRT_E_LIBSRT;
	}
	return WAVE_SRT_OK;
}

void wave_srt_close(void)
{
	pthread_mutex_lock(&g_state.lock);
	SRTSOCKET s = g_state.sock;
	g_state.sock = SRT_INVALID_SOCK;
	pthread_mutex_unlock(&g_state.lock);

	if (s != SRT_INVALID_SOCK) {
		srt_close(s);
		WSRT_LOG(LOG_INFO, "socket closed");
	}
}
