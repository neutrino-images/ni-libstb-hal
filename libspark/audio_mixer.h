/*
 * audio_mixer.h
 *
 * (C) 2012 martii
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AUDIO_MIXER_H__
#define __AUDIO_MIXER_H__

#include <alsa/asoundlib.h>

class mixerVolume
{
	private:
		long min, max;
		snd_mixer_t *handle;
		snd_mixer_elem_t *elem;
	public:
		mixerVolume(const char *selem_name, const char *Card, long volume = -1);
		~mixerVolume(void);
		bool setVolume(long volume);
};

#endif // __AUDIO_MIXER_H__
