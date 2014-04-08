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

class WriterMPEG2 : public Writer
{
	public:
		bool Write(int fd, AVFormatContext *avfc, AVStream *stream, AVPacket *packet, int64_t pts);
		WriterMPEG2();
};

bool WriterMPEG2::Write(int fd, AVFormatContext * /* avfc */, AVStream * /* stream */, AVPacket *packet, int64_t pts)
{
	if (fd < 0 || !packet)
		return false;

	unsigned char PesHeader[PES_MAX_HEADER_SIZE];

	for (int Position = 0; Position < packet->size; ) {
		int PacketLength = std::min(packet->size - Position, MAX_PES_PACKET_SIZE);
		struct iovec iov[2];
		iov[0].iov_base = PesHeader;
		iov[0].iov_len = InsertPesHeader(PesHeader, PacketLength, 0xe0, pts, 0);
		iov[1].iov_base = packet->data + Position;
		iov[1].iov_len = PacketLength;

		ssize_t l = writev(fd, iov, 2);
		if (l < 0) {
			return false;
			break;
		}
		Position += PacketLength;
		pts = INVALID_PTS_VALUE;
	}
	return true;
}

WriterMPEG2::WriterMPEG2()
{
	Register(this, AV_CODEC_ID_MPEG2TS, VIDEO_ENCODING_AUTO);
}

static WriterMPEG2 writer_mpeg2 __attribute__ ((init_priority (300)));
