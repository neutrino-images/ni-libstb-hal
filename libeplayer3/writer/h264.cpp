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
#include <sys/uio.h>
#include <errno.h>

#include "misc.h"
#include "pes.h"
#include "writer.h"

#define NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS		24
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

static uint8_t Head[] = { 0, 0, 0, 1 };

class WriterH264 : public Writer
{
	private:
		bool initialHeader;
		unsigned int NalLengthBytes;
	public:
		bool Write(int fd, AVStream *stream, AVPacket *packet, int64_t pts);
		void Init();
		WriterH264();
};

void WriterH264::Init(void)
{
	initialHeader = true;
	NalLengthBytes = 1;
}

bool WriterH264::Write(int fd, AVStream *stream, AVPacket *packet, int64_t pts)
{
	if (fd < 0 || !packet || !packet->data)
		return false;
	uint8_t PesHeader[PES_MAX_HEADER_SIZE];
	unsigned int TimeDelta;
	unsigned int TimeScale;
	int len = 0;
	int ic = 0;
	struct iovec iov[128];

	TimeDelta = av_rescale(1000ll, stream->r_frame_rate.num, stream->r_frame_rate.den);
	TimeScale = (TimeDelta < 23970) ? 1001 : 1000; /* fixme: revise this */

	if ((packet->size > 3)
	 && ((packet->data[0] == 0x00 && packet->data[1] == 0x00 && packet->data[2] == 0x00 && packet->data[3] == 0x01)
	  || (packet->data[0] == 0xff && packet->data[1] == 0xff && packet->data[2] == 0xff && packet->data[3] == 0xff))) {
		unsigned int FakeStartCode = /* (call->Version << 8) | */ PES_VERSION_FAKE_START_CODE;
		iov[ic++].iov_base = PesHeader;
		if (initialHeader) {
			initialHeader = false;
			iov[ic].iov_base = stream->codec->extradata;
			iov[ic++].iov_len = stream->codec->extradata_size;
			len += stream->codec->extradata_size;
		}
		iov[ic].iov_base = packet->data;
		iov[ic++].iov_len = packet->size;
		len += packet->size;
		// Hellmaster1024:
		// some packets will only be accepted by the player if we send one byte more than data is available.
		// The content of this byte does not matter. It will be ignored by the player
		iov[ic].iov_base = (void *) "";
		iov[ic++].iov_len = 1;
		iov[0].iov_len = InsertPesHeader(PesHeader, len, MPEG_VIDEO_PES_START_CODE, pts, FakeStartCode);
		return writev(fd, iov, ic) > -1;
	}

	if (initialHeader) {
		avcC_t *avcCHeader = (avcC_t *) stream->codec->extradata;

		if (!avcCHeader) {
			fprintf(stderr, "stream->codec->extradata == NULL\n");
			return false;
		}

		if (avcCHeader->Version != 1)
			fprintf(stderr, "Error unknown avcC version (%x). Expect problems.\n", avcCHeader->Version);

		uint8_t Header[19];
		unsigned int HeaderLen = 0;
		Header[HeaderLen++] = 0x00;	// Start code
		Header[HeaderLen++] = 0x00;
		Header[HeaderLen++] = 0x01;
		Header[HeaderLen++] = NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS;
		// Container message version - changes when/if we vary the format of the message
		Header[HeaderLen++] = CONTAINER_PARAMETERS_VERSION;
		Header[HeaderLen++] = 0xff;	// Field separator

		if (TimeDelta == 0xffffffff)
			TimeDelta = (TimeScale > 1000) ? 1001 : 1;

		Header[HeaderLen++] = (TimeScale >> 24) & 0xff;	// Output the timescale
		Header[HeaderLen++] = (TimeScale >> 16) & 0xff;
		Header[HeaderLen++] = 0xff;
		Header[HeaderLen++] = (TimeScale >> 8) & 0xff;
		Header[HeaderLen++] = TimeScale & 0xff;
		Header[HeaderLen++] = 0xff;

		Header[HeaderLen++] = (TimeDelta >> 24) & 0xff;	// Output frame period
		Header[HeaderLen++] = (TimeDelta >> 16) & 0xff;
		Header[HeaderLen++] = 0xff;
		Header[HeaderLen++] = (TimeDelta >> 8) & 0xff;
		Header[HeaderLen++] = TimeDelta & 0xff;
		Header[HeaderLen++] = 0xff;
		Header[HeaderLen++] = 0x80;	// Rsbp trailing bits

		ic = 0;
		iov[ic].iov_base = PesHeader;
		iov[ic++].iov_len = InsertPesHeader(PesHeader, HeaderLen, MPEG_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
		iov[ic].iov_base = Header;
		iov[ic++].iov_len = HeaderLen;
		len = writev(fd, iov, ic);
		if (len < 0)
			return false;

		NalLengthBytes = (avcCHeader->NalLengthMinusOne & 0x03) + 1;
		unsigned int ParamSets = avcCHeader->NumParamSets & 0x1f;
		unsigned int ParamOffset = 0;
		unsigned int InitialHeaderLength = 0;

		ic = 0;
		iov[ic++].iov_base = PesHeader;
		for (unsigned int i = 0; i < ParamSets; i++) {
			unsigned int PsLength = (avcCHeader->Params[ParamOffset] << 8) + avcCHeader->Params[ParamOffset + 1];

			iov[ic].iov_base = (char *) Head;
			iov[ic++].iov_len = sizeof(Head);
			InitialHeaderLength += sizeof(Head);
			iov[ic].iov_base = &avcCHeader->Params[ParamOffset + 2];
			iov[ic++].iov_len = PsLength;
			InitialHeaderLength += PsLength;
			ParamOffset += PsLength + 2;
		}

		ParamSets = avcCHeader->Params[ParamOffset++];

		for (unsigned int i = 0; i < ParamSets; i++) {
			unsigned int PsLength = (avcCHeader->Params[ParamOffset] << 8) + avcCHeader->Params[ParamOffset + 1];

			iov[ic].iov_base = (char *) Head;
			iov[ic++].iov_len = sizeof(Head);
			InitialHeaderLength += sizeof(Head);
			iov[ic].iov_base = &avcCHeader->Params[ParamOffset + 2];
			iov[ic++].iov_len = PsLength;
			InitialHeaderLength += PsLength;
			ParamOffset += PsLength + 2;
		}

		iov[0].iov_len = InsertPesHeader(PesHeader, InitialHeaderLength, MPEG_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
		ssize_t l = writev(fd, iov, ic);
		if (l < 0)
			return false;
		len += l;

		initialHeader = 0;
	}

	unsigned int SampleSize = packet->size;
	unsigned int NalStart = 0;
	unsigned int VideoPosition = 0;

	do {
		unsigned int NalLength;
		uint8_t NalData[4];

		memcpy(NalData, packet->data + VideoPosition, NalLengthBytes);
		VideoPosition += NalLengthBytes;
		NalStart += NalLengthBytes;
		switch (NalLengthBytes) {
			case 1:
				NalLength = (NalData[0]);
				break;
			case 2:
				NalLength = (NalData[0] << 8) | (NalData[1]);
				break;
			case 3:
				NalLength = (NalData[0] << 16) | (NalData[1] << 8) | (NalData[2]);
				break;
			default:
				NalLength = (NalData[0] << 24) | (NalData[1] << 16) | (NalData[2] << 8) | (NalData[3]);
				break;
		}

		if (NalStart + NalLength > SampleSize) {
			fprintf(stderr, "nal length past end of buffer - size %u frame offset %u left %u\n", NalLength, NalStart, SampleSize - NalStart);
			NalStart = SampleSize;
		} else {
			NalStart += NalLength;
			ic = 0;
			iov[ic++].iov_base = PesHeader;

			iov[ic].iov_base = Head;
			iov[ic++].iov_len = sizeof(Head);

			iov[ic].iov_base = packet->data + VideoPosition;
			iov[ic++].iov_len = NalLength;

			VideoPosition += NalLength;

			iov[0].iov_len = InsertPesHeader(PesHeader, NalLength, MPEG_VIDEO_PES_START_CODE, pts, 0);
			ssize_t l = writev(fd, iov, ic);
			if (l < 0)
				return false;
			len += l;

			pts = INVALID_PTS_VALUE;
		}
	} while (NalStart < SampleSize);

	return len > -1;
}

WriterH264::WriterH264()
{
	Register(this, AV_CODEC_ID_H264, VIDEO_ENCODING_H264);
	Init();
}

static WriterH264 writerh264 __attribute__ ((init_priority (300)));
