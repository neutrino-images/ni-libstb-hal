/*
 * manager handling.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdlib.h>
#include <string.h>
#include "manager.h"

void Manager::addVideoTrack(Track &track)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	Track *t = new Track;
	*t = track;
	videoTracks[track.pid] = t;
}

void Manager::addAudioTrack(Track &track)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	Track *t = new Track;
	*t = track;
	audioTracks[track.pid] = t;
}

void Manager::addSubtitleTrack(Track &track)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	Track *t = new Track;
	*t = track;
	subtitleTracks[track.pid] = t;
}

void Manager::addTeletextTrack(Track &track)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	Track *t = new Track;
	*t = track;
	teletextTracks[track.pid] = t;
}

std::vector<Track> Manager::getVideoTracks()
{
//	input.UpdateTracks();
	std::vector<Track> res;
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for(std::map<int,Track*>::iterator it = videoTracks.begin(); it != videoTracks.end(); ++it)
		if (!it->second->inactive)
			res.push_back(*it->second);
	return res;
}

std::vector<Track> Manager::getAudioTracks()
{
//	input.UpdateTracks();
	std::vector<Track> res;
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for(std::map<int,Track*>::iterator it = audioTracks.begin(); it != audioTracks.end(); ++it)
		if (!it->second->inactive)
			res.push_back(*it->second);
	return res;
}

std::vector<Track> Manager::getSubtitleTracks()
{
//	input.UpdateTracks();
	std::vector<Track> res;
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for(std::map<int,Track*>::iterator it = subtitleTracks.begin(); it != subtitleTracks.end(); ++it)
		if (!it->second->inactive)
			res.push_back(*it->second);
	return res;
}

std::vector<Track> Manager::getTeletextTracks()
{
//	input.UpdateTracks();
	std::vector<Track> res;
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for(std::map<int,Track*>::iterator it = teletextTracks.begin(); it != teletextTracks.end(); ++it)
		if (!it->second->inactive)
			res.push_back(*it->second);
	return res;
}

Track *Manager::getVideoTrack(int pid)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::map<int,Track*>::iterator it = videoTracks.find(pid);
	if (it != videoTracks.end() && !it->second->inactive)
		return it->second;
	return NULL;
}

Track *Manager::getAudioTrack(int pid)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::map<int,Track*>::iterator it = audioTracks.find(pid);
	if (it != audioTracks.end() && !it->second->inactive)
		return it->second;
	return NULL;
}

Track *Manager::getSubtitleTrack(int pid)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::map<int,Track*>::iterator it = subtitleTracks.find(pid);
	if (it != subtitleTracks.end() && !it->second->inactive)
		return it->second;
	return NULL;
}

Track *Manager::getTeletextTrack(int pid)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::map<int,Track*>::iterator it = teletextTracks.find(pid);
	if (it != teletextTracks.end() && !it->second->inactive)
		return it->second;
	return NULL;
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
