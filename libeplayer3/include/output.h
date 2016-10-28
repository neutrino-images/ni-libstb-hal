/*
 * output class
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

#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#include <scoped_lock.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include "writer.h"

class Player;

class Output
{
	friend class Player;

	private:
		int videofd;
		int audiofd;
		Writer *videoWriter, *audioWriter;
		Mutex audioMutex, videoMutex;
		Track *audioTrack, *videoTrack;
		Player *player;
	public:
		Output();
		~Output();
		bool Open();
		bool Close();
		bool Play();
		bool Stop();
		bool Pause();
		bool Continue();
		bool Mute(bool);
		bool Flush();
		bool FastForward(int speed);
		bool SlowMotion(int speed);
		bool AVSync(bool);
		bool Clear();
		bool ClearAudio();
		bool ClearVideo();
		bool GetPts(int64_t &pts);
		bool GetFrameCount(int64_t &framecount);
		bool SwitchAudio(Track *track);
		bool SwitchVideo(Track *track);
		bool Write(AVStream *stream, AVPacket *packet, int64_t Pts);
};

#endif
