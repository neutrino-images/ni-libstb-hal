/*
 * linuxdvb output/writer handling
 *
 * Copyright (C) 2010  crow
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
#include <errno.h>
#include <sys/uio.h>

#include "misc.h"
#include "pes.h"
#include "writer.h"

class WriterH263 : public Writer
{
	public:
		bool Write(AVPacket *packet, int64_t pts);
		WriterH263();
};

bool WriterH263::Write(AVPacket *packet, int64_t pts)
{
	if (!packet || !packet->data)
		return false;
	uint8_t PesHeader[PES_MAX_HEADER_SIZE];

	int HeaderLength = InsertPesHeader(PesHeader, packet->size, H263_VIDEO_PES_START_CODE, pts, 0);

	int PrivateHeaderLength = InsertVideoPrivateDataHeader(&PesHeader[HeaderLength], packet->size);

	int PesLength = PesHeader[PES_LENGTH_BYTE_0] + (PesHeader[PES_LENGTH_BYTE_1] << 8) + PrivateHeaderLength;

	PesHeader[PES_LENGTH_BYTE_0] = PesLength & 0xff;
	PesHeader[PES_LENGTH_BYTE_1] = (PesLength >> 8) & 0xff;
	PesHeader[PES_HEADER_DATA_LENGTH_BYTE] += PrivateHeaderLength;
	PesHeader[PES_FLAGS_BYTE] |= PES_EXTENSION_DATA_PRESENT;

	HeaderLength += PrivateHeaderLength;

	struct iovec iov[2];
	iov[0].iov_base = PesHeader;
	iov[0].iov_len = HeaderLength;
	iov[1].iov_base = packet->data;
	iov[1].iov_len = packet->size;
	return writev(fd, iov, 2) > -1;
}

WriterH263::WriterH263()
{
	Register(this, AV_CODEC_ID_H263, VIDEO_ENCODING_H263);
	Register(this, AV_CODEC_ID_H263P, VIDEO_ENCODING_H263);
	Register(this, AV_CODEC_ID_H263I, VIDEO_ENCODING_H263);
	Register(this, AV_CODEC_ID_FLV1, VIDEO_ENCODING_FLV1);
}

static WriterH263 writer_h263 __attribute__((init_priority(300)));
