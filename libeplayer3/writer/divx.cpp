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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <errno.h>

#include "misc.h"
#include "pes.h"
#include "writer.h"

class WriterDIVX : public Writer
{
	private:
		bool initialHeader;
	public:
		bool Write(int fd, AVFormatContext *avfc, AVStream *stream, AVPacket *packet, int64_t pts);
		void Init();
		WriterDIVX();
};

void WriterDIVX::Init()
{
	initialHeader = true;
}

bool WriterDIVX::Write(int fd, AVFormatContext * /* avfc */, AVStream *stream, AVPacket *packet, int64_t pts)
{
	if (fd < 0 || !packet)
		return false;

	uint8_t PesHeader[PES_MAX_HEADER_SIZE];
	uint8_t FakeHeaders[64];	// 64bytes should be enough to make the fake headers
	unsigned int FakeHeaderLength;
	uint8_t Version = 5;
	unsigned int FakeStartCode = (Version << 8) | PES_VERSION_FAKE_START_CODE;
	unsigned int usecPerFrame = 41708;	/* Hellmaster1024: default value */
	BitPacker_t ld = { FakeHeaders, 0, 32 };

	usecPerFrame = 1000000 / av_q2d(stream->r_frame_rate);

	memset(FakeHeaders, 0, sizeof(FakeHeaders));

	/* Create info record for frame parser */
	/* divx4 & 5
	   VOS
	   PutBits(&ld, 0x0, 8);
	   PutBits(&ld, 0x0, 8);
	 */
	PutBits(&ld, 0x1b0, 32);	// startcode
	PutBits(&ld, 0, 8);		// profile = reserved
	PutBits(&ld, 0x1b2, 32);	// startcode (user data)
	PutBits(&ld, 0x53545443, 32);	// STTC - an embedded ST timecode from an avi file
	PutBits(&ld, usecPerFrame, 32);
	// microseconds per frame
	FlushBits(&ld);

	FakeHeaderLength = (ld.Ptr - (FakeHeaders));

	struct iovec iov[4];
	int ic = 0;
	iov[ic].iov_base = PesHeader;
	iov[ic++].iov_len = InsertPesHeader(PesHeader, packet->size, MPEG_VIDEO_PES_START_CODE, pts, FakeStartCode);
	iov[ic].iov_base = FakeHeaders;
	iov[ic++].iov_len = FakeHeaderLength;

	if (initialHeader) {
		iov[ic].iov_base = stream->codec->extradata;
		iov[ic++].iov_len = stream->codec->extradata_size;
		initialHeader = false;
	}
	iov[ic].iov_base = packet->data;
	iov[ic++].iov_len = packet->size;

	return writev(fd, iov, ic) > -1;
}

WriterDIVX::WriterDIVX()
{
	Register(this, AV_CODEC_ID_MPEG4, VIDEO_ENCODING_MPEG4P2);
	Register(this, AV_CODEC_ID_MSMPEG4V1, VIDEO_ENCODING_MPEG4P2);
	Register(this, AV_CODEC_ID_MSMPEG4V2, VIDEO_ENCODING_MPEG4P2);
	Register(this, AV_CODEC_ID_MSMPEG4V3, VIDEO_ENCODING_MPEG4P2);
}

static WriterDIVX writer_divx __attribute__ ((init_priority (300)));
