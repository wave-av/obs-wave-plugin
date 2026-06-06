/*
 * obs-wave-plugin — SRT push transport (libsrt wrapper).
 *
 * Pure C wrapper around libsrt. Does not touch libobs — that decoupling
 * means we can unit-test the URL parser + state machine without an OBS
 * runtime, and it keeps the libsrt link surface to a single translation
 * unit so a missing/incompatible libsrt only breaks this file.
 *
 * Lifecycle:
 *   wave_srt_open(url, stream_key)   → opens caller socket; returns 0 on success
 *   wave_srt_push(buf, len)          → enqueues a TS packet
 *   wave_srt_close()                 → graceful close, flushes pending writes
 *
 * Threading model: wave_srt_push() is called from libobs's encoder thread.
 * libsrt's send is non-blocking (via SRTO_SNDSYN=false), so push returns
 * immediately and pending packets are queued in libsrt's internal buffer.
 * If the buffer fills, push returns WAVE_SRT_E_WOULDBLOCK and the caller
 * should drop the frame rather than block the encoder thread.
 *
 * Copyright (C) 2026  WAVE Online LLC
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Public error codes. Negative because libsrt's SRT_ERROR is -1; we map
 * specific failure modes to distinct codes so the caller can surface
 * actionable UI strings instead of a generic "send failed". */
#define WAVE_SRT_OK            0
#define WAVE_SRT_E_BAD_URL    -1
#define WAVE_SRT_E_CONNECT    -2
#define WAVE_SRT_E_WOULDBLOCK -3
#define WAVE_SRT_E_CLOSED     -4
#define WAVE_SRT_E_LIBSRT     -5

/* Maximum streamid header length (libsrt enforces 512 bytes; we cap lower
 * for our own URL composition + safety margin). */
#define WAVE_SRT_MAX_STREAMID 256

/*
 * Parsed view of a wave:// or srt:// URL. Output of wave_srt_parse_url();
 * input to wave_srt_open() if the caller pre-parses (e.g. for the
 * test suite). Pointers reference the input buffer — do NOT free.
 */
struct wave_srt_url {
	const char *host;     /* zero-terminated; lives in input arena */
	uint16_t port;        /* 6000 default if absent */
	const char *streamid; /* may be NULL — overridden by stream_key */
};

/*
 * Open a connection. `url` accepts `srt://<host>[:<port>][?streamid=<id>]`
 * and `wave://<host>[:<port>]`; the latter is the recommended form for the
 * obs-wave Settings UI because it makes the protocol choice explicit. The
 * `stream_key` argument is appended as `streamid=<key>` in the SRT handshake.
 *
 * Returns one of the WAVE_SRT_* codes. On success the module owns the
 * socket; the caller MUST eventually call wave_srt_close().
 */
int wave_srt_open(const char *url, const char *stream_key);

/*
 * Push a single MPEG-TS packet (or aggregated burst). libsrt expects each
 * sendmsg to be a complete payload; the caller is responsible for upstream
 * muxing (the OBS encoded_packet path provides this for free).
 *
 * Returns WAVE_SRT_OK or one of the negative error codes. On
 * WAVE_SRT_E_WOULDBLOCK the encoder thread should drop the frame; on
 * any other negative code the socket is dead and a re-open is required.
 */
int wave_srt_push(const uint8_t *buf, size_t len);

/*
 * Close the active socket. Safe to call multiple times; second + later
 * calls are no-ops. Blocks for up to ~500ms while libsrt flushes pending
 * payloads (SRTO_LINGER), then returns.
 */
void wave_srt_close(void);

/*
 * Test-exposed parser. Splits a wave:// or srt:// URL into host/port/streamid.
 * `out->streamid` may be NULL when no `?streamid=` query is present. The
 * lifetime of the pointers in `out` matches `url`. Returns WAVE_SRT_OK or
 * WAVE_SRT_E_BAD_URL.
 *
 * Visible publicly so the unit tests (which don't link libsrt) can pin the
 * parser shape independent of the socket layer.
 */
int wave_srt_parse_url(const char *url, struct wave_srt_url *out, char *scratch, size_t scratch_len);

/* Human-readable error string for diagnostics / UI surfacing. Never returns
 * NULL — unknown codes map to "unknown". */
const char *wave_srt_strerror(int code);

#ifdef __cplusplus
}
#endif
