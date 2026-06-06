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
		WSRT_LOG(LOG_ERROR, "streamid too long (%zu > %d)", strlen(src), WAVE_SRT_MAX_STREAMID);
		return WAVE_SRT_E_BAD_URL;
	}
	snprintf(final_streamid, sizeof(final_streamid), "%s", src);

	SRTSOCKET s = srt_create_socket();
	if (s == SRT_INVALID_SOCK) {
		WSRT_LOG(LOG_ERROR, "srt_create_socket: %s", srt_getlasterror_str());
		return WAVE_SRT_E_LIBSRT;
	}

	/* Pre-connect socket options. We DO NOT flip SRTO_SNDSYN here:
	 *   - SRTO_SNDSYN governs both connect() AND send(). Setting it false
	 *     before connect would make srt_connect() return success before
	 *     the handshake completes, then OBS would start sending into a
	 *     half-open socket and the first frames would fail (Sentry HIGH
	 *     on PR#5). The fix is to leave SNDSYN at its default (true)
	 *     across connect and flip it to false only after the handshake
	 *     returns success.
	 * SRTO_LATENCY=120ms matches the wave-desktop encoder default. */
	int latency = 120;
	int payload_size = 1316; /* MPEG-TS 7 packets * 188B */
#define SRT_SETOPT_OR_FAIL(opt, valp, len, name)                          \
		do {                                                              \
			if (srt_setsockflag(s, (opt), (valp), (len)) == SRT_ERROR) { \
				WSRT_LOG(LOG_ERROR,                               \
				         "srt_setsockflag(" name "): %s",         \
				         srt_getlasterror_str());                 \
				srt_close(s);                                     \
				return WAVE_SRT_E_LIBSRT;                         \
			}                                                         \
		} while (0)
	SRT_SETOPT_OR_FAIL(SRTO_PAYLOADSIZE, &payload_size, sizeof(payload_size), "SRTO_PAYLOADSIZE");
	SRT_SETOPT_OR_FAIL(SRTO_LATENCY, &latency, sizeof(latency), "SRTO_LATENCY");
	SRT_SETOPT_OR_FAIL(SRTO_STREAMID, final_streamid, (int)strlen(final_streamid), "SRTO_STREAMID");
#undef SRT_SETOPT_OR_FAIL

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
	memcpy(&sa.sin_addr, &((struct sockaddr_in *)ai->ai_addr)->sin_addr, sizeof(sa.sin_addr));
	freeaddrinfo(ai);

	if (srt_connect(s, (struct sockaddr *)&sa, sizeof(sa)) == SRT_ERROR) {
		WSRT_LOG(LOG_ERROR, "srt_connect failed: %s", srt_getlasterror_str());
		srt_close(s);
		return WAVE_SRT_E_CONNECT;
	}

	/* Post-connect: now that the handshake completed successfully, flip
	 * sends to non-blocking so the OBS encoder thread never stalls. We
	 * also flip RCVSYN false for symmetry (we never read, but if a future
	 * keepalive does, it must not block either). If the post-connect
	 * setsockflag fails, treat as a hard error — we'd otherwise leak a
	 * misconfigured live socket. (Sentry HIGH + CR Major PR#5) */
	bool sndsyn = false;
	bool rcvsyn = false;
	if (srt_setsockflag(s, SRTO_SNDSYN, &sndsyn, sizeof(sndsyn)) == SRT_ERROR ||
	    srt_setsockflag(s, SRTO_RCVSYN, &rcvsyn, sizeof(rcvsyn)) == SRT_ERROR) {
		WSRT_LOG(LOG_ERROR, "post-connect SNDSYN/RCVSYN flip failed: %s", srt_getlasterror_str());
		srt_close(s);
		return WAVE_SRT_E_LIBSRT;
	}

	pthread_mutex_lock(&g_state.lock);
	g_state.sock = s;
	pthread_mutex_unlock(&g_state.lock);

	WSRT_LOG(LOG_INFO, "connected to %s:%u (streamid=%zuB)", parsed.host, parsed.port, strlen(final_streamid));
	return WAVE_SRT_OK;
}

int wave_srt_push(const uint8_t *buf, size_t len)
{
	if (!buf || len == 0)
		return WAVE_SRT_E_BAD_URL;

	/* Hold the lock for the entire srt_send() critical section, not just
	 * the handle read. Otherwise wave_srt_close() can tear down the
	 * socket between the snapshot and the send, which is exactly the
	 * race the mutex was meant to prevent (CR Critical PR#5). Sends are
	 * non-blocking post-connect (SRTO_SNDSYN=false) so this lock is
	 * bounded — libsrt returns SRT_EASYNCSND immediately on buffer full. */
	pthread_mutex_lock(&g_state.lock);
	SRTSOCKET s = g_state.sock;
	if (s == SRT_INVALID_SOCK) {
		pthread_mutex_unlock(&g_state.lock);
		return WAVE_SRT_E_CLOSED;
	}

	int sent = srt_send(s, (const char *)buf, (int)len);
	int rej = 0;
	int errcode = 0;
	if (sent == SRT_ERROR) {
		rej = srt_getrejectreason(s);
		errcode = srt_getlasterror(NULL);
	}
	pthread_mutex_unlock(&g_state.lock);

	if (sent != SRT_ERROR)
		return WAVE_SRT_OK;

	/* SRT_EASYNCSND maps to our WOULDBLOCK; caller drops the frame. */
	if (errcode == SRT_EASYNCSND)
		return WAVE_SRT_E_WOULDBLOCK;
	WSRT_LOG(LOG_ERROR, "srt_send failed: %s (reject=%d)", srt_getlasterror_str(), rej);
	return WAVE_SRT_E_LIBSRT;
}

void wave_srt_close(void)
{
	/* Hold the lock across srt_close() too. If we released the lock
	 * before srt_close(), a wave_srt_push() running on the encoder
	 * thread could still be inside srt_send() with the same handle.
	 * The push path takes the same lock, so this serializes them
	 * cleanly (CR Critical PR#5). */
	pthread_mutex_lock(&g_state.lock);
	SRTSOCKET s = g_state.sock;
	g_state.sock = SRT_INVALID_SOCK;
	if (s != SRT_INVALID_SOCK) {
		srt_close(s);
		WSRT_LOG(LOG_INFO, "socket closed");
	}
	pthread_mutex_unlock(&g_state.lock);
}
