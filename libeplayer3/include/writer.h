/*
 * writer class headers
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

#ifndef __WRITER_H__
#define __WRITER_H__

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include <linux/dvb/stm_ioctls.h>

#define AV_CODEC_ID_INJECTPCM AV_CODEC_ID_PCM_S16LE

class Player;

class Writer
{
	protected:
		int fd;
		Player *player;
	public:
		static void Register(Writer *w, enum AVCodecID id, video_encoding_t encoding);
		static void Register(Writer *w, enum AVCodecID id, audio_encoding_t encoding);
		static video_encoding_t GetVideoEncoding(enum AVCodecID id);
		static audio_encoding_t GetAudioEncoding(enum AVCodecID id);
		static Writer *GetWriter(enum AVCodecID id, enum AVMediaType codec_type, int track_type);

		virtual void Init(int _fd, AVStream * /*stream*/, Player *_player ) { fd = _fd; player = _player; }
		virtual bool Write(AVPacket *packet, int64_t pts);
};
#endif
