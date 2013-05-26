/*
 * audio_mixer.c
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
#include <dlfcn.h>

mixerVolume::mixerVolume(const char *name, const char *card, long volume) {
	sid = NULL;
	elem = NULL;
	handle = NULL;
	min = 0;
	max = 100;
	char cardId[10];

	alsaLib = dlopen("/lib/libasound.so.2", RTLD_NOW);
	if (!alsaLib)
		return;
	if (!(this->snd_card_get_index = (int (*)(const char *))dlsym(alsaLib, "snd_card_get_index"))
	 || !(this->snd_mixer_open = (int (*)(snd_mixer_t **, int)) dlsym(alsaLib, "snd_mixer_open"))
	 || !(this->snd_mixer_attach = (int (*)(snd_mixer_t *, const char *)) dlsym(alsaLib, "snd_mixer_attach"))
	 || !(this->snd_mixer_selem_register = (int (*)(snd_mixer_t *, struct snd_mixer_selem_regopt *, snd_mixer_class_t **)) dlsym(alsaLib, "snd_mixer_selem_register"))
	 || !(this->snd_mixer_load = (int (*)(snd_mixer_t *)) dlsym(alsaLib, "snd_mixer_load"))
	 || !(this->snd_mixer_selem_id_set_index = (void (*)(snd_mixer_selem_id_t *, unsigned int)) dlsym(alsaLib, "snd_mixer_selem_id_set_index"))
	 || !(this->snd_mixer_selem_id_set_name = (void (*)(snd_mixer_selem_id_t *, const char *)) dlsym(alsaLib, "snd_mixer_selem_id_set_name"))
	 || !(this->snd_mixer_find_selem = (snd_mixer_elem_t *(*)(snd_mixer_t *, const snd_mixer_selem_id_t *)) dlsym(alsaLib, "snd_mixer_find_selem"))
	 || !(this->snd_mixer_selem_get_playback_volume_range = (int (*)(snd_mixer_elem_t *, long *, long *)) dlsym(alsaLib, "snd_mixer_selem_get_playback_volume_range"))
	 || !(this->snd_mixer_close = (int (*)(snd_mixer_t *)) dlsym(alsaLib, "snd_mixer_close"))
	 || !(this->snd_mixer_selem_id_free = (void (*)(snd_mixer_selem_id_t *)) dlsym(alsaLib, "snd_mixer_selem_id_free"))
	 || !(this->snd_mixer_selem_set_playback_volume_all = (int (*)(snd_mixer_elem_t *, long)) dlsym(alsaLib, "snd_mixer_selem_set_playback_volume_all")))
		return;

	if (!name || !card)
		return;

	int cx = snd_card_get_index(card);
	if (cx < 0 || cx > 31)
		return;
	snprintf(cardId, sizeof(cardId), "hw:%i", cx);

	if (0 > this->snd_mixer_open(&handle, 0))
		return;
	if (0 > this->snd_mixer_attach(handle, cardId))
		return;
	if (0 > this->snd_mixer_selem_register(handle, NULL, NULL))
		return;
	if (0 > this->snd_mixer_load(handle))
		return;
	snd_mixer_selem_id_alloca(&sid);
	if (!sid)
		return;
	this->snd_mixer_selem_id_set_index(sid, 0);
	this->snd_mixer_selem_id_set_name(sid, name);
	elem = this->snd_mixer_find_selem(handle, sid);
	if (elem) {
		this->snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
		setVolume(volume);
	}
}
mixerVolume::~mixerVolume()
{
	if (handle)
		this->snd_mixer_close(handle);
	if (sid)
		this->snd_mixer_selem_id_free(sid);
	if (alsaLib)
		dlclose(alsaLib);
}

bool mixerVolume::setVolume(long volume) {
	return elem
		&& (volume > -1)
		&& (volume < 101)
		&& !this->snd_mixer_selem_set_playback_volume_all(elem, min + volume * (max - min)/100);
}
