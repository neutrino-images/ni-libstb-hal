/*
 * linuxdvb output/writer handling
 *
 * Copyright (C) 2010  konfetti (based on code from libeplayer2)
 * Copyright (C) 2014  martii   (based on code from libeplayer3)
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>
#include <errno.h>

#include <algorithm>

#include "misc.h"
#include "pes.h"
#include "writer.h"

#define WMV3_PRIVATE_DATA_LENGTH		 4

#define METADATA_STRUCT_A_START			12
#define METADATA_STRUCT_B_START			24
#define METADATA_STRUCT_B_FRAMERATE_START	32
#define METADATA_STRUCT_C_START			 8


#define VC1_SEQUENCE_LAYER_METADATA_START_CODE	0x80
#define VC1_FRAME_START_CODE			0x0d

class WriterVC1 : public Writer
{
	private:
		bool initialHeader;
		uint8_t FrameHeaderSeen;
		AVStream *stream;
	public:
		bool Write(AVPacket *packet, int64_t pts);
		void Init(int _fd, AVStream *_stream, Player *_player);
		WriterVC1();
};

void WriterVC1::Init(int _fd, AVStream *_stream, Player *_player)
{
	fd = _fd;
	stream = _stream;
	player = _player;
	initialHeader = true;
}

bool WriterVC1::Write(AVPacket *packet, int64_t pts)
{
	if (!packet || !packet->data)
		return false;

	if (initialHeader) {
		initialHeader = false;
		FrameHeaderSeen = false;

		const uint8_t SequenceLayerStartCode[] =
			{ 0x00, 0x00, 0x01, VC1_SEQUENCE_LAYER_METADATA_START_CODE };


		const uint8_t Metadata[] = {
			0x00, 0x00, 0x00, 0xc5,
			0x04, 0x00, 0x00, 0x00,
			0xc0, 0x00, 0x00, 0x00,	/* Struct C set for for advanced profile */
			0x00, 0x00, 0x00, 0x00,	/* Struct A */
			0x00, 0x00, 0x00, 0x00,
			0x0c, 0x00, 0x00, 0x00,
			0x60, 0x00, 0x00, 0x00,	/* Struct B */
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00
		};

		uint8_t PesHeader[PES_MAX_HEADER_SIZE];
		uint8_t PesPayload[128];
		uint8_t *PesPtr;
		unsigned int usecPerFrame = av_rescale(AV_TIME_BASE, stream->r_frame_rate.den, stream->r_frame_rate.num);
		struct iovec iov[2];


		memset(PesPayload, 0, sizeof(PesPayload));

		PesPtr = PesPayload;

		memcpy(PesPtr, SequenceLayerStartCode, sizeof(SequenceLayerStartCode));
		PesPtr += sizeof(SequenceLayerStartCode);

		memcpy(PesPtr, Metadata, sizeof(Metadata));
		PesPtr += METADATA_STRUCT_C_START;
		PesPtr += WMV3_PRIVATE_DATA_LENGTH;

		/* Metadata Header Struct A */
		*PesPtr++ = (get_codecpar(stream)->height >> 0) & 0xff;
		*PesPtr++ = (get_codecpar(stream)->height >> 8) & 0xff;
		*PesPtr++ = (get_codecpar(stream)->height >> 16) & 0xff;
		*PesPtr++ = get_codecpar(stream)->height >> 24;
		*PesPtr++ = (get_codecpar(stream)->width >> 0) & 0xff;
		*PesPtr++ = (get_codecpar(stream)->width >> 8) & 0xff;
		*PesPtr++ = (get_codecpar(stream)->width >> 16) & 0xff;
		*PesPtr++ = get_codecpar(stream)->width >> 24;

		PesPtr += 12;		/* Skip flag word and Struct B first 8 bytes */

		*PesPtr++ = (usecPerFrame >> 0) & 0xff;
		*PesPtr++ = (usecPerFrame >> 8) & 0xff;
		*PesPtr++ = (usecPerFrame >> 16) & 0xff;
		*PesPtr++ = usecPerFrame >> 24;

		iov[0].iov_base = PesHeader;
		iov[1].iov_base = PesPayload;
		iov[1].iov_len = PesPtr - PesPayload;
		iov[0].iov_len = InsertPesHeader(PesHeader, iov[1].iov_len, VC1_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
		if (writev(fd, iov, 2) < 0)
			return false;

		/* For VC1 the codec private data is a standard vc1 sequence header so we just copy it to the output */
		iov[0].iov_base = PesHeader;
		iov[1].iov_base = get_codecpar(stream)->extradata;
		iov[1].iov_len = get_codecpar(stream)->extradata_size;
		iov[0].iov_len = InsertPesHeader(PesHeader, iov[1].iov_len, VC1_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
		if (writev(fd, iov, 2) < 0)
			return false;

		initialHeader = false;
	}

	if (packet->size > 0) {
		int Position = 0;
		bool insertSampleHeader = true;

		while (Position < packet->size) {
			int PacketLength = std::min(packet->size - Position, MAX_PES_PACKET_SIZE);
			uint8_t PesHeader[PES_MAX_HEADER_SIZE];
			int HeaderLength = InsertPesHeader(PesHeader, PacketLength, VC1_VIDEO_PES_START_CODE, pts, 0);

			if (insertSampleHeader) {
				const uint8_t Vc1FrameStartCode[] = { 0, 0, 1, VC1_FRAME_START_CODE };

				if (!FrameHeaderSeen && (packet->size > 3) && (memcmp(packet->data, Vc1FrameStartCode, 4) == 0))
					FrameHeaderSeen = true;
				if (!FrameHeaderSeen) {
					memcpy(&PesHeader[HeaderLength], Vc1FrameStartCode, sizeof(Vc1FrameStartCode));
					HeaderLength += sizeof(Vc1FrameStartCode);
				}
				insertSampleHeader = false;
			}

			struct iovec iov[2];
			iov[0].iov_base = PesHeader;
			iov[0].iov_len = HeaderLength;
			iov[1].iov_base = packet->data + Position;
			iov[1].iov_len = PacketLength;

			ssize_t l = writev(fd, iov, 2);
			if (l < 0)
				return false;

			Position += PacketLength;
			pts = INVALID_PTS_VALUE;
		}
	}

	return true;
}

WriterVC1::WriterVC1()
{
	Register(this, AV_CODEC_ID_VC1, VIDEO_ENCODING_VC1);
}

static WriterVC1 writer_vc1 __attribute__ ((init_priority (300)));
