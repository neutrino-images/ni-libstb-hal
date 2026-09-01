/*
 * streaminput_ffmpeg -- AVERROR to stream_error_code_t translation
 * (WORK-231, decision D6)
 *
 * This is the ONLY place where FFmpeg error constants meet the
 * streaminput vocabulary. Include it exclusively from translation
 * units that already compile against libavutil (container_ffmpeg.c,
 * record.cpp, streamts.cpp, streamprobe); the streaminput core itself
 * must stay libav-free because armbox/mipsbox/raspi builds have no
 * AVUTIL_CFLAGS.
 *
 * All constants referenced here exist unchanged from FFmpeg 4.4
 * through 7.x (verified against 4.4.1, 5.1.4, 6.1.1 and master; see
 * WORK-231, version contract D8). Do not add master-only constants
 * such as AVERROR_HTTP_TOO_MANY_REQUESTS.
 *
 * (C) 2026 Thilo Graf
 *
 * License: GPLv2 or later
 */
#ifndef __STREAMINPUT_FFMPEG_H__
#define __STREAMINPUT_FFMPEG_H__

#include <errno.h>
/* stdint.h first: libavutil/common.h refuses C++ builds unless the
 * C99 constant macros are visible (UINT64_C), and C++11 stdint.h
 * provides them unconditionally. */
#include <stdint.h>

/* libav headers carry no extern "C" of their own, so a C++ translation
 * unit that reaches them through this header must wrap them -- same
 * pattern as record.cpp. common.h before error.h: in FFmpeg 4.4
 * error.h uses MKTAG without pulling in its definition itself (fixed
 * upstream in 5.x). */
#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/common.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#ifdef __cplusplus
}
#endif

#include <streaminput.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline stream_error_code_t streaminput_error_from_averror(int averror)
{
	if (averror >= 0)
		return STREAM_ERR_NONE;

	switch (averror)
	{
		case AVERROR(ECONNRESET):
			return STREAM_ERR_CONNECTION_RESET;
		case AVERROR(ECONNREFUSED):
		case AVERROR(EHOSTUNREACH):
		case AVERROR(ENETUNREACH):
			return STREAM_ERR_CONNECTION_FAILED;
		case AVERROR(ETIMEDOUT):
			return STREAM_ERR_TIMED_OUT;
		case AVERROR(EIO):
			return STREAM_ERR_IO;
		case AVERROR_INVALIDDATA:
			return STREAM_ERR_INVALID_DATA;
		case AVERROR_EXIT:
			return STREAM_ERR_EXIT_REQUESTED;
		case AVERROR_EOF:
			return STREAM_ERR_EOF;
		case AVERROR_HTTP_BAD_REQUEST:
			return STREAM_ERR_HTTP_BAD_REQUEST;
		case AVERROR_HTTP_UNAUTHORIZED:
			return STREAM_ERR_HTTP_UNAUTHORIZED;
		case AVERROR_HTTP_FORBIDDEN:
			return STREAM_ERR_HTTP_FORBIDDEN;
		case AVERROR_HTTP_NOT_FOUND:
			return STREAM_ERR_HTTP_NOT_FOUND;
		case AVERROR_HTTP_OTHER_4XX:
			return STREAM_ERR_HTTP_OTHER_4XX;
		case AVERROR_HTTP_SERVER_ERROR:
			return STREAM_ERR_HTTP_SERVER_ERROR;
		case AVERROR_PROTOCOL_NOT_FOUND:
			return STREAM_ERR_PROTOCOL_NOT_FOUND;
		default:
			return STREAM_ERR_UNKNOWN;
	}
}

/* Convenience: classify an AVERROR directly (no separate HTTP status
 * available at the call sites this serves). */
static inline stream_failure_class_t streaminput_classify_averror(int averror)
{
	return streaminput_classify(streaminput_error_from_averror(averror), 0);
}

/* Feed a policy result into an AVDictionary. Returns 0 when every entry
 * was stored, otherwise the first av_dict_set() error (AVERROR(ENOMEM)
 * in practice). The remaining entries are still attempted, matching the
 * historic blocks' independent av_dict_set() calls: dict ends up with
 * every entry that could be stored. Those blocks discarded the return
 * values and could not tell a complete policy from a half-applied one;
 * this module reports the first error and leaves the decision to the
 * caller. */
static inline int streaminput_kv_to_avdict(const stream_kv_list_t *opts, AVDictionary **dict)
{
	size_t i;
	int ret, err = 0;

	if (!opts || !dict)
		return -1;
	for (i = 0; i < opts->count; i++)
	{
		ret = av_dict_set(dict, opts->items[i].key, opts->items[i].value, 0);
		if (ret < 0 && err == 0)
			err = ret;
	}
	return err;
}

/* One-call form for the plain "url + resolver headers" call sites that
 * have no stream_source_t of their own yet: build the profile options
 * and put them into dict. headers may be NULL or empty.
 *
 * Returns 0 on success. A negative return means the policy did NOT
 * arrive in full: -1 if the options could not be built at all (dict
 * untouched), or the first av_dict_set() error if storing ran out of
 * memory partway (dict holds every entry that could be stored). Either
 * way the open that follows would use an incomplete policy, so callers
 * are expected to look at the return value rather than drop it. */
static inline int streaminput_apply_policy(const char *url, const char *headers, stream_input_profile_t profile, AVDictionary **dict)
{
	stream_source_t source;
	stream_kv_list_t opts;
	int ret;

	streaminput_source_init(&source);
	streaminput_kv_init(&opts);

	ret = streaminput_source_set_url(&source, url);
	if (ret == 0)
		ret = streaminput_source_set_headers(&source, headers);
	if (ret == 0)
		ret = streaminput_policy_build(&source, profile, &opts);
	if (ret == 0)
		ret = streaminput_kv_to_avdict(&opts, dict);

	streaminput_kv_free(&opts);
	streaminput_source_free(&source);
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* __STREAMINPUT_FFMPEG_H__ */
