/*
 * audio_mixer.cpp
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

#include <audio_mixer.h>

mixerVolume::mixerVolume(const char *name, const char *card, long volume)
{
	snd_mixer_selem_id_t *sid = NULL;
	elem = NULL;
	handle = NULL;
	min = 0;
	max = 100;
	char cardId[10];

	if (!name || !card)
		return;

	int cx = snd_card_get_index(card);
	if (cx < 0 || cx > 31)
		return;
	snprintf(cardId, sizeof(cardId), "hw:%i", cx);

	if (0 > snd_mixer_open(&handle, 0))
		return;
	if (0 > snd_mixer_attach(handle, cardId))
		return;
	if (0 > snd_mixer_selem_register(handle, NULL, NULL))
		return;
	if (0 > snd_mixer_load(handle))
		return;
	snd_mixer_selem_id_alloca(&sid);
	if (!sid)
		return;
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, name);
	elem = snd_mixer_find_selem(handle, sid);
	if (elem)
	{
		snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
		setVolume(volume);
	}
}
mixerVolume::~mixerVolume()
{
	if (handle)
		snd_mixer_close(handle);
}

bool mixerVolume::setVolume(long volume)
{
	return elem
	    && (volume > -1)
	    && (volume < 101)
	    && !snd_mixer_selem_set_playback_volume_all(elem, min + volume * (max - min) / 100);
}
