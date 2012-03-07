#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/dvb/audio.h>
#include "audio_lib.h"
#include "lt_debug.h"

#define AUDIO_DEVICE	"/dev/dvb/adapter0/audio0"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_AUDIO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_AUDIO, this, args)

#include <linux/soundcard.h>

cAudio * audioDecoder = NULL;

cAudio::cAudio(void *, void *, void *)
{
	fd = -1;
	clipfd = -1;
	mixer_fd = -1;
	openDevice();
	Muted = false;
}

cAudio::~cAudio(void)
{
	closeDevice();
}

void cAudio::openDevice(void)
{
	if (fd < 0)
	{
		if ((fd = open(AUDIO_DEVICE, O_RDWR)) < 0)
			lt_info("openDevice: open failed (%m)\n");
		fcntl(fd, F_SETFD, FD_CLOEXEC);
		do_mute(true, false);
	}
	else
		lt_info("openDevice: already open (fd = %d)\n", fd);
}

void cAudio::closeDevice(void)
{
	if (fd >= 0)
		close(fd);
	fd = -1;
	if (clipfd >= 0)
		close(clipfd);
	clipfd = -1;
	if (mixer_fd >= 0)
		close(mixer_fd);
	mixer_fd = -1;
}

int cAudio::do_mute(bool enable, bool remember)
{
	lt_debug("%s(%d, %d)\n", __FUNCTION__, enable, remember);
	char str[4];

	if (remember)
		Muted = enable;

	sprintf(str, "%d", Muted);

	int f = open("/proc/stb/audio/j1_mute", O_RDWR);
	write(f, str, strlen(str));
	close(f);

	if (!enable)
	{
		f = open("/proc/stb/avs/0/volume", O_RDWR);
		read(f, str, 4);
		close(f);
		str[3] = '\0';
		f = open("/proc/stb/avs/0/volume", O_RDWR);
		write(f, str, strlen(str));
		close(f);
	}
	return 0;
}

int map_volume(const int volume)
{
	unsigned char vol = volume;
	if (vol > 100)
		vol = 100;

	vol = 63 - vol * 63 / 100;
	return vol;
}


int cAudio::setVolume(unsigned int left, unsigned int right)
{
	lt_debug("%s(%d, %d)\n", __func__, left, right);

	volume = (left + right) / 2;
	int v = map_volume(volume);
	if (clipfd != -1 && mixer_fd != -1) {
		int tmp = 0;
		/* not sure if left / right is correct here, but it is always the same anyways ;-) */
		if (! Muted)
			tmp = left << 8 | right;
		int ret = ioctl(mixer_fd, MIXER_WRITE(mixer_num), &tmp);
		if (ret == -1)
			lt_info("%s: MIXER_WRITE(%d),%04x: %m\n", __func__, mixer_num, tmp);
		return ret;
	}
	/* compensate for different decoding volume of mpeg and ac3 streams
	 * TODO: check if this is correct for all channels
	 *       and if we need to do something with DTS?  */
	if (StreamType == AUDIO_FMT_MPEG)
		v = map_volume(volume * 30 / 53);

	char str[4];
	sprintf(str, "%d", v);

	int f = open("/proc/stb/avs/0/volume", O_RDWR);
	write(f, str, strlen(str));
	close(f);
	return 0;
}

int cAudio::Start(void)
{
	int ret;
	ret = ioctl(fd, AUDIO_PLAY);
	return ret;
}

int cAudio::Stop(void)
{
	return ioctl(fd, AUDIO_STOP);
}

bool cAudio::Pause(bool /*Pcm*/)
{
	return true;
};

void cAudio::SetSyncMode(AVSYNC_TYPE Mode)
{
	lt_debug("%s %d\n", __func__, Mode);
	ioctl(fd, AUDIO_SET_AV_SYNC, Mode);
};

//AUDIO_ENCODING_AC3
#define AUDIO_STREAMTYPE_AC3 0
//AUDIO_ENCODING_MPEG2
#define AUDIO_STREAMTYPE_MPEG 1
//AUDIO_ENCODING_DTS
#define AUDIO_STREAMTYPE_DTS 2

#define AUDIO_ENCODING_LPCM 2
#define AUDIO_ENCODING_LPCMA 11
void cAudio::SetStreamType(AUDIO_FORMAT type)
{
	int bypass = AUDIO_STREAMTYPE_MPEG;
	bool update_volume = (StreamType != type);
	lt_debug("%s %d\n", __FUNCTION__, type);
	StreamType = type;

	switch (type)
	{
		case AUDIO_FMT_DOLBY_DIGITAL:
			bypass = AUDIO_STREAMTYPE_AC3;
			break;
		case AUDIO_FMT_DTS:
			bypass = AUDIO_STREAMTYPE_DTS;
			break;
		case AUDIO_FMT_MPEG:
		default:
			break;
	}

	// Normaly the encoding should be set using AUDIO_SET_ENCODING
	// But as we implemented the behavior to bypass (cause of e2) this is correct here
	if (ioctl(fd, AUDIO_SET_BYPASS_MODE, bypass) < 0)
		lt_info("%s: AUDIO_SET_BYPASS_MODE failed (%m)\n", __func__);
	if (update_volume)
		setVolume(volume, volume);
};

int cAudio::setChannel(int channel)
{
	lt_debug("%s %d\n", __FUNCTION__, channel);
	return 0;
};

int cAudio::PrepareClipPlay(int ch, int srate, int bits, int little_endian)
{
	int fmt;
	unsigned int devmask, stereo, usable;
	const char *dsp_dev = getenv("DSP_DEVICE");
	const char *mix_dev = getenv("MIX_DEVICE");
	lt_debug("%s ch %d srate %d bits %d le %d\n", __FUNCTION__, ch, srate, bits, little_endian);
	if (clipfd >= 0) {
		lt_info("%s: clipfd already opened (%d)\n", __FUNCTION__, clipfd);
		return -1;
	}
	mixer_num = -1;
	mixer_fd = -1;
	/* a different DSP device can be given with DSP_DEVICE and MIX_DEVICE
	 * if this device cannot be opened, we fall back to the internal OSS device
	 * Example:
	 *   modprobe snd-usb-audio
	 *   export DSP_DEVICE=/dev/sound/dsp2
	 *   export MIX_DEVICE=/dev/sound/mixer2
	 *   neutrino
	 */
	if ((!dsp_dev) || (access(dsp_dev, W_OK))) {
		if (dsp_dev)
			lt_info("%s: DSP_DEVICE is set (%s) but cannot be opened,"
				" fall back to /dev/dsp1\n", __func__, dsp_dev);
		dsp_dev = "/dev/dsp1";
	}
	lt_info("%s: dsp_dev %s mix_dev %s\n", __func__, dsp_dev, mix_dev); /* NULL mix_dev is ok */
	/* the tdoss dsp driver seems to work only on the second open(). really. */
	clipfd = open(dsp_dev, O_WRONLY);
	if (clipfd < 0) {
		lt_info("%s open %s: %m\n", dsp_dev, __FUNCTION__);
		return -1;
	}
	fcntl(clipfd, F_SETFD, FD_CLOEXEC);
	/* no idea if we ever get little_endian == 0 */
	if (little_endian)
		fmt = AFMT_S16_BE;
	else
		fmt = AFMT_S16_LE;
	if (ioctl(clipfd, SNDCTL_DSP_SETFMT, &fmt))
		perror("SNDCTL_DSP_SETFMT");
	if (ioctl(clipfd, SNDCTL_DSP_CHANNELS, &ch))
		perror("SNDCTL_DSP_CHANNELS");
	if (ioctl(clipfd, SNDCTL_DSP_SPEED, &srate))
		perror("SNDCTL_DSP_SPEED");
	if (ioctl(clipfd, SNDCTL_DSP_RESET))
		perror("SNDCTL_DSP_RESET");

	if (!mix_dev)
		return 0;

	mixer_fd = open(mix_dev, O_RDWR);
	if (mixer_fd < 0) {
		lt_info("%s: open mixer %s failed (%m)\n", __func__, mix_dev);
		/* not a real error */
		return 0;
	}
	if (ioctl(mixer_fd, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
		lt_info("%s: SOUND_MIXER_READ_DEVMASK %m\n", __func__);
		devmask = 0;
	}
	if (ioctl(mixer_fd, SOUND_MIXER_READ_STEREODEVS, &stereo) == -1) {
		lt_info("%s: SOUND_MIXER_READ_STEREODEVS %m\n", __func__);
		stereo = 0;
	}
	usable = devmask & stereo;
	if (usable == 0) {
		lt_info("%s: devmask: %08x stereo: %08x, no usable dev :-(\n",
			__func__, devmask, stereo);
		close(mixer_fd);
		mixer_fd = -1;
		return 0; /* TODO: should we treat this as error? */
	}
	/* __builtin_popcount needs GCC, it counts the set bits... */
	if (__builtin_popcount (usable) != 1) {
		/* TODO: this code is not yet tested as I have only single-mixer devices... */
		lt_info("%s: more than one mixer control: devmask %08x stereo %08x\n"
			"%s: querying MIX_NUMBER environment variable...\n",
			__func__, devmask, stereo, __func__);
		const char *tmp = getenv("MIX_NUMBER");
		if (tmp)
			mixer_num = atoi(tmp);
		lt_info("%s: mixer_num is %d -> device %08x\n",
			__func__, (mixer_num >= 0) ? (1 << mixer_num) : 0);
		/* no error checking, you'd better know what you are doing... */
	} else {
		mixer_num = 0;
		while (!(usable & 0x01)) {
			mixer_num++;
			usable >>= 1;
		}
	}
	setVolume(volume, volume);

	return 0;
};

int cAudio::WriteClip(unsigned char *buffer, int size)
{
	int ret;
	// lt_debug("cAudio::%s\n", __FUNCTION__);
	if (clipfd <= 0) {
		lt_info("%s: clipfd not yet opened\n", __FUNCTION__);
		return -1;
	}
	ret = write(clipfd, buffer, size);
	if (ret < 0)
		lt_info("%s: write error (%m)\n", __FUNCTION__);
	return ret;
};

int cAudio::StopClip()
{
	lt_debug("%s\n", __FUNCTION__);
	if (clipfd <= 0) {
		lt_info("%s: clipfd not yet opened\n", __FUNCTION__);
		return -1;
	}
	close(clipfd);
	clipfd = -1;
	if (mixer_fd >= 0)
		close(mixer_fd);
	mixer_fd = -1;
	setVolume(volume, volume);
	return 0;
};

void cAudio::getAudioInfo(int &type, int &layer, int &freq, int &bitrate, int &mode)
{
	lt_debug("%s\n", __FUNCTION__);
	type = 0;
	layer = 0;
	freq = 0;
	bitrate = 0;
	mode = 0;
#if 0
	unsigned int atype;
	static const int freq_mpg[] = {44100, 48000, 32000, 0};
	static const int freq_ac3[] = {48000, 44100, 32000, 0};
	scratchl2 i;
	if (ioctl(fd, MPEG_AUD_GET_DECTYP, &atype) < 0)
		perror("cAudio::getAudioInfo MPEG_AUD_GET_DECTYP");
	if (ioctl(fd, MPEG_AUD_GET_STATUS, &i) < 0)
		perror("cAudio::getAudioInfo MPEG_AUD_GET_STATUS");

	type = atype;
#if 0
/* this does not work, some of the values are negative?? */
	AMPEGStatus A;
	memcpy(&A, &i.word00, sizeof(i.word00));
	layer   = A.audio_mpeg_layer;
	mode    = A.audio_mpeg_mode;
	bitrate = A.audio_mpeg_bitrate;
	switch(A.audio_mpeg_frequency)
#endif
	/* layer and bitrate are not used anyway... */
	layer   = 0; //(i.word00 >> 17) & 3;
	bitrate = 0; //(i.word00 >> 12) & 3;
	switch (type)
	{
		case 0:	/* MPEG */
			mode = (i.word00 >> 6) & 3;
			freq = freq_mpg[(i.word00 >> 10) & 3];
			break;
		case 1:	/* AC3 */
			mode = (i.word00 >> 28) & 7;
			freq = freq_ac3[(i.word00 >> 16) & 3];
			break;
		default:
			mode = 0;
			freq = 0;
	}
	//fprintf(stderr, "type: %d layer: %d freq: %d bitrate: %d mode: %d\n", type, layer, freq, bitrate, mode);
#endif
};

void cAudio::SetSRS(int /*iq_enable*/, int /*nmgr_enable*/, int /*iq_mode*/, int /*iq_level*/)
{
	lt_debug("%s\n", __FUNCTION__);
};

void cAudio::SetSpdifDD(bool enable)
{
	lt_debug("%s %d\n", __FUNCTION__, enable);
};

void cAudio::ScheduleMute(bool On)
{
	lt_debug("%s %d\n", __FUNCTION__, On);
};

void cAudio::EnableAnalogOut(bool enable)
{
	lt_debug("%s %d\n", __FUNCTION__, enable);
};

#define AUDIO_BYPASS_ON  0
#define AUDIO_BYPASS_OFF 1
void cAudio::setBypassMode(bool disable)
{
	lt_debug("%s %d\n", __FUNCTION__, disable);
	int mode = disable ? AUDIO_BYPASS_OFF : AUDIO_BYPASS_ON;
	if (ioctl(fd, AUDIO_SET_BYPASS_MODE, mode) < 0)
		lt_info("%s AUDIO_SET_BYPASS_MODE %d: %m\n", __func__, mode);
	return;
}
