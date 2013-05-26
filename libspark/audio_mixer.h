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
		snd_mixer_elem_t* elem;
		snd_mixer_selem_id_t *sid;

		void *alsaLib;
		int (*snd_card_get_index)(const char *name);
		int (*snd_mixer_open)(snd_mixer_t **mixer, int mode);
		int (*snd_mixer_attach)(snd_mixer_t *mixer, const char *name);
		int (*snd_mixer_selem_register)(snd_mixer_t *mixer, struct snd_mixer_selem_regopt *options, snd_mixer_class_t **classp);
		int (*snd_mixer_load)(snd_mixer_t *mixer);
		void (*snd_mixer_selem_id_set_index)(snd_mixer_selem_id_t *obj, unsigned int val);
		void (*snd_mixer_selem_id_set_name)(snd_mixer_selem_id_t *obj, const char *val);
		snd_mixer_elem_t *(*snd_mixer_find_selem)(snd_mixer_t *mixer, const snd_mixer_selem_id_t *id);
		int (*snd_mixer_selem_get_playback_volume_range)(snd_mixer_elem_t *elem, long *min, long *max);
		int (*snd_mixer_close)(snd_mixer_t *mixer);
		void (*snd_mixer_selem_id_free)(snd_mixer_selem_id_t *obj);
		int (*snd_mixer_selem_set_playback_volume_all)(snd_mixer_elem_t *elem, long value);
		size_t (*snd_mixer_selem_id_sizeof)(void);
	public:
		mixerVolume(const char *selem_name, const char *Card, long volume = -1);
		~mixerVolume(void);
		bool setVolume(long volume);
};
#endif

