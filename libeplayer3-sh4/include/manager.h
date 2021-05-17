/*
 * manager class
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

#ifndef __MANAGER_H__
#define __MANAGER_H__

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#include <OpenThreads/ScopedLock>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

class Player;

struct Track
{
	std::string title;
	int pid;
	AVStream *stream;
	bool inactive;
	bool hidden; // not part of currently selected program
	bool is_static;
	int ac3flags;
	int type, mag, page; // for teletext
	Track() : pid(-1), stream(NULL), inactive(false), hidden(false), is_static(false), ac3flags(0) {}
};

struct Program
{
	int id;
	std::string title;
	std::vector<AVStream *> streams;
};

class Manager
{
		friend class Player;

	private:
		Player *player;
		OpenThreads::Mutex mutex;
		std::map<int, Track *> videoTracks, audioTracks, subtitleTracks, teletextTracks;
		std::map<int, Program> Programs;
		void addTrack(std::map<int, Track *> &tracks, Track &track);
		Track *getTrack(std::map<int, Track *> &tracks, int pid);
		std::vector<Track> getTracks(std::map<int, Track *> &tracks);
	public:
		void addVideoTrack(Track &track);
		void addAudioTrack(Track &track);
		void addSubtitleTrack(Track &track);
		void addTeletextTrack(Track &track);
		void addProgram(Program &program);

		std::vector<Track> getVideoTracks();
		std::vector<Track> getAudioTracks();
		std::vector<Track> getSubtitleTracks();
		std::vector<Track> getTeletextTracks();
		std::vector<Program> getPrograms();
		bool selectProgram(const int id);

		Track *getVideoTrack(int pid);
		Track *getAudioTrack(int pid);
		Track *getSubtitleTrack(int pid);
		Track *getTeletextTrack(int pid);

		bool initTrackUpdate();
		void clearTracks();

		~Manager();
};
#endif
