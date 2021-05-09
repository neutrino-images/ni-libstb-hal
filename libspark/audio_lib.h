/* public header file */

#ifndef __AUDIO_LIB_H__
#define __AUDIO_LIB_H__

#include "cs_types.h"

typedef enum
{
	AUDIO_SYNC_WITH_PTS,
	AUDIO_NO_SYNC,
	AUDIO_SYNC_AUDIO_MASTER
} AUDIO_SYNC_MODE;

typedef enum {
	HDMI_ENCODED_OFF,
	HDMI_ENCODED_AUTO,
	HDMI_ENCODED_FORCED
} HDMI_ENCODED_MODE;

class mixerVolume;

class cAudio
{
	friend class cPlayback;
	private:
		int fd;
		bool Muted;

		int clipfd; /* for pcm playback */
		int mixer_fd;  /* if we are using the OSS mixer */
		int mixer_num; /* oss mixer to use, if any */

		int	StreamType;
		AUDIO_SYNC_MODE    SyncMode;
		bool started;

		int volume;

		void openDevice(void);
		void closeDevice(void);

		int do_mute(bool enable, bool remember);
		void setBypassMode(bool disable);

		mixerVolume *mixerAnalog, *mixerHDMI, *mixerSPDIF;
		int volumeAnalog, volumeHDMI, volumeSPDIF;
		bool mixersMuted;

	public:
		/* construct & destruct */
		cAudio(void *, void *, void *);
		~cAudio(void);

		void open_AVInput_Device(void) { return; };
		void close_AVInput_Device(void) { return; };

		void setAVInput(int val);

		void *GetHandle() { return NULL; };
		/* shut up */
		int mute(bool remember = true) { return do_mute(true, remember); };
		int unmute(bool remember = true) { return do_mute(false, remember); };

		/* volume, min = 0, max = 255 */
		int setVolume(unsigned int left, unsigned int right);
		int getVolume(void) { return volume;}
		bool getMuteStatus(void) { return Muted; };

		/* start and stop audio */
		int Start(void);
		int Stop(void);
		bool Pause(bool Pcm = true);
		void SetStreamType(int bypass);
		int GetStreamType(void) { return StreamType; }
		void SetSyncMode(AVSYNC_TYPE Mode);

		/* select channels */
		int setChannel(int channel);
		int PrepareClipPlay(int uNoOfChannels, int uSampleRate, int uBitsPerSample, int bLittleEndian);
		int WriteClip(unsigned char * buffer, int size);
		int StopClip();
		void getAudioInfo(int &type, int &layer, int& freq, int &bitrate, int &mode);
		void SetSRS(int iq_enable, int nmgr_enable, int iq_mode, int iq_level);
		bool IsHdmiDDSupported() { return true; };
		void SetHdmiDD(bool enable);
		void SetSpdifDD(bool enable);
		void ScheduleMute(bool On);
		void EnableAnalogOut(bool enable);

		void openMixers(void);
		void closeMixers(void);
		void setMixerVolume(const char *name, long value, bool remember = true);
		void muteMixers(bool m = true);
};

#endif // __AUDIO_LIB_H__
