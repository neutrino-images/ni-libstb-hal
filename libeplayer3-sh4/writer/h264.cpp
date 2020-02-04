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

#include "misc.h"
#include "pes.h"
#include "writer.h"

#define NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS		24	// Reference: player/standards/h264.h
#define CONTAINER_PARAMETERS_VERSION			0x00

typedef struct avcC_s {
	uint8_t Version;		// configurationVersion
	uint8_t Profile;		// AVCProfileIndication
	uint8_t Compatibility;		// profile_compatibility
	uint8_t Level;			// AVCLevelIndication
	uint8_t NalLengthMinusOne;	// held in bottom two bits
	uint8_t NumParamSets;		// held in bottom 5 bits
	uint8_t Params[1];		// {length,params}{length,params}...sequence then picture
} avcC_t;

class WriterH264 : public Writer
{
	private:
		bool initialHeader;
		unsigned int NalLengthBytes;
		AVStream *stream;
	public:
		bool Write(AVPacket *packet, int64_t pts);
		void Init(int _fd, AVStream *_stream, Player *_player);
		WriterH264();
};

void WriterH264::Init(int _fd, AVStream *_stream, Player *_player)
{
	fd = _fd;
	stream = _stream;
	player = _player;
	initialHeader = true;
	NalLengthBytes = 1;
}

bool WriterH264::Write(AVPacket *packet, int64_t pts)
{
	if (!packet || !packet->data)
		return false;
	uint8_t PesHeader[PES_MAX_HEADER_SIZE];
	struct iovec iov[512];

	uint8_t *d = packet->data;

	// byte-stream format
	if ((packet->size > 3) && (   (d[0] == 0x00 && d[1] == 0x00 && d[2] == 0x00 && d[3] == 0x01) // first NAL unit
			           || (d[0] == 0xff && d[1] == 0xff && d[2] == 0xff && d[3] == 0xff) // FIXME, needed???
	)) {
		unsigned int FakeStartCode = /* (call->Version << 8) | */ PES_VERSION_FAKE_START_CODE;
		int ic = 0;
		iov[ic++].iov_base = PesHeader;
		unsigned int len = 0;
		if (initialHeader) {
			initialHeader = false;
			iov[ic].iov_base = get_codecpar(stream)->extradata;
			iov[ic++].iov_len = get_codecpar(stream)->extradata_size;
			len += get_codecpar(stream)->extradata_size;
		}
		iov[ic].iov_base = packet->data;
		iov[ic++].iov_len = packet->size;
		len += packet->size;
#if 1 // FIXME: needed?
		// Hellmaster1024:
		// some packets will only be accepted by the player if we send one byte more than data is available.
		// The content of this byte does not matter. It will be ignored by the player
		iov[ic].iov_base = (void *) "";
		iov[ic++].iov_len = 1;
		len++;
#endif
		iov[0].iov_len = InsertPesHeader(PesHeader, len, MPEG_VIDEO_PES_START_CODE, pts, FakeStartCode);
		return writev(fd, iov, ic) > -1;
	}

	// convert NAL units without sync byte sequence to byte-stream format
	if (initialHeader) {
		avcC_t *avcCHeader = (avcC_t *) get_codecpar(stream)->extradata;

		if (!avcCHeader) {
			fprintf(stderr, "stream->codec->extradata == NULL\n");
			return false;
		}

		if (avcCHeader->Version != 1)
			fprintf(stderr, "Error unknown avcC version (%x). Expect problems.\n", avcCHeader->Version);

		// The player will use FrameRate and TimeScale to calculate the default frame rate.
		// FIXME: TimeDelta should be used instead of FrameRate. This is a historic implementation bug.
		// Reference:  player/frame_parser/frame_parser_video_h264.cpp FrameParser_VideoH264_c::ReadPlayer2ContainerParameters()
		unsigned int FrameRate = av_rescale(1000ll, stream->r_frame_rate.num, stream->r_frame_rate.den);
		unsigned int TimeScale = (FrameRate < 23970) ? 1001 : 1000; /* FIXME: revise this */

		uint8_t Header[20];
		unsigned int len = 0;
		Header[len++] = 0x00;	// Start code, 00 00 00 01 for first NAL unit
		Header[len++] = 0x00;
		Header[len++] = 0x00;
		Header[len++] = 0x01;
		Header[len++] = NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS; // NAL unit header
		// Container message version - changes when/if we vary the format of the message
		Header[len++] = CONTAINER_PARAMETERS_VERSION;
		Header[len++] = 0xff;	// marker bits

		#if 0
		if (FrameRate == 0xffffffff)
			FrameRate = (TimeScale > 1000) ? 1001 : 1;
		#endif

		Header[len++] = (TimeScale >> 24) & 0xff;	// Output the timescale
		Header[len++] = (TimeScale >> 16) & 0xff;
		Header[len++] = 0xff;	// marker bits
		Header[len++] = (TimeScale >>  8) & 0xff;
		Header[len++] = (TimeScale      ) & 0xff;
		Header[len++] = 0xff;	// marker bits

		Header[len++] = (FrameRate >> 24) & 0xff;	// Output frame period (should be: time delta)
		Header[len++] = (FrameRate >> 16) & 0xff;
		Header[len++] = 0xff;	// marker bits
		Header[len++] = (FrameRate >>  8) & 0xff;
		Header[len++] = (FrameRate      ) & 0xff;
		Header[len++] = 0xff;	// marker bits

		Header[len++] = 0x80;	// Rsbp trailing bits

		int ic = 0;
		iov[ic].iov_base = PesHeader;
		iov[ic++].iov_len = InsertPesHeader(PesHeader, len, MPEG_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
		iov[ic].iov_base = Header;
		iov[ic++].iov_len = len;
		if (writev(fd, iov, ic) < 0)
			return false;

		ic = 0;
		iov[ic++].iov_base = PesHeader;

		NalLengthBytes = (avcCHeader->NalLengthMinusOne & 0x03) + 1;
		unsigned int ParamOffset = 0;
		len = 0;

		// sequence parameter set
		unsigned int ParamSets = avcCHeader->NumParamSets & 0x1f;
		for (unsigned int i = 0; i < ParamSets; i++) {
			unsigned int PsLength = (avcCHeader->Params[ParamOffset] << 8) | avcCHeader->Params[ParamOffset + 1];

			iov[ic].iov_base = (uint8_t *) "\0\0\0\1";
			iov[ic++].iov_len = 4;
			len += 4;
			iov[ic].iov_base = &avcCHeader->Params[ParamOffset + 2];
			iov[ic++].iov_len = PsLength;
			len += PsLength;
			ParamOffset += PsLength + 2;
		}

		// picture parameter set
		ParamSets = avcCHeader->Params[ParamOffset++];

		for (unsigned int i = 0; i < ParamSets; i++) {
			unsigned int PsLength = (avcCHeader->Params[ParamOffset] << 8) | avcCHeader->Params[ParamOffset + 1];

			iov[ic].iov_base = (uint8_t *) "\0\0\0\1";
			iov[ic++].iov_len = 4;
			len += 4;
			iov[ic].iov_base = &avcCHeader->Params[ParamOffset + 2];
			iov[ic++].iov_len = PsLength;
			len += PsLength;
			ParamOffset += PsLength + 2;
		}

		iov[0].iov_len = InsertPesHeader(PesHeader, len, MPEG_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
		ssize_t l = writev(fd, iov, ic);
		if (l < 0)
			return false;

		initialHeader = false;
	}

	uint8_t *de = d + packet->size;
	do {
		unsigned int len = 0;
		switch (NalLengthBytes) {
			case 4:
				len = *d;
				d++;
			case 3:
				len <<= 8;
				len |= *d;
				d++;
			case 2:
				len <<= 8;
				len |= *d;
				d++;
			default:
				len <<= 8;
				len |= *d;
				d++;
		}

		if (d + len > de) {
			fprintf(stderr, "NAL length past end of buffer - size %u frame offset %d left %d\n", len, (int) (d - packet->data), (int) (de - d));
			break;
		}

		int ic = 0;
		iov[ic++].iov_base = PesHeader;
		iov[ic].iov_base = (uint8_t *) "\0\0\0\1";
		iov[ic++].iov_len = 4;

		iov[ic].iov_base = d;
		iov[ic++].iov_len = len;
		iov[0].iov_len = InsertPesHeader(PesHeader, len + 3, MPEG_VIDEO_PES_START_CODE, pts, 0);
		ssize_t l = writev(fd, iov, ic);
		if (l < 0)
			return false;

		d += len;
		pts = INVALID_PTS_VALUE;

	} while (d < de);

	return true;
}

WriterH264::WriterH264()
{
	Register(this, AV_CODEC_ID_H264, VIDEO_ENCODING_H264);
}

static WriterH264 writerh264 __attribute__ ((init_priority (300)));
