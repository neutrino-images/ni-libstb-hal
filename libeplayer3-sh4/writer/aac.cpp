/*
 * linuxdvb output/writer handling.
 *
 * Copyright (C) 2010  konfetti (based on code from libeplayer2)
 * Copyright (C) 2014  DboxOldie (based on code from libeplayer3)
 * inspired by martii
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>
#include <errno.h>

#include <algorithm>

#include "misc.h"
#include "pes.h"
#include "writer.h"

#define AAC_HEADER_LENGTH 7
#define AAC_DEBUG 0

#if AAC_DEBUG
static inline void Hexdump(unsigned char *Data, int length)
{
	int k;
	for (k = 0; k < length; k++)
	{
		printf("%02x ", Data[k]);
		if (((k + 1) & 31) == 0)
			printf("\n");
	}
	printf("\n");

}
#endif

static inline int aac_get_sample_rate_index(uint32_t sample_rate)
{
	if (96000 <= sample_rate)
		return 0;
	else if (88200 <= sample_rate)
		return 1;
	else if (64000 <= sample_rate)
		return 2;
	else if (48000 <= sample_rate)
		return 3;
	else if (44100 <= sample_rate)
		return 4;
	else if (32000 <= sample_rate)
		return 5;
	else if (24000 <= sample_rate)
		return 6;
	else if (22050 <= sample_rate)
		return 7;
	else if (16000 <= sample_rate)
		return 8;
	else if (12000 <= sample_rate)
		return 9;
	else if (11025 <= sample_rate)
		return 10;
	else if (8000 <= sample_rate)
		return 11;
	else if (7350 <= sample_rate)
		return 12;
	else
		return 13;
}

#if 0
static unsigned char DefaultAACHeader[] = {0xff, 0xf1, 0x50, 0x80, 0x00, 0x1f, 0xfc};
#endif

class WriterAAC : public Writer
{
	private:
		uint8_t aacbuf[8];
		unsigned int aacbuflen;
		AVStream *stream;
	public:
		bool Write(AVPacket *packet, int64_t pts);
		void Init(int _fd, AVStream *_stream, Player *_player);
		WriterAAC();
};

void WriterAAC::Init(int _fd, AVStream *_stream, Player *_player)
{
	fd = _fd;
	stream = _stream;
	player = _player;
#if AAC_DEBUG
	printf("Create AAC ExtraData\n");
	printf("stream->codec->extradata_size %d\n", stream->codec->extradata_size);
	Hexdump(stream->codec->extradata, stream->codec->extradata_size);
#endif
	unsigned int object_type = 2;   // LC
	unsigned int sample_index = aac_get_sample_rate_index(stream->codec->sample_rate);
	unsigned int chan_config = stream->codec->channels;
	if (stream->codec->extradata_size >= 2)
	{
		object_type = stream->codec->extradata[0] >> 3;
		sample_index = ((stream->codec->extradata[0] & 0x7) << 1) + (stream->codec->extradata[1] >> 7);
		chan_config = (stream->codec->extradata[1] >> 3) && 0xf;
	}
#if AAC_DEBUG
	printf("aac object_type %d\n", object_type);
	printf("aac sample_index %d\n", sample_index);
	printf("aac chan_config %d\n", chan_config);
#endif
	object_type -= 1;   // Cause of ADTS
	aacbuflen = AAC_HEADER_LENGTH;
	aacbuf[0] = 0xFF;
	aacbuf[1] = 0xF1;
	aacbuf[2] = ((object_type & 0x03) << 6) | (sample_index << 2) | ((chan_config >> 2) & 0x01);
	aacbuf[3] = (chan_config & 0x03) << 6;
	aacbuf[4] = 0x00;
	aacbuf[5] = 0x1F;
	aacbuf[6] = 0xFC;
	aacbuf[7] = 0x00;
#if AAC_DEBUG
	printf("AAC_HEADER -> ");
	Hexdump(aacbuf, 7);
#endif
}

bool WriterAAC::Write(AVPacket *packet, int64_t pts)
{
	if (!packet || !packet->data)
		return false;

	uint8_t PesHeader[PES_MAX_HEADER_SIZE];
	uint8_t ExtraData[AAC_HEADER_LENGTH];

	for (int pos = 0; pos < packet->size + AAC_HEADER_LENGTH;)
	{
		int PacketLength = std::min(packet->size - pos + AAC_HEADER_LENGTH, MAX_PES_PACKET_SIZE);

		memcpy(ExtraData, aacbuf, AAC_HEADER_LENGTH);

//		ExtraData[3] |= (PacketLength >> 11) & 0x3;
		ExtraData[4] = (PacketLength >> 3) & 0xff;
		ExtraData[5] |= (PacketLength << 5) & 0xe0;

		struct iovec iov[3];
		iov[0].iov_base = PesHeader;
		iov[0].iov_len = InsertPesHeader(PesHeader, PacketLength, AAC_AUDIO_PES_START_CODE, pts, 0);
		iov[1].iov_base = ExtraData;
		iov[1].iov_len = AAC_HEADER_LENGTH;
		iov[2].iov_base = packet->data;
		iov[2].iov_len = packet->size;

		ssize_t l = writev(fd, iov, 3);
#if AAC_DEBUG
//		printf("Packet Size + AAC_HEADER_LENGTH= %d Packet Size= %d Written= %d\n", PacketLength, packet->size, l);
#endif
		if (l < 0)
			return false;
		pos += PacketLength;
		pts = INVALID_PTS_VALUE;
	}
	return true;
}

WriterAAC::WriterAAC()
{
	Register(this, AV_CODEC_ID_AAC, AUDIO_ENCODING_AAC);
}

static WriterAAC writer_aac __attribute__((init_priority(300)));
