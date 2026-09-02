/*
 * streaminput -- shared stream input core (WORK-231, spec phase 1/2)
 *
 * See include/streaminput.h for the module contract (plain C99, no
 * libav dependency, stateless, caller-owned memory).
 *
 * (C) 2026 Thilo Graf
 *
 * License: GPLv2 or later
 */
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strncasecmp */

#include <streaminput.h>

static char *si_strdup(const char *s)
{
	size_t len;
	char *copy;

	if (!s)
		return NULL;
	len = strlen(s) + 1;
	copy = (char *)malloc(len);
	if (copy)
		memcpy(copy, s, len);
	return copy;
}

static int si_replace(char **slot, const char *value)
{
	char *copy = NULL;

	if (value && *value)
	{
		copy = si_strdup(value);
		if (!copy)
			return -1;
	}
	free(*slot);
	*slot = copy;
	return 0;
}

void streaminput_kv_init(stream_kv_list_t *list)
{
	if (!list)
		return;
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

void streaminput_kv_free(stream_kv_list_t *list)
{
	size_t i;

	if (!list)
		return;
	for (i = 0; i < list->count; i++)
	{
		free(list->items[i].key);
		free(list->items[i].value);
	}
	free(list->items);
	streaminput_kv_init(list);
}

int streaminput_kv_set(stream_kv_list_t *list, const char *key, const char *value)
{
	size_t i;
	char *vcopy;

	if (!list || !key || !*key || !value)
		return -1;

	vcopy = si_strdup(value);
	if (!vcopy)
		return -1;

	for (i = 0; i < list->count; i++)
	{
		if (strcmp(list->items[i].key, key) == 0)
		{
			free(list->items[i].value);
			list->items[i].value = vcopy;
			return 0;
		}
	}

	if (list->count == list->capacity)
	{
		size_t ncap = list->capacity ? list->capacity * 2 : 4;
		stream_kv_t *nitems = (stream_kv_t *)realloc(list->items, ncap * sizeof(*nitems));
		if (!nitems)
		{
			free(vcopy);
			return -1;
		}
		list->items = nitems;
		list->capacity = ncap;
	}

	list->items[list->count].key = si_strdup(key);
	if (!list->items[list->count].key)
	{
		free(vcopy);
		return -1;
	}
	list->items[list->count].value = vcopy;
	list->count++;
	return 0;
}

const char *streaminput_kv_get(const stream_kv_list_t *list, const char *key)
{
	size_t i;

	if (!list || !key)
		return NULL;
	for (i = 0; i < list->count; i++)
		if (strcmp(list->items[i].key, key) == 0)
			return list->items[i].value;
	return NULL;
}

void streaminput_source_init(stream_source_t *src)
{
	if (!src)
		return;
	src->url = NULL;
	src->url2 = NULL;
	src->headers = NULL;
	src->protocol = STREAM_PROTOCOL_UNKNOWN;
	src->live = 0;
}

void streaminput_source_free(stream_source_t *src)
{
	if (!src)
		return;
	free(src->url);
	free(src->url2);
	free(src->headers);
	streaminput_source_init(src);
}

int streaminput_source_set_url(stream_source_t *src, const char *url)
{
	if (!src)
		return -1;
	if (si_replace(&src->url, url) < 0)
		return -1;
	src->protocol = streaminput_detect_protocol(src->url);
	return 0;
}

int streaminput_source_set_url2(stream_source_t *src, const char *url2)
{
	if (!src)
		return -1;
	return si_replace(&src->url2, url2);
}

int streaminput_source_set_headers(stream_source_t *src, const char *headers)
{
	if (!src)
		return -1;
	return si_replace(&src->headers, headers);
}

/* Case-sensitive on purpose: FFmpeg matches URL schemes case
 * sensitively, and so did the legacy option blocks this module
 * replaces -- behaviour equivalence beats RFC lenience here. */
static int si_has_prefix(const char *s, const char *prefix)
{
	return strncmp(s, prefix, strlen(prefix)) == 0;
}

int streaminput_url_is_http(const char *url)
{
	if (!url)
		return 0;
	return si_has_prefix(url, "http://") || si_has_prefix(url, "https://");
}

/* Path ends in the given suffix, ignoring any query string or
 * fragment ("...playlist.m3u8#t=30" is still an HLS path). */
static int si_path_has_suffix(const char *url, const char *suffix)
{
	const char *end = strpbrk(url, "?#");
	size_t plen = end ? (size_t)(end - url) : strlen(url);
	size_t slen = strlen(suffix);

	if (plen < slen)
		return 0;
	return strncasecmp(url + plen - slen, suffix, slen) == 0;
}

stream_protocol_t streaminput_detect_protocol(const char *url)
{
	if (!url || !*url)
		return STREAM_PROTOCOL_UNKNOWN;

	if (streaminput_url_is_http(url))
	{
		if (si_path_has_suffix(url, ".m3u8"))
			return STREAM_PROTOCOL_HLS;
		if (si_path_has_suffix(url, ".mpd"))
			return STREAM_PROTOCOL_DASH;
		return STREAM_PROTOCOL_HTTP;
	}
	if (si_has_prefix(url, "rtsp://"))
		return STREAM_PROTOCOL_RTSP;
	if (si_has_prefix(url, "file://") || url[0] == '/')
		return STREAM_PROTOCOL_FILE;
	return STREAM_PROTOCOL_UNKNOWN;
}

int streaminput_policy_build(const stream_source_t *src, stream_input_profile_t profile, stream_kv_list_t *opts)
{
	if (!src || !opts)
		return -1;

	switch (profile)
	{
		case STREAM_PROFILE_RECORD:
			/* Historic CStreamRec/CStreamStream block, value for
			 * value: headers whenever the resolver supplied some,
			 * timeout (20 s in microseconds) and auto-reconnect
			 * only for http(s) inputs. */
			if (src->headers && *src->headers)
			{
				if (streaminput_kv_set(opts, "headers", src->headers) < 0)
					return -1;
			}
			if (streaminput_url_is_http(src->url))
			{
				if (streaminput_kv_set(opts, "timeout", "20000000") < 0)
					return -1;
				if (streaminput_kv_set(opts, "reconnect", "1") < 0)
					return -1;
			}
			return 0;
	}
	/* No default label in the switch: a new stream_input_profile_t
	 * (LIVE is announced for M4) should trip -Wswitch above, the same
	 * way it does in streaminput_classify() and the *_name()
	 * functions. Out-of-range values still land here. */
	return -1;
}

stream_failure_class_t streaminput_classify(stream_error_code_t code, int http_status)
{
	/* A user abort and an orderly end of stream are final local
	 * outcomes; an HTTP status picked up along the way must not
	 * relabel them. Every other code defers to an explicit status. */
	if (code == STREAM_ERR_EXIT_REQUESTED)
		return STREAM_FAILURE_ABORTED;
	if (code == STREAM_ERR_EOF)
		return STREAM_FAILURE_END_OF_STREAM;

	if (http_status >= 400 && http_status < 500)
		return STREAM_FAILURE_HTTP_4XX;
	if (http_status >= 500 && http_status < 600)
		return STREAM_FAILURE_HTTP_5XX;

	switch (code)
	{
		case STREAM_ERR_NONE:
			return STREAM_FAILURE_NONE;
		case STREAM_ERR_CONNECTION_RESET:
		case STREAM_ERR_IO:
			return STREAM_FAILURE_TEMPORARY_NETWORK;
		case STREAM_ERR_CONNECTION_FAILED:
			return STREAM_FAILURE_CONNECTION_FAILED;
		case STREAM_ERR_TIMED_OUT:
			return STREAM_FAILURE_CONNECTION_TIMEOUT;
		case STREAM_ERR_INVALID_DATA:
			return STREAM_FAILURE_INVALID_MANIFEST;
		case STREAM_ERR_EXIT_REQUESTED:
			return STREAM_FAILURE_ABORTED;
		case STREAM_ERR_EOF:
			return STREAM_FAILURE_END_OF_STREAM;
		case STREAM_ERR_HTTP_BAD_REQUEST:
		case STREAM_ERR_HTTP_UNAUTHORIZED:
		case STREAM_ERR_HTTP_FORBIDDEN:
		case STREAM_ERR_HTTP_NOT_FOUND:
		case STREAM_ERR_HTTP_OTHER_4XX:
			return STREAM_FAILURE_HTTP_4XX;
		case STREAM_ERR_HTTP_SERVER_ERROR:
			return STREAM_FAILURE_HTTP_5XX;
		case STREAM_ERR_PROTOCOL_NOT_FOUND:
			return STREAM_FAILURE_UNSUPPORTED_PROTOCOL;
		case STREAM_ERR_UNKNOWN:
			return STREAM_FAILURE_UNKNOWN;
	}
	/* No default label: a new stream_error_code_t should trip -Wswitch
	 * here, the same way it does in the *_name() functions below. */
	return STREAM_FAILURE_UNKNOWN;
}

size_t streaminput_redact_url(const char *url, char *out, size_t outlen)
{
	static const char redacted[] = STREAMINPUT_REDACTED_MARKER;
	const char *query;
	size_t plen, n;

	if (!out || outlen == 0)
		return 0;
	out[0] = '\0';
	if (!url)
		return 0;

	query = strchr(url, '?');
	/* A trailing '?' carries nothing to hide; keep it verbatim rather
	 * than implying a redacted query that was never there. */
	if (query && query[1] == '\0')
		query = NULL;
	plen = query ? (size_t)(query - url) : strlen(url);

	/* With a query present the output must end in the complete marker:
	 * a torn "?<red" tells the reader nothing, and a cut landing on
	 * the '?' would be byte-identical to a URL that had no query at
	 * all. So the path gives way, never the marker. Only when the
	 * buffer cannot even hold the marker alone (outlen <
	 * sizeof(redacted)) are bare path bytes emitted. */
	if (query && outlen >= sizeof(redacted) && plen > outlen - sizeof(redacted))
		plen = outlen - sizeof(redacted);
	if (plen >= outlen)
		plen = outlen - 1;
	memcpy(out, url, plen);
	n = plen;
	out[n] = '\0';

	if (query && outlen >= sizeof(redacted))
	{
		memcpy(out + n, redacted, sizeof(redacted) - 1);
		n += sizeof(redacted) - 1;
		out[n] = '\0';
	}
	return n;
}

const char *streaminput_protocol_name(stream_protocol_t protocol)
{
	switch (protocol)
	{
		case STREAM_PROTOCOL_UNKNOWN:
			return "unknown";
		case STREAM_PROTOCOL_HTTP:
			return "http";
		case STREAM_PROTOCOL_HLS:
			return "hls";
		case STREAM_PROTOCOL_DASH:
			return "dash";
		case STREAM_PROTOCOL_RTSP:
			return "rtsp";
		case STREAM_PROTOCOL_FILE:
			return "file";
	}
	return "unknown";
}

const char *streaminput_stage_name(stream_stage_t stage)
{
	switch (stage)
	{
		case STREAM_STAGE_RESOLVE:
			return "resolve";
		case STREAM_STAGE_REDIRECT:
			return "redirect";
		case STREAM_STAGE_CONNECT:
			return "connect";
		case STREAM_STAGE_OPEN:
			return "open";
		case STREAM_STAGE_PROBE:
			return "probe";
		case STREAM_STAGE_PLAYBACK:
			return "playback";
	}
	return "unknown";
}

const char *streaminput_error_code_name(stream_error_code_t code)
{
	switch (code)
	{
		case STREAM_ERR_NONE:
			return "none";
		case STREAM_ERR_CONNECTION_RESET:
			return "connection-reset";
		case STREAM_ERR_CONNECTION_FAILED:
			return "connection-failed";
		case STREAM_ERR_TIMED_OUT:
			return "timed-out";
		case STREAM_ERR_IO:
			return "io-error";
		case STREAM_ERR_INVALID_DATA:
			return "invalid-data";
		case STREAM_ERR_EXIT_REQUESTED:
			return "exit-requested";
		case STREAM_ERR_EOF:
			return "eof";
		case STREAM_ERR_HTTP_BAD_REQUEST:
			return "http-400";
		case STREAM_ERR_HTTP_UNAUTHORIZED:
			return "http-401";
		case STREAM_ERR_HTTP_FORBIDDEN:
			return "http-403";
		case STREAM_ERR_HTTP_NOT_FOUND:
			return "http-404";
		case STREAM_ERR_HTTP_OTHER_4XX:
			return "http-4xx";
		case STREAM_ERR_HTTP_SERVER_ERROR:
			return "http-5xx";
		case STREAM_ERR_PROTOCOL_NOT_FOUND:
			return "protocol-not-found";
		case STREAM_ERR_UNKNOWN:
			return "unknown";
	}
	return "unknown";
}

const char *streaminput_failure_class_name(stream_failure_class_t failure)
{
	switch (failure)
	{
		case STREAM_FAILURE_NONE:
			return "none";
		case STREAM_FAILURE_CONNECTION_FAILED:
			return "connection-failed";
		case STREAM_FAILURE_CONNECTION_TIMEOUT:
			return "connection-timeout";
		case STREAM_FAILURE_HTTP_4XX:
			return "http-4xx";
		case STREAM_FAILURE_HTTP_5XX:
			return "http-5xx";
		case STREAM_FAILURE_INVALID_MANIFEST:
			return "invalid-manifest";
		case STREAM_FAILURE_UNSUPPORTED_PROTOCOL:
			return "unsupported-protocol";
		case STREAM_FAILURE_TEMPORARY_NETWORK:
			return "temporary-network";
		case STREAM_FAILURE_END_OF_STREAM:
			return "end-of-stream";
		case STREAM_FAILURE_ABORTED:
			return "aborted";
		case STREAM_FAILURE_UNKNOWN:
			return "unknown";
	}
	return "unknown";
}
