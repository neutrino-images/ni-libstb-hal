/*
 * streaminput -- shared stream input core (WORK-231, spec phase 1/2)
 *
 * Describes a network stream source, classifies transport failures and
 * builds the FFmpeg input options for it in ONE place, so that the
 * player (libeplayer3), the app-side relay paths (record/streamts) and
 * the streamprobe tool stop carrying their own copies.
 *
 * Deliberate properties of this module:
 *
 *  - Plain C (C99), no exceptions, no RTTI: callers are C compiled with
 *    -fno-exceptions (libeplayer3) as well as C++ (neutrino).
 *  - NO libav includes and no libav linkage. On armbox/mipsbox/raspi
 *    the build has no AVUTIL_CFLAGS, so this header must stand alone.
 *    The translation from AVERROR codes to stream_error_code_t lives in
 *    streaminput_ffmpeg.h, which only TUs that already use libavutil
 *    may include.
 *  - Stateless and reentrant: no globals, no statics. All state lives
 *    in caller-owned structs. This module replaces a process-global
 *    option dictionary; do not reintroduce one here.
 *  - Ownership: streaminput_source_init() puts a struct into a defined
 *    empty state; set/put functions copy their arguments (malloc);
 *    streaminput_source_free()/streaminput_kv_free() release everything
 *    and re-init. Plain int returns: 0 on success, -1 on allocation
 *    failure (the struct stays valid and freeable).
 *  - Headers stay an OPAQUE string for now. Resolvers deliver a
 *    preformatted header blob (livestream_info_t::header) and the specs
 *    plugin-compat rules forbid mangling it; a structured header model
 *    arrives with the Start() options object (D4, phase 5).
 *
 * (C) 2026 Thilo Graf
 *
 * License: GPLv2 or later
 */
#ifndef __STREAMINPUT_H__
#define __STREAMINPUT_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stages of a deterministic stream open (spec section 7). Purely
 * descriptive: used to tag log lines and failure reports. */
typedef enum
{
	STREAM_STAGE_RESOLVE = 0,
	STREAM_STAGE_REDIRECT,
	STREAM_STAGE_CONNECT,
	STREAM_STAGE_OPEN,
	STREAM_STAGE_PROBE,
	STREAM_STAGE_PLAYBACK
} stream_stage_t;

/* Transport error codes, this module's own vocabulary (decision D6).
 * streaminput_ffmpeg.h translates AVERROR values into these; nothing
 * in this header depends on FFmpeg. */
typedef enum
{
	STREAM_ERR_NONE = 0,
	STREAM_ERR_CONNECTION_RESET,
	/* Connection never came up: refused, host or net unreachable. The
	 * most common real failure once a resolver hands over a dead
	 * endpoint, so it gets its own code rather than falling to
	 * STREAM_ERR_UNKNOWN. */
	STREAM_ERR_CONNECTION_FAILED,
	STREAM_ERR_TIMED_OUT,
	STREAM_ERR_IO,
	STREAM_ERR_INVALID_DATA,
	STREAM_ERR_EXIT_REQUESTED,
	STREAM_ERR_EOF,
	STREAM_ERR_HTTP_BAD_REQUEST,
	STREAM_ERR_HTTP_UNAUTHORIZED,
	STREAM_ERR_HTTP_FORBIDDEN,
	STREAM_ERR_HTTP_NOT_FOUND,
	STREAM_ERR_HTTP_OTHER_4XX,
	STREAM_ERR_HTTP_SERVER_ERROR,
	STREAM_ERR_PROTOCOL_NOT_FOUND,
	STREAM_ERR_UNKNOWN
} stream_error_code_t;

/* Failure classes (spec section 8) -- TRANSPORT SUBSET ONLY.
 * Resolver, DNS and user-intent outcomes (resolver failed, DNS failed,
 * user abort, ...) cannot be derived from a transport error code plus
 * HTTP status and stay composed by the caller (decision D3). */
typedef enum
{
	STREAM_FAILURE_NONE = 0,
	STREAM_FAILURE_CONNECTION_FAILED,
	STREAM_FAILURE_CONNECTION_TIMEOUT,
	STREAM_FAILURE_HTTP_4XX,
	STREAM_FAILURE_HTTP_5XX,
	STREAM_FAILURE_INVALID_MANIFEST,
	STREAM_FAILURE_UNSUPPORTED_PROTOCOL,
	STREAM_FAILURE_TEMPORARY_NETWORK,
	STREAM_FAILURE_END_OF_STREAM,
	STREAM_FAILURE_ABORTED,
	STREAM_FAILURE_UNKNOWN
} stream_failure_class_t;

/* Coarse protocol classification of a URL (spec section 2). */
typedef enum
{
	STREAM_PROTOCOL_UNKNOWN = 0,
	STREAM_PROTOCOL_HTTP,
	STREAM_PROTOCOL_HLS,
	STREAM_PROTOCOL_DASH,
	STREAM_PROTOCOL_RTSP,
	STREAM_PROTOCOL_FILE
} stream_protocol_t;

/* Input profiles for streaminput_policy_build(). RECORD reproduces the
 * option set the app-side relay paths (CStreamRec/CStreamStream) have
 * always used. The LIVE profile (libeplayer3 defaults) follows with the
 * container adoption in milestone M4. */
typedef enum
{
	STREAM_PROFILE_RECORD = 0
} stream_input_profile_t;

/* One key/value pair; both strings owned by the list. */
typedef struct
{
	char *key;
	char *value;
} stream_kv_t;

/* Growable key/value list, used for the policy output (FFmpeg option
 * names and values as strings; the consumer feeds them into an
 * AVDictionary). Keys are unique: setting an existing key replaces its
 * value. */
typedef struct
{
	stream_kv_t *items;
	size_t count;
	size_t capacity;
} stream_kv_list_t;

/* Generic description of a stream source (spec section 2, reduced to
 * what phase 1/2 consumers actually read). headers is the resolver's
 * opaque header blob, copied verbatim -- see the ownership notes at the
 * top. live: 0 = no/unknown, 1 = live. */
typedef struct
{
	char *url;
	char *url2;
	char *headers;
	stream_protocol_t protocol;
	int live;
} stream_source_t;

void streaminput_kv_init(stream_kv_list_t *list);
void streaminput_kv_free(stream_kv_list_t *list);
int streaminput_kv_set(stream_kv_list_t *list, const char *key, const char *value);
const char *streaminput_kv_get(const stream_kv_list_t *list, const char *key);

void streaminput_source_init(stream_source_t *src);
void streaminput_source_free(stream_source_t *src);
/* All three setters copy their argument, and all three treat NULL and ""
 * alike: the field is cleared and left at NULL. So after set_url(src, "")
 * the public src->url is NULL, not an empty string -- read it with that in
 * mind rather than handing it straight to strlen(). */
/* Copies url and derives src->protocol from it. */
int streaminput_source_set_url(stream_source_t *src, const char *url);
int streaminput_source_set_url2(stream_source_t *src, const char *url2);
/* Copies the opaque resolver header blob verbatim. */
int streaminput_source_set_headers(stream_source_t *src, const char *headers);

/* URL scheme starts with http:// or https:// -- the exact predicate the
 * legacy option blocks used, kept as-is for behaviour equivalence. */
int streaminput_url_is_http(const char *url);
stream_protocol_t streaminput_detect_protocol(const char *url);

/* Build the FFmpeg input options for a source under a profile into
 * opts (existing entries are kept, keys set here replace duplicates).
 * STREAM_PROFILE_RECORD reproduces the historic relay block bit for
 * bit: "headers" = source headers (only if non-empty), and for http(s)
 * URLs "timeout" = "20000000" and "reconnect" = "1". */
int streaminput_policy_build(const stream_source_t *src, stream_input_profile_t profile, stream_kv_list_t *opts);

/* Map a transport error plus optional HTTP status (0 = none) to a
 * failure class. STREAM_ERR_EXIT_REQUESTED and STREAM_ERR_EOF are
 * final local outcomes and win unconditionally; for every other code
 * an explicit http_status wins. */
stream_failure_class_t streaminput_classify(stream_error_code_t code, int http_status);

/* Copy url into out with everything from the first '?' on replaced by
 * "?<redacted>" (tokens, signatures and session ids live there -- spec
 * section 12); a fragment behind the query is hidden with it, which
 * errs on the safe side. Generalizes libeplayer3's
 * hls_ad_debug_redact_uri with one deliberate difference: a bare
 * trailing '?' carries nothing to hide and is kept verbatim here,
 * where that function reports it as redacted. Always NUL-terminates
 * when outlen > 0. Truncation shortens the path, never the marker:
 * with a query present the output ends in the complete "?<redacted>"
 * whenever outlen is at least sizeof that marker, and degenerates to
 * bare path bytes below that. Returns the number of characters
 * written (excluding NUL). */
size_t streaminput_redact_url(const char *url, char *out, size_t outlen);

/* Stable lowercase token names for logging, never NULL. */
const char *streaminput_stage_name(stream_stage_t stage);
const char *streaminput_error_code_name(stream_error_code_t code);
const char *streaminput_failure_class_name(stream_failure_class_t failure);

#ifdef __cplusplus
}
#endif

#endif /* __STREAMINPUT_H__ */
