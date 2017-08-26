<<<<<<< HEAD
#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/video_td.h"
#elif HAVE_DUCKBOX_HARDWARE
#include "../libduckbox/video_lib.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/video_lib.h"
#elif HAVE_ARM_HARDWARE
#include "../libarmbox/video_lib.h"
#elif HAVE_AZBOX_HARDWARE
#include "../azbox/video_lib.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../raspi/video_lib.h"
#else
#include "../generic-pc/video_lib.h"
#endif
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
=======
/*
	Copyright 2010-2013 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef _VIDEO_LIB_H
#define _VIDEO_LIB_H

#include <vector>
#include <cs_types.h>

typedef struct cs_vs_format_t
{
	char format[16];
} cs_vs_format_struct_t;

typedef enum {
	ANALOG_SD_RGB_CINCH = 0x00,
	ANALOG_SD_YPRPB_CINCH,
	ANALOG_HD_RGB_CINCH,
	ANALOG_HD_YPRPB_CINCH,
	ANALOG_SD_RGB_SCART = 0x10,
	ANALOG_SD_YPRPB_SCART,
	ANALOG_HD_RGB_SCART,
	ANALOG_HD_YPRPB_SCART,
	ANALOG_SCART_MASK = 0x10
} analog_mode_t;


typedef enum {
	VIDEO_FORMAT_MPEG2 = 0,
	VIDEO_FORMAT_MPEG4, /* H264 */
	VIDEO_FORMAT_VC1,
	VIDEO_FORMAT_JPEG,
	VIDEO_FORMAT_GIF,
	VIDEO_FORMAT_PNG,
	VIDEO_FORMAT_DIVX,/* DIVX 3.11 */
	VIDEO_FORMAT_MPEG4PART2,/* MPEG4 SVH, MPEG4 SP, MPEG4 ASP, DIVX4,5,6 */
	VIDEO_FORMAT_REALVIDEO8,
	VIDEO_FORMAT_REALVIDEO9,
	VIDEO_FORMAT_ON2_VP6,
	VIDEO_FORMAT_ON2_VP8,
	VIDEO_FORMAT_SORENSON_SPARK,
	VIDEO_FORMAT_H263,
	VIDEO_FORMAT_H263_ENCODER,
	VIDEO_FORMAT_H264_ENCODER,
	VIDEO_FORMAT_MPEG4PART2_ENCODER,
	VIDEO_FORMAT_AVS,
	VIDEO_FORMAT_VIP656,
	VIDEO_FORMAT_UNSUPPORTED
} VIDEO_FORMAT;

typedef enum {
	VIDEO_SD = 0,
	VIDEO_HD,
	VIDEO_120x60i,
	VIDEO_320x240i,
	VIDEO_1440x800i,
	VIDEO_360x288i
} VIDEO_DEFINITION;

typedef enum {
	VIDEO_FRAME_RATE_23_976 = 0,
	VIDEO_FRAME_RATE_24,
	VIDEO_FRAME_RATE_25,
	VIDEO_FRAME_RATE_29_97,
	VIDEO_FRAME_RATE_30,
	VIDEO_FRAME_RATE_50,
	VIDEO_FRAME_RATE_59_94,
	VIDEO_FRAME_RATE_60
} VIDEO_FRAME_RATE;

typedef enum {
	DISPLAY_AR_1_1,
	DISPLAY_AR_4_3,
	DISPLAY_AR_14_9,
	DISPLAY_AR_16_9,
	DISPLAY_AR_20_9,
	DISPLAY_AR_RAW,
} DISPLAY_AR;

typedef enum {
	DISPLAY_AR_MODE_PANSCAN = 0,
	DISPLAY_AR_MODE_LETTERBOX,
	DISPLAY_AR_MODE_NONE,
	DISPLAY_AR_MODE_PANSCAN2
} DISPLAY_AR_MODE;

typedef enum {
	VIDEO_DB_DR_NEITHER = 0,
	VIDEO_DB_ON,
	VIDEO_DB_DR_BOTH
} VIDEO_DB_DR;

typedef enum {
	VIDEO_PLAY_STILL = 0,
	VIDEO_PLAY_CLIP,
	VIDEO_PLAY_TRICK,
	VIDEO_PLAY_MOTION,
	VIDEO_PLAY_MOTION_NO_SYNC
} VIDEO_PLAY_MODE;

typedef enum {
	VIDEO_STD_NTSC,
	VIDEO_STD_SECAM,
	VIDEO_STD_PAL,
	VIDEO_STD_480P,
	VIDEO_STD_576P,
	VIDEO_STD_720P60,
	VIDEO_STD_1080I60,
	VIDEO_STD_720P50,
	VIDEO_STD_1080I50,
	VIDEO_STD_1080P30,
	VIDEO_STD_1080P24,
	VIDEO_STD_1080P25,
	VIDEO_STD_1080P50,
	VIDEO_STD_1080P60,
	VIDEO_STD_AUTO,
	VIDEO_STD_MAX
} VIDEO_STD;

/* not used, for dummy functions */
typedef enum {
	VIDEO_HDMI_CEC_MODE_OFF	= 0,
	VIDEO_HDMI_CEC_MODE_TUNER,
	VIDEO_HDMI_CEC_MODE_RECORDER
} VIDEO_HDMI_CEC_MODE;

typedef enum
{
	VIDEO_CONTROL_BRIGHTNESS = 0,
	VIDEO_CONTROL_CONTRAST,
	VIDEO_CONTROL_SATURATION,
	VIDEO_CONTROL_HUE,
	VIDEO_CONTROL_SHARPNESS,
	VIDEO_CONTROL_MAX = VIDEO_CONTROL_SHARPNESS
} VIDEO_CONTROL;

class cDemux;
class cPlayback;
class VDec;

class cVideo
{
	friend class cPlayback;
	friend class cDemux;
	public:
		/* constructor & destructor */
		cVideo(int mode, void *, void *, unsigned int unit = 0);
		~cVideo(void);

		void * GetTVEnc() { return NULL; };
		void * GetTVEncSD() { return NULL; };

		/* aspect ratio */
		int getAspectRatio(void);
		void getPictureInfo(int &width, int &height, int &rate);
		int setAspectRatio(int aspect, int mode);

		/* cropping mode */
		int setCroppingMode(void);

		/* get play state */
		int getPlayState(void);

		/* blank on freeze */
		int getBlank(void);
		int setBlank(int enable);

		/* change video play state. Parameters are all unused. */
		int Start(void *PcrChannel = NULL, unsigned short PcrPid = 0, unsigned short VideoPid = 0, void *x = NULL);
		int Stop(bool blank = true);
		bool Pause(void);

		/* get video system infos */
		int GetVideoSystem(void);
		/* when system = -1 then use current video system */
		void GetVideoSystemFormatName(cs_vs_format_t* format, int system = -1);

		/* set video_system */
		int SetVideoSystem(int video_system, bool remember = true);
		int SetStreamType(VIDEO_FORMAT type);
		void SetSyncMode(AVSYNC_TYPE mode);
		bool SetCECMode(VIDEO_HDMI_CEC_MODE) { return true; };
		void SetCECAutoView(bool) { return; };
		void SetCECAutoStandby(bool) { return; };
		void ShowPicture(const char * fname);
		void StopPicture();
		void Standby(unsigned int bOn);
		void Pig(int x, int y, int w, int h, int osd_w = 1064, int osd_h = 600);
		void SetControl(int, int) { return; };
		void setContrast(int val);
		void SetVideoMode(analog_mode_t mode);
		void SetDBDR(int) { return; };
		void SetAudioHandle(void *) { return; };
		void SetAutoModes(int [VIDEO_STD_MAX]) { return; };
		int  OpenVBI(int) { return 0; };
		int  CloseVBI(void) { return 0; };
		int  StartVBI(unsigned short) { return 0; };
		int  StopVBI(void) { return 0; };
		void SetDemux(cDemux *dmx);
		bool GetScreenImage(unsigned char * &data, int &xres, int &yres, bool get_video = true, bool get_osd = false, bool scale_to_video = false);
	private:
		VDec *vdec;
		void *pdata;
};
>>>>>>> 6e3b116... libspark: implement cVideo::GetVideoSystemFormatName()

#if STB_HAL_VIDEO_HAS_GETSCREENIMAGE
#define SCREENSHOT 1
#endif
