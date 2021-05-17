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

#define PES_AUDIO_PRIVATE_HEADER_SIZE   16  // consider maximum private header size.
#define PES_AUDIO_HEADER_SIZE       (32 + PES_AUDIO_PRIVATE_HEADER_SIZE)

class WriterDTS : public Writer
{
	public:
		bool Write(AVPacket *packet, int64_t pts);
		WriterDTS();
};

bool WriterDTS::Write(AVPacket *packet, int64_t pts)
{
	if (!packet || !packet->data)
		return false;

	uint8_t PesHeader[PES_AUDIO_HEADER_SIZE];

// #define DO_BYTESWAP
#ifdef DO_BYTESWAP
	uint8_t Data[packet->size];
	memcpy(Data, packet->data, packet->size);

	/* 16-bit byte swap all data before injecting it */
	for (i = 0; i < packet->size; i += 2)
	{
		uint8_t Tmp = Data[i];
		Data[i] = Data[i + 1];
		Data[i + 1] = Tmp;
	}
#endif

	struct iovec iov[2];

	iov[0].iov_base = PesHeader;
	iov[0].iov_len = InsertPesHeader(PesHeader, packet->size, MPEG_AUDIO_PES_START_CODE /*PRIVATE_STREAM_1_PES_START_CODE */, pts, 0);
#ifdef DO_BYTESPWAP
	iov[1].iov_base = Data;
#else
	iov[1].iov_base = packet->data;
#endif
	iov[1].iov_len = packet->size;

	return writev(fd, iov, 2) > -1;
}

WriterDTS::WriterDTS()
{
	Register(this, AV_CODEC_ID_DTS, AUDIO_ENCODING_DTS);
}

static WriterDTS writer_dts __attribute__((init_priority(300)));
