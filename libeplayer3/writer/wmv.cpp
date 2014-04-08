/*
 * linuxdvb output/writer handling
 *
 * Copyright (C) 2010  konfetti (based on code from libeplayer2)
 * Copyright (C) 2014  martii   (based on code from libeplayer3)
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>

#include "misc.h"
#include "pes.h"
#include "writer.h"

#include <algorithm>

#define WMV3_PRIVATE_DATA_LENGTH	4

static const unsigned char Metadata[] = {
	0x00, 0x00, 0x00, 0xc5,
	0x04, 0x00, 0x00, 0x00,
#define METADATA_STRUCT_C_START			8
	0xc0, 0x00, 0x00, 0x00,	/* Struct C set for for advanced profile */
#define METADATA_STRUCT_A_START			12
	0x00, 0x00, 0x00, 0x00,	/* Struct A */
	0x00, 0x00, 0x00, 0x00,
	0x0c, 0x00, 0x00, 0x00,
#define METADATA_STRUCT_B_START			24
	0x60, 0x00, 0x00, 0x00,	/* Struct B */
	0x00, 0x00, 0x00, 0x00,
#define METADATA_STRUCT_B_FRAMERATE_START	32
	0x00, 0x00, 0x00, 0x00
};

class WriterWMV : public Writer
{
	private:
		bool initialHeader;
	public:
		bool Write(int fd, AVFormatContext *avfc, AVStream *stream, AVPacket *packet, int64_t pts);
		void Init();
		WriterWMV();
};

void WriterWMV::Init()
{
	initialHeader = 1;
}

bool WriterWMV::Write(int fd, AVFormatContext * /* avfc */, AVStream *stream, AVPacket *packet, int64_t pts)
{
	if (fd < 0 || !packet)
		return false;

	if (initialHeader) {
#define PES_MIN_HEADER_SIZE 9
		unsigned char PesPacket[PES_MIN_HEADER_SIZE + 128];
		unsigned char *PesPtr;
		unsigned int MetadataLength;
		unsigned int usecPerFrame = ((10000000.0 / av_q2d(stream->r_frame_rate)));

		PesPtr = &PesPacket[PES_MIN_HEADER_SIZE];

		memcpy(PesPtr, Metadata, sizeof(Metadata));
		PesPtr += METADATA_STRUCT_C_START;

		unsigned char privateData[WMV3_PRIVATE_DATA_LENGTH] = { 0 };
		memcpy(privateData, stream->codec->extradata, stream->codec->extradata_size > WMV3_PRIVATE_DATA_LENGTH ? WMV3_PRIVATE_DATA_LENGTH : stream->codec->extradata_size);

		memcpy(PesPtr, privateData, WMV3_PRIVATE_DATA_LENGTH);
		PesPtr += WMV3_PRIVATE_DATA_LENGTH;

		/* Metadata Header Struct A */
		*PesPtr++ = (stream->codec->height >> 0) & 0xff;
		*PesPtr++ = (stream->codec->height >> 8) & 0xff;
		*PesPtr++ = (stream->codec->height >> 16) & 0xff;
		*PesPtr++ = stream->codec->height >> 24;
		*PesPtr++ = (stream->codec->width >> 0) & 0xff;
		*PesPtr++ = (stream->codec->width >> 8) & 0xff;
		*PesPtr++ = (stream->codec->width >> 16) & 0xff;
		*PesPtr++ = stream->codec->width >> 24;

		PesPtr += 12;		/* Skip flag word and Struct B first 8 bytes */

		*PesPtr++ = (usecPerFrame >> 0) & 0xff;
		*PesPtr++ = (usecPerFrame >> 8) & 0xff;
		*PesPtr++ = (usecPerFrame >> 16) & 0xff;
		*PesPtr++ = usecPerFrame >> 24;

		MetadataLength = PesPtr - &PesPacket[PES_MIN_HEADER_SIZE];

		int HeaderLength = InsertPesHeader(PesPacket, MetadataLength, VC1_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);

		if (write(fd, PesPacket, HeaderLength + MetadataLength) < 0)
			return false;

		initialHeader = false;
	}

	if (packet->size > 0 && packet->data) {
		int Position = 0;
		bool insertSampleHeader = true;

		while (Position < packet->size) {

			int PacketLength = std::min(packet->size - Position, MAX_PES_PACKET_SIZE);

			unsigned char PesHeader[PES_MAX_HEADER_SIZE] = { 0 };
			int HeaderLength = InsertPesHeader(PesHeader, PacketLength, VC1_VIDEO_PES_START_CODE, pts, 0);

			if (insertSampleHeader) {
				unsigned int PesLength;
				unsigned int PrivateHeaderLength;

				PrivateHeaderLength = InsertVideoPrivateDataHeader(&PesHeader[HeaderLength], packet->size);
				/* Update PesLength */
				PesLength = PesHeader[PES_LENGTH_BYTE_0] + (PesHeader[PES_LENGTH_BYTE_1] << 8) + PrivateHeaderLength;
				PesHeader[PES_LENGTH_BYTE_0] = PesLength & 0xff;
				PesHeader[PES_LENGTH_BYTE_1] = (PesLength >> 8) & 0xff;
				PesHeader[PES_HEADER_DATA_LENGTH_BYTE] += PrivateHeaderLength;
				PesHeader[PES_FLAGS_BYTE] |= PES_EXTENSION_DATA_PRESENT;

				HeaderLength += PrivateHeaderLength;
				insertSampleHeader = false;
			}

			uint8_t PacketStart[packet->size + HeaderLength];
			memcpy(PacketStart, PesHeader, HeaderLength);
			memcpy(PacketStart + HeaderLength, packet->data + Position, PacketLength);
			if (write(fd, PacketStart, PacketLength + HeaderLength) < 0)
				return false;

			Position += PacketLength;
			pts = INVALID_PTS_VALUE;
		}
	}

	return true;
}

WriterWMV::WriterWMV()
{
	Register(this, AV_CODEC_ID_WMV1, VIDEO_ENCODING_WMV);
	Register(this, AV_CODEC_ID_WMV2, VIDEO_ENCODING_WMV);
	Register(this, AV_CODEC_ID_WMV3, VIDEO_ENCODING_WMV);
}

static WriterWMV writer_wmv __attribute__ ((init_priority (300)));
