/*
	Copyright 2010 Carsten Juttner <carjay@gmx.net>
	Copyright 2012,2013 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>

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

#ifndef __glthread__
#define __glthread__
#include <OpenThreads/Thread>
#include <OpenThreads/Mutex>
#include <vector>
#include <map>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/gl.h>
#include <linux/fb.h> /* for screeninfo etc. */
extern "C" {
#include <libavutil/rational.h>
}

class GLFramebuffer : public OpenThreads::Thread
{
public:
	GLFramebuffer(int x, int y);
	~GLFramebuffer();

	void run();
	std::vector<unsigned char> *getOSDBuffer() { return &mOSDBuffer; } /* pointer to OSD bounce buffer */

	int getOSDWidth() { return mState.width; }
	int getOSDHeight() { return mState.height; }
	void blit() { mState.blit = true; }

	void setOutputFormat(AVRational a, int h, int c) { mOA = a; *mY = h; mCrop = c; mReInit = true; }

	void clear();
	fb_var_screeninfo getScreenInfo() { return screeninfo; }

private:
	fb_var_screeninfo screeninfo;
	int *mX;
	int *mY;
	int _mX[2];			/* output window size */
	int _mY[2];			/* [0] = normal, [1] = fullscreen */
	AVRational mOA;			/* output window aspect ratio */
	AVRational mVA;			/* video aspect ratio */
	AVRational _mVA;		/* for detecting changes in mVA */
	bool mVAchanged;
	float zoom;			/* for cropping */
	float xscale;			/* and aspect ratio */
	int mCrop;			/* DISPLAY_AR_MODE */

	bool mFullscreen;		/* fullscreen? */
	bool mReInit;			/* setup things for GL */
	OpenThreads::Mutex mReInitLock;
	bool mShutDown;			/* if set main loop is left */
	bool mInitDone;			/* condition predicate */
	// OpenThreads::Condition mInitCond;	/* condition variable for init */
	// mutable OpenThreads::Mutex mMutex;	/* lock our data */

	std::vector<unsigned char> mOSDBuffer; /* silly bounce buffer */

	std::map<unsigned char, int> mKeyMap;
	std::map<int, int> mSpecialMap;
	int input_fd;
	int64_t last_apts;

	static void rendercb();		/* callback for GLUT */
	void render();			/* actual render function */
	static void keyboardcb(unsigned char key, int x, int y);
	static void specialcb(int key, int x, int y);
	static void resizecb(int w, int h);
	void checkReinit(int w, int h);	/* e.g. in case window was resized */

	void initKeys();		/* setup key bindings for window */
	void setupCtx();		/* create the window and make the context current */
	void setupOSDBuffer();		/* create the OSD buffer */
	void setupGLObjects();		/* PBOs, textures and stuff */
	void releaseGLObjects();
	// void drawCube(float size);	/* cubes are the building blocks of our society */
	void drawSquare(float size, float x_factor = 1);	/* do not be square */

	struct {
		int width;		/* width and height, fixed for a framebuffer instance */
		int height;
		GLuint osdtex;		/* holds the OSD texture */
		GLuint pbo;		/* PBO we use for transfer to texture */
		GLuint displaytex;	/* holds the display texture */
		GLuint displaypbo;
		//int go3d;
		bool blit;
	} mState;

	void bltOSDBuffer();
	void bltDisplayBuffer();
};
#endif
