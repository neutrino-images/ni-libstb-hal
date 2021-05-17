/*
    Copyright 2010 Carsten Juttner <carjay@gmx.net>
    Copyright 2012,2013,2016 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>

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

    ********************************************************************
    private stuff of the GLFB thread that is only used inside libstb-hal
    and not exposed to the application.
*/

#ifndef __glfb_priv__
#define __glfb_priv__
#include <OpenThreads/Mutex>
#include <vector>
#include <map>
#if USE_OPENGL
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/gl.h>
#include <linux/fb.h> /* for screeninfo etc. */
#endif
#if USE_CLUTTER
#include <clutter/clutter.h>
#endif
#include "glfb.h"
extern "C" {
#include <libavutil/rational.h>
}

class GLFbPC
{
	public:
		GLFbPC(int x, int y, std::vector<unsigned char> &buf);
		~GLFbPC();
		std::vector<unsigned char> *getOSDBuffer()
		{
			return osd_buf;    /* pointer to OSD bounce buffer */
		}
		int getOSDWidth()
		{
			return mState.width;
		}
		int getOSDHeight()
		{
			return mState.height;
		}
		void blit()
		{
			mState.blit = true;
		};
		fb_var_screeninfo getScreenInfo()
		{
			return si;
		}
		void setOutputFormat(AVRational a, int h, int c)
		{
			mOA = a;
			*mY = h;
			mCrop = c;
			mReInit = true;
		}
		/* just make everything public for simplicity - this is only used inside libstb-hal anyway
		private:
		*/
		fb_var_screeninfo si;
		int *mX;
		int *mY;
		int _mX[2];         /* output window size */
		int _mY[2];         /* [0] = normal, [1] = fullscreen */
		AVRational mOA;         /* output window aspect ratio */
		AVRational mVA;         /* video aspect ratio */
		AVRational _mVA;        /* for detecting changes in mVA */
		bool mVAchanged;
		float zoom;         /* for cropping */
		float xscale;           /* and aspect ratio */
		int mCrop;          /* DISPLAY_AR_MODE */

		bool mFullscreen;       /* fullscreen? */
		bool mReInit;           /* setup things for GL */
		OpenThreads::Mutex mReInitLock;
		bool mShutDown;         /* if set main loop is left */
		bool mInitDone;         /* condition predicate */
		// OpenThreads::Condition mInitCond;    /* condition variable for init */
		// mutable OpenThreads::Mutex mMutex;   /* lock our data */

		std::vector<unsigned char> *osd_buf; /* silly bounce buffer */

#if USE_OPENGL
		std::map<unsigned char, int> mKeyMap;
		std::map<int, int> mSpecialMap;
#endif
#if USE_CLUTTER
		std::map<int, int> mKeyMap;
#endif
		int input_fd;
		int64_t last_apts;
		void run();

		static void rendercb();     /* callback for GLUT */
		void render();          /* actual render function */
#if USE_OPENGL
		static void keyboardcb(unsigned char key, int x, int y);
		static void specialcb(int key, int x, int y);
		static void resizecb(int w, int h);
		void checkReinit(int w, int h); /* e.g. in case window was resized */
		void setupGLObjects();      /* PBOs, textures and stuff */
		void releaseGLObjects();
		void drawSquare(float size, float x_factor = 1);    /* do not be square */
#endif
#if USE_CLUTTER
		static bool keyboardcb(ClutterActor *actor, ClutterEvent *event, gpointer user_data);
#endif

		void initKeys();        /* setup key bindings for window */
#if 0
		void setupCtx();        /* create the window and make the context current */
		void setupOSDBuffer();      /* create the OSD buffer */
#endif

		struct
		{
			int width;      /* width and height, fixed for a framebuffer instance */
			int height;
			bool blit;
#if USE_OPENGL
			GLuint osdtex;      /* holds the OSD texture */
			GLuint pbo;     /* PBO we use for transfer to texture */
			GLuint displaytex;  /* holds the display texture */
			GLuint displaypbo;
#endif
		} mState;

		void bltOSDBuffer();
		void bltDisplayBuffer();
};
#endif
