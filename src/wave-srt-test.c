/*
 * obs-wave-plugin — unit tests for the wave-srt URL parser.
 *
 * Pure C, no libsrt + no libobs. Run via the CMake target
 * `wave-srt-test` (added in CMakeLists.txt). Catches parser regressions
 * (bad-URL acceptance, scratch-buffer overruns, missing-port defaults)
 * before they reach the OBS encoder thread.
 *
 * Compile: clang -Wall -Wextra -Werror -std=c11 -I src \
 *            -DWAVE_SRT_NO_OBS_LOG -DWAVE_SRT_TEST_PARSER_ONLY \
 *            src/wave-srt-test.c -o wave-srt-test
 *
 * The TEST_PARSER_ONLY define skips wave_srt_open/push/close which need
 * libsrt; only wave_srt_parse_url is exercised here.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "wave-srt.h"

static int failures = 0;

#define EXPECT(cond, msg)                                                  \
	do {                                                               \
		if (!(cond)) {                                             \
			fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__,      \
			        __LINE__, msg);                            \
			failures++;                                        \
		}                                                          \
	} while (0)

static void test_parse_wave_url(void)
{
	char scratch[256];
	struct wave_srt_url u;
	int rc = wave_srt_parse_url("wave://ingest.wave.online:6000", &u,
	                            scratch, sizeof(scratch));
	EXPECT(rc == WAVE_SRT_OK, "wave:// with port should parse");
	EXPECT(strcmp(u.host, "ingest.wave.online") == 0, "host");
	EXPECT(u.port == 6000, "port");
	EXPECT(u.streamid == NULL, "no streamid");
}

static void test_parse_srt_url_no_port(void)
{
	char scratch[256];
	struct wave_srt_url u;
	int rc = wave_srt_parse_url("srt://example.com", &u, scratch,
	                            sizeof(scratch));
	EXPECT(rc == WAVE_SRT_OK, "srt:// no port");
	EXPECT(strcmp(u.host, "example.com") == 0, "host");
	EXPECT(u.port == 6000, "default port");
}

static void test_parse_url_with_streamid(void)
{
	char scratch[256];
	struct wave_srt_url u;
	int rc = wave_srt_parse_url(
	        "wave://ingest.wave.online?streamid=op_jake_2026", &u,
	        scratch, sizeof(scratch));
	EXPECT(rc == WAVE_SRT_OK, "streamid");
	EXPECT(strcmp(u.host, "ingest.wave.online") == 0, "host");
	EXPECT(u.streamid != NULL && strcmp(u.streamid, "op_jake_2026") == 0,
	       "streamid value");
}

static void test_parse_rejects_bare_host(void)
{
	char scratch[256];
	struct wave_srt_url u;
	int rc = wave_srt_parse_url("example.com:6000", &u, scratch,
	                            sizeof(scratch));
	EXPECT(rc == WAVE_SRT_E_BAD_URL, "must require scheme");
}

static void test_parse_rejects_bad_scheme(void)
{
	char scratch[256];
	struct wave_srt_url u;
	int rc = wave_srt_parse_url("http://example.com", &u, scratch,
	                            sizeof(scratch));
	EXPECT(rc == WAVE_SRT_E_BAD_URL, "http:// not accepted");
}

static void test_parse_rejects_empty_host(void)
{
	char scratch[256];
	struct wave_srt_url u;
	int rc = wave_srt_parse_url("wave://", &u, scratch, sizeof(scratch));
	EXPECT(rc == WAVE_SRT_E_BAD_URL, "empty host rejected");
}

static void test_parse_rejects_out_of_range_port(void)
{
	char scratch[256];
	struct wave_srt_url u;
	int rc = wave_srt_parse_url("wave://example.com:99999", &u, scratch,
	                            sizeof(scratch));
	EXPECT(rc == WAVE_SRT_E_BAD_URL, "port > 65535 rejected");
	rc = wave_srt_parse_url("wave://example.com:0", &u, scratch,
	                        sizeof(scratch));
	EXPECT(rc == WAVE_SRT_E_BAD_URL, "port 0 rejected");
}

static void test_parse_handles_tiny_scratch(void)
{
	char scratch[8]; /* too small for "ingest.wave.online" */
	struct wave_srt_url u;
	int rc = wave_srt_parse_url("wave://ingest.wave.online", &u, scratch,
	                            sizeof(scratch));
	EXPECT(rc == WAVE_SRT_E_BAD_URL, "scratch too small rejected (no overrun)");
}

static void test_parse_null_inputs(void)
{
	char scratch[64];
	struct wave_srt_url u;
	EXPECT(wave_srt_parse_url(NULL, &u, scratch, sizeof(scratch)) ==
	               WAVE_SRT_E_BAD_URL,
	       "NULL url");
	EXPECT(wave_srt_parse_url("wave://x", NULL, scratch, sizeof(scratch)) ==
	               WAVE_SRT_E_BAD_URL,
	       "NULL out");
	EXPECT(wave_srt_parse_url("wave://x", &u, NULL, 256) ==
	               WAVE_SRT_E_BAD_URL,
	       "NULL scratch");
}

static void test_strerror_known_codes(void)
{
	EXPECT(strcmp(wave_srt_strerror(WAVE_SRT_OK), "ok") == 0, "ok");
	EXPECT(strstr(wave_srt_strerror(WAVE_SRT_E_BAD_URL), "URL") != NULL,
	       "bad url message contains URL");
	EXPECT(strcmp(wave_srt_strerror(9999), "unknown") == 0, "unknown");
}

int main(void)
{
	test_parse_wave_url();
	test_parse_srt_url_no_port();
	test_parse_url_with_streamid();
	test_parse_rejects_bare_host();
	test_parse_rejects_bad_scheme();
	test_parse_rejects_empty_host();
	test_parse_rejects_out_of_range_port();
	test_parse_handles_tiny_scratch();
	test_parse_null_inputs();
	test_strerror_known_codes();
	if (failures == 0) {
		printf("OK — wave-srt parser tests pass\n");
		return 0;
	}
	fprintf(stderr, "%d test failure(s)\n", failures);
	return 1;
}
