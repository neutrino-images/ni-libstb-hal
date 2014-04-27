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

class WriterFLAC : public Writer
{
	public:
		bool Write(int fd, AVFormatContext * /* avfc */, AVStream *stream, AVPacket *packet, int64_t pts);
		WriterFLAC();
};

bool WriterFLAC::Write(int fd, AVFormatContext * /* avfc */, AVStream * /* stream */, AVPacket *packet, int64_t pts)
{
	if (fd < 0 || !packet || !packet->data)
		return false;

	uint8_t PesHeader[PES_MAX_HEADER_SIZE];

	for (int pos = 0; pos < packet->size; ) {
		int PacketLength = std::min(packet->size - pos, MAX_PES_PACKET_SIZE);
		struct iovec iov[2];
		iov[0].iov_base = PesHeader;
		iov[0].iov_len = InsertPesHeader(PesHeader, PacketLength, MPEG_AUDIO_PES_START_CODE, pts, 0);
		iov[1].iov_base = packet->data + pos;
		iov[1].iov_len = PacketLength;

		ssize_t l = writev(fd, iov, 2);
		if (l < 0)
			return false;
		pos += PacketLength;
		pts = INVALID_PTS_VALUE;
	}
	return true;
}

WriterFLAC::WriterFLAC()
{
	Register(this, AV_CODEC_ID_FLAC, AUDIO_ENCODING_LPCM);
}

static WriterFLAC writer_flac __attribute__ ((init_priority (300)));
