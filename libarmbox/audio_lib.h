/* public header file */

#ifndef _AUDIO_LIB_H_
#define _AUDIO_LIB_H_

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

typedef enum
{
	AUDIO_FMT_UNKNOWN = -1,
	AUDIO_FMT_DOLBY_DIGITAL = 0,
	AUDIO_FMT_MPEG = 1,
	AUDIO_FMT_DTS = 2,
	AUDIO_FMT_LPCM = 6,
	AUDIO_FMT_AAC = 8,
	AUDIO_FMT_AAC_HE = 9,
	AUDIO_FMT_MP3 = 0xa,
	AUDIO_FMT_AAC_PLUS = 0xb,
	AUDIO_FMT_DTS_HD = 0x10,
	AUDIO_FMT_WMA = 0x20,
	AUDIO_FMT_WMA_PRO = 0x21,
	AUDIO_FMT_DD_PLUS = 0x22,
	AUDIO_FMT_AMR = 0x23,
	AUDIO_FMT_RAW = 0xf
} AUDIO_FORMAT;

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

		AUDIO_FORMAT	StreamType;
		AUDIO_SYNC_MODE    SyncMode;
		bool started;

		int volume;

		int do_mute(bool enable, bool remember);
		void setBypassMode(bool disable);

	public:
		/* construct & destruct */
		cAudio(void *, void *, void *);
		~cAudio(void);

		void openDevice(void);
		void closeDevice(void);

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
		void SetStreamType(AUDIO_FORMAT type);
		AUDIO_FORMAT GetStreamType(void) { return StreamType; }
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
};

#endif
