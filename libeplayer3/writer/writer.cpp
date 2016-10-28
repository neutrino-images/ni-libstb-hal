/*
 * linuxdvb output/writer handling
 *
 * Copyright (C) 2014  martii
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <map>

#include "pes.h"
#include "writer.h"

// This does suck ... the original idea was to just link the object files and let them register themselves.
// Alas, that didn't work as expected.
#include "ac3.cpp"
#include "divx.cpp"
#include "dts.cpp"
#include "h263.cpp"
#include "h264.cpp"
#include "mp3.cpp"
#include "mpeg2.cpp"
#include "pcm.cpp"
#include "vc1.cpp"
#include "wmv.cpp"
//#include "aac.cpp"

static std::map<enum AVCodecID,Writer *>writers __attribute__ ((init_priority (200)));
static std::map<enum AVCodecID,video_encoding_t>vencoding __attribute__ ((init_priority (200)));
static std::map<enum AVCodecID,audio_encoding_t>aencoding __attribute__ ((init_priority (200)));

void Writer::Register(Writer *w, enum AVCodecID id, video_encoding_t encoding)
{
	writers[id] = w;
	vencoding[id] = encoding;
}

void Writer::Register(Writer *w, enum AVCodecID id, audio_encoding_t encoding)
{
	writers[id] = w;
	aencoding[id] = encoding;
}

bool Writer::Write(AVPacket * /* packet */, int64_t /* pts */)
{
	return false;
}

static Writer writer __attribute__ ((init_priority (300)));

Writer *Writer::GetWriter(enum AVCodecID id, enum AVMediaType codec_type, int track_type)
{
        fprintf(stderr, "GETWRITER %d %d %d", id, codec_type, track_type);
	if (track_type != 6) { // hack for ACC resampling
		std::map<enum AVCodecID,Writer*>::iterator it = writers.find(id);
	if (it != writers.end())
		return it->second;
	}
	switch (codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			if (id == AV_CODEC_ID_INJECTPCM) // should not happen
				break;
            return GetWriter(AV_CODEC_ID_INJECTPCM, codec_type, 100);
		case AVMEDIA_TYPE_VIDEO:
			if (id == AV_CODEC_ID_MPEG2TS) // should not happen
				break;
            return GetWriter(AV_CODEC_ID_MPEG2TS, codec_type, 100);
		default:
			break;
	}
	return &writer;
}

video_encoding_t Writer::GetVideoEncoding(enum AVCodecID id)
{
	std::map<enum AVCodecID,video_encoding_t>::iterator it = vencoding.find(id);
	if (it != vencoding.end())
		return it->second;
	return VIDEO_ENCODING_AUTO;
}

audio_encoding_t Writer::GetAudioEncoding(enum AVCodecID id)
{
	std::map<enum AVCodecID,audio_encoding_t>::iterator it = aencoding.find(id);
	if (it != aencoding.end())
		return it->second;
	return AUDIO_ENCODING_LPCMA;
}
