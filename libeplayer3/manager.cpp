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

#include <stdlib.h>
#include <string.h>
#include "manager.h"
#include "player.h"

void Manager::addTrack(std::map<int,Track*> &tracks, Track &track)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::map<int,Track*>::iterator it = tracks.find(track.pid);
	if (it == tracks.end()) {
		Track *t = new Track;
		*t = track;
		tracks[track.pid] = t;
	} else
		*it->second = track;
}

void Manager::addVideoTrack(Track &track)
{
	addTrack(videoTracks, track);
}

void Manager::addAudioTrack(Track &track)
{
	addTrack(audioTracks, track);
}

void Manager::addSubtitleTrack(Track &track)
{
	addTrack(subtitleTracks, track);
}

void Manager::addTeletextTrack(Track &track)
{
	addTrack(teletextTracks, track);
}

std::vector<Track> Manager::getTracks(std::map<int,Track*> &tracks)
{
	player->input.UpdateTracks();
	std::vector<Track> res;
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for(std::map<int,Track*>::iterator it = tracks.begin(); it != tracks.end(); ++it)
		if (!it->second->inactive)
			res.push_back(*it->second);
	return res;
}

std::vector<Track> Manager::getVideoTracks()
{
	return getTracks(videoTracks);
}

std::vector<Track> Manager::getAudioTracks()
{
	return getTracks(audioTracks);
}

std::vector<Track> Manager::getSubtitleTracks()
{
	return getTracks(subtitleTracks);
}

std::vector<Track> Manager::getTeletextTracks()
{
	return getTracks(teletextTracks);
}

Track *Manager::getTrack(std::map<int,Track*> &tracks, int pid)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::map<int,Track*>::iterator it = tracks.find(pid);
	if (it != tracks.end() && !it->second->inactive)
		return it->second;
	return NULL;
}
Track *Manager::getVideoTrack(int pid)
{
	return getTrack(videoTracks, pid);
}

Track *Manager::getAudioTrack(int pid)
{
	return getTrack(audioTracks, pid);
}

Track *Manager::getSubtitleTrack(int pid)
{
	return getTrack(subtitleTracks, pid);
}

Track *Manager::getTeletextTrack(int pid)
{
	return getTrack(teletextTracks, pid);
}

bool Manager::initTrackUpdate()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);

	for (std::map<int,Track*>::iterator it = audioTracks.begin(); it != audioTracks.end(); ++it)
		it->second->inactive = !it->second->is_static;

	for (std::map<int, Track*>::iterator it = videoTracks.begin(); it != videoTracks.end(); ++it)
		it->second->inactive = !it->second->is_static;

	for (std::map<int,Track*>::iterator it = subtitleTracks.begin(); it != subtitleTracks.end(); ++it)
		it->second->inactive = !it->second->is_static;

	for (std::map<int,Track*>::iterator it = teletextTracks.begin(); it != teletextTracks.end(); ++it)
		it->second->inactive = !it->second->is_static;

	return true;
}

void Manager::clearTracks()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);

	for (std::map<int,Track*>::iterator it = audioTracks.begin(); it != audioTracks.end(); ++it)
		delete it->second;
	audioTracks.clear();

	for (std::map<int, Track*>::iterator it = videoTracks.begin(); it != videoTracks.end(); ++it)
		delete it->second;
	videoTracks.clear();

	for (std::map<int,Track*>::iterator it = subtitleTracks.begin(); it != subtitleTracks.end(); ++it)
		delete it->second;
	subtitleTracks.clear();

	for (std::map<int,Track*>::iterator it = teletextTracks.begin(); it != teletextTracks.end(); ++it)
		delete it->second;
	teletextTracks.clear();
}

Manager::~Manager()
{
	clearTracks();
}
