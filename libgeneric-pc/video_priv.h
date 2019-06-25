/*
	Copyright 2013 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>

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

#ifndef __vdec__

#include <OpenThreads/Thread>
#include <OpenThreads/Mutex>

#include "video_hal.h"
extern "C" {
#include <libavutil/rational.h>
}

#define VDEC_MAXBUFS 0x40
class VDec : public OpenThreads::Thread
{
	friend class GLFbPC;
	friend class cDemux;
	friend class cVideo;
	private:
		/* called from GL thread */
		class SWFramebuffer : public std::vector<unsigned char>
		{
		public:
			SWFramebuffer() : mWidth(0), mHeight(0) {}
			void width(int w) { mWidth = w; }
			void height(int h) { mHeight = h; }
			void pts(uint64_t p) { mPts = p; }
			void AR(AVRational a) { mAR = a; }
			int width() const { return mWidth; }
			int height() const { return mHeight; }
			int64_t pts() const { return mPts; }
			AVRational AR() const { return mAR; }
		private:
			int mWidth;
			int mHeight;
			int64_t mPts;
			AVRational mAR;
		};
		int buf_in, buf_out, buf_num;
	public:
		/* constructor & destructor */
		VDec(void);
		~VDec(void);
		/* aspect ratio */
		int getAspectRatio(void);
		int setAspectRatio(int aspect, int mode);
		void getPictureInfo(int &width, int &height, int &rate);

#if 0
		/* cropping mode */
		int setCroppingMode(int x = 0 /*vidDispMode_t x = VID_DISPMODE_NORM*/);

		/* get play state */
		int getPlayState(void);

		/* blank on freeze */
		int getBlank(void);
		int setBlank(int enable);
#endif
		int GetVideoSystem();
		int SetVideoSystem(int system);

		/* change video play state. Parameters are all unused. */
		int Start();
		int Stop(bool blank = true);

		int SetStreamType(VIDEO_FORMAT type);
		void ShowPicture(const char * fname);
		void Pig(int x, int y, int w, int h);
		bool GetScreenImage(unsigned char * &data, int &xres, int &yres, bool get_video = true, bool get_osd = false, bool scale_to_video = false);
		SWFramebuffer *getDecBuf(void);
		int64_t GetPTS(void);
	private:
		void run();
		SWFramebuffer buffers[VDEC_MAXBUFS];
		int dec_w, dec_h;
		int dec_r;
		bool w_h_changed;
		bool thread_running;
		VIDEO_FORMAT v_format;
		OpenThreads::Mutex buf_m;
		DISPLAY_AR display_aspect;
		DISPLAY_AR_MODE display_crop;
		int output_h;
		VIDEO_STD v_std;
		int pig_x;
		int pig_y;
		int pig_w;
		int pig_h;
		bool pig_changed;
		OpenThreads::Mutex still_m;
		bool stillpicture;
};
#endif
