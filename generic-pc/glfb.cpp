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

	openGL based framebuffer implementation
	based on Carjay's neutrino-hd-dvbapi work, see
		http://gitorious.org/neutrino-hd/neutrino-hd-dvbapi

	TODO:	AV-Sync code is not existent yet
		cleanup carjay's crazy 3D stuff :-)
		video mode setting (4:3, 16:9, panscan...)
*/

#include <vector>

#include <sys/types.h>
#include <signal.h>

#include <cstdio>
#include <cstring>
#include <errno.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include "glfb.h"
#include "video_lib.h"
#include "audio_lib.h"

#include "lt_debug.h"

#define lt_debug_c(args...) _lt_debug(HAL_DEBUG_INIT, NULL, args)
#define lt_info_c(args...) _lt_info(HAL_DEBUG_INIT, NULL, args)
#define lt_debug(args...) _lt_debug(HAL_DEBUG_INIT, this, args)
#define lt_info(args...) _lt_info(HAL_DEBUG_INIT, this, args)


extern cVideo *videoDecoder;
extern cAudio *audioDecoder;

static GLFramebuffer *gThiz = 0; /* GLUT does not allow for an arbitrary argument to the render func */

GLFramebuffer::GLFramebuffer(int x, int y): mReInit(true), mShutDown(false), mInitDone(false)
{
	mState.width  = x;
	mState.height = y;
	mX = y * 16 / 9; /* hard coded 16:9 initial aspect ratio for now */
	mY = y;
	mVA = 1.0;

	mState.blit = true;
	last_apts = 0;

	/* linux framebuffer compat mode */
	screeninfo.bits_per_pixel = 32;
	screeninfo.xres = mState.width;
	screeninfo.xres_virtual = screeninfo.xres;
	screeninfo.yres = mState.height;
	screeninfo.yres_virtual = screeninfo.yres;
	screeninfo.blue.length = 8;
	screeninfo.blue.offset = 0;
	screeninfo.green.length = 8;
	screeninfo.green.offset = 8;
	screeninfo.red.length = 8;
	screeninfo.red.offset = 16;
	screeninfo.transp.length = 8;
	screeninfo.transp.offset = 24;

	unlink("/tmp/neutrino.input");
	mkfifo("/tmp/neutrino.input", 0600);
	input_fd = open("/tmp/neutrino.input", O_RDWR|O_CLOEXEC|O_NONBLOCK);
	if (input_fd < 0)
		lt_info("%s: could not open /tmp/neutrino.input FIFO: %m\n", __func__);
	initKeys();
	OpenThreads::Thread::start();
	while (!mInitDone)
		usleep(1);
}

GLFramebuffer::~GLFramebuffer()
{
	mShutDown = true;
	OpenThreads::Thread::join();
	if (input_fd >= 0)
		close(input_fd);
}

void GLFramebuffer::initKeys()
{
	mSpecialMap[GLUT_KEY_UP]    = KEY_UP;
	mSpecialMap[GLUT_KEY_DOWN]  = KEY_DOWN;
	mSpecialMap[GLUT_KEY_LEFT]  = KEY_LEFT;
	mSpecialMap[GLUT_KEY_RIGHT] = KEY_RIGHT;

	mSpecialMap[GLUT_KEY_F1] = KEY_RED;
	mSpecialMap[GLUT_KEY_F2] = KEY_GREEN;
	mSpecialMap[GLUT_KEY_F3] = KEY_YELLOW;
	mSpecialMap[GLUT_KEY_F4] = KEY_BLUE;

	mSpecialMap[GLUT_KEY_PAGE_UP]   = KEY_PAGEUP;
	mSpecialMap[GLUT_KEY_PAGE_DOWN] = KEY_PAGEDOWN;

	mKeyMap[0x0d] = KEY_OK;
	mKeyMap[0x1b] = KEY_EXIT;
	mKeyMap['i']  = KEY_INFO;
	mKeyMap['m']  = KEY_MENU;

	mKeyMap['+']  = KEY_VOLUMEUP;
	mKeyMap['-']  = KEY_VOLUMEDOWN;
	mKeyMap['.']  = KEY_MUTE;
	mKeyMap['h']  = KEY_HELP;
	mKeyMap['p']  = KEY_POWER;

	mKeyMap['0']  = KEY_0;
	mKeyMap['1']  = KEY_1;
	mKeyMap['2']  = KEY_2;
	mKeyMap['3']  = KEY_3;
	mKeyMap['4']  = KEY_4;
	mKeyMap['5']  = KEY_5;
	mKeyMap['6']  = KEY_6;
	mKeyMap['7']  = KEY_7;
	mKeyMap['8']  = KEY_8;
	mKeyMap['9']  = KEY_9;
}

void GLFramebuffer::run()
{
	setupCtx();
	setupOSDBuffer();
	mInitDone = true; /* signal that setup is finished */

	/* init the good stuff */
	GLenum err = glewInit();
	if(err == GLEW_OK)
	{
		if((!GLEW_VERSION_1_5)||(!GLEW_EXT_pixel_buffer_object)||(!GLEW_ARB_texture_non_power_of_two))
		{
			lt_info("GLFB: Sorry, your graphics card is not supported. "
				"Needs at least OpenGL 1.5, pixel buffer objects and NPOT textures.\n");
			lt_info("incompatible graphics card: %m");
			_exit(1); /* Life is hard */
		}
		else
		{
			gThiz = this;
			glutDisplayFunc(GLFramebuffer::rendercb);
			glutKeyboardFunc(GLFramebuffer::keyboardcb);
			glutSpecialFunc(GLFramebuffer::specialcb);
			setupGLObjects(); /* needs GLEW prototypes */
			glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
			glutMainLoop();
			releaseGLObjects();
		}
	}
	else
		lt_info("GLFB: error initializing glew: %d\n", err);
	lt_info("GLFB: GL thread stopping\n");
}


void GLFramebuffer::setupCtx()
{
	int argc = 1;
	/* some dummy commandline for GLUT to be happy */
	char const *argv[2] = { "neutrino", 0 };
	lt_info("GLFB: GL thread starting\n");
	glutInit(&argc, const_cast<char **>(argv));
	glutInitWindowSize(mX, mY);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutCreateWindow("Neutrino");
}

void GLFramebuffer::setupOSDBuffer()
{	/* the OSD buffer size can be decoupled from the actual
	   window size since the GL can blit-stretch with no
	   trouble at all, ah, the luxury of ignorance... */
	// mMutex.lock();
	if (mState.width && mState.height)
	{
		/* 32bit FB depth, *2 because tuxtxt uses a shadow buffer */
		int fbmem = mState.width * mState.height * 4 * 2;
		mOSDBuffer.resize(fbmem);
		lt_info("GLFB: OSD buffer set to %d bytes\n", fbmem);
	}
}

void GLFramebuffer::setupGLObjects()
{
	unsigned char buf[4] = { 0, 0, 0, 0 }; /* 1 black pixel */
	glGenTextures(1, &mState.osdtex);
	glGenTextures(1, &mState.displaytex);
	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mState.width, mState.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glBindTexture(GL_TEXTURE_2D, mState.displaytex); /* we do not yet know the size so will set that inline */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glGenBuffers(1, &mState.pbo);
	glGenBuffers(1, &mState.displaypbo);

	/* hack to start with black video buffer instead of white */
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mState.displaypbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(buf), buf, GL_STREAM_DRAW_ARB);
	glBindTexture(GL_TEXTURE_2D, mState.displaytex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}


void GLFramebuffer::releaseGLObjects()
{
	glDeleteBuffers(1, &mState.pbo);
	glDeleteBuffers(1, &mState.displaypbo);
	glDeleteTextures(1, &mState.osdtex);
	glDeleteTextures(1, &mState.displaytex);
}


/* static */ void GLFramebuffer::rendercb()
{
	gThiz->render();
}


/* static */ void GLFramebuffer::keyboardcb(unsigned char key, int /*x*/, int /*y*/)
{
	lt_debug_c("GLFB::%s: 0x%x\n", __func__, key);
	struct input_event ev;
	std::map<unsigned char, int>::const_iterator i = gThiz->mKeyMap.find(key);
	if (i == gThiz->mKeyMap.end())
		return;
	ev.code  = i->second;
	ev.value = 1; /* key own */
	ev.type  = EV_KEY;
	gettimeofday(&ev.time, NULL);
	lt_debug_c("GLFB::%s: pushing 0x%x\n", __func__, ev.code);
	write(gThiz->input_fd, &ev, sizeof(ev));
	ev.value = 0; /* neutrino is stupid, so push key up directly after key down */
	write(gThiz->input_fd, &ev, sizeof(ev));
}

/* static */ void GLFramebuffer::specialcb(int key, int /*x*/, int /*y*/)
{
	lt_debug_c("GLFB::%s: 0x%x\n", __func__, key);
	struct input_event ev;
	std::map<int, int>::const_iterator i = gThiz->mSpecialMap.find(key);
	if (i == gThiz->mSpecialMap.end())
		return;
	ev.code  = i->second;
	ev.value = 1;
	ev.type  = EV_KEY;
	gettimeofday(&ev.time, NULL);
	lt_debug_c("GLFB::%s: pushing 0x%x\n", __func__, ev.code);
	write(gThiz->input_fd, &ev, sizeof(ev));
	ev.value = 0;
	write(gThiz->input_fd, &ev, sizeof(ev));
}

int sleep_us = 30000;

void GLFramebuffer::render()
{
	if (!mReInit) /* for example if window is resized */
		checkReinit();

	if(mShutDown)
		glutLeaveMainLoop();

	if (mReInit)
	{
		mReInit = false;
		glViewport(0, 0, mX, mY);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		float aspect = static_cast<float>(mX)/mY;
		float osdaspect = 1.0/aspect; //(static_cast<float>(mState.width)/mState.height);
//		if(!mState.go3d)
		{
			glOrtho(aspect*-osdaspect, aspect*osdaspect, -1.0, 1.0, -1.0, 1.0 );
			glClearColor(0.0, 0.0, 0.0, 1.0);
		}
#if 0
		else
		{	/* carjay is crazy... :-) */
			gluPerspective(45.0, static_cast<float>(mX)/mY, 0.05, 1000.0);
			glTranslatef(0.0, 0.0, -2.0);
			glClearColor(0.25, 0.25, 0.25, 1.0);
		}
#endif
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	bltDisplayBuffer(); /* decoded video stream */
	if (mState.blit) {
		/* only blit manually after fb->blit(), this helps to find missed blit() calls */
		mState.blit = false;
		lt_debug("GLFB::%s blit!\n", __func__);
		bltOSDBuffer(); /* OSD */
	}

	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#if 0
	// cube test
	if(mState.go3d)
	{
		glEnable(GL_DEPTH_TEST);
		static float ydeg = 0.0;
		glPushMatrix();
		glRotatef(ydeg, 0.0, 1.0, 0.0);
		glBindTexture(GL_TEXTURE_2D, mState.displaytex);
		drawCube(0.5);
		glScalef(1.01, 1.01, 1.01);
		glBindTexture(GL_TEXTURE_2D, mState.osdtex);
		drawCube(0.5);
		glPopMatrix();
		ydeg += 0.75f;
	}
	else
#endif
	{
		glBindTexture(GL_TEXTURE_2D, mState.displaytex);
		drawSquare(1.0, mVA / (16.0/9));
		glBindTexture(GL_TEXTURE_2D, mState.osdtex);
		drawSquare(1.0);
	}

	glFlush();
	glutSwapBuffers();

	GLuint err = glGetError();
	if (err != 0)
		lt_info("GLFB::%s: GLError:%d 0x%04x\n", __func__, err, err);
	usleep(sleep_us);
	glutPostRedisplay();
}


void GLFramebuffer::checkReinit()
{
	int x = glutGet(GLUT_WINDOW_WIDTH);
	int y = glutGet(GLUT_WINDOW_HEIGHT);
	if ( x != mX || y != mY )
	{
		mX = x;
		mY = y;
		mReInit = true;
	}
}

#if 0
void GLFramebuffer::drawCube(float size)
{
	GLfloat vertices[] = {
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f
	};

	GLubyte indices[] = {
		0, 1, 2, 3, /* front  */
		0, 3, 4, 5, /* right  */
		0, 5, 6, 1, /* top    */
		1, 6, 7, 2, /* left   */
		7, 4, 3, 2, /* bottom */
		4, 7, 6, 5  /* back   */
	};

	GLfloat texcoords[] = {
		1.0, 0.0, // v0
		0.0, 0.0, // v1
		0.0, 1.0, // v2
		1.0, 1.0, // v3
		0.0, 1.0, // v4
		0.0, 0.0, // v5
		1.0, 0.0, // v6
		1.0, 1.0  // v7
	};

	glPushMatrix();
	glScalef(size, size, size);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, indices);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glPopMatrix();
}
#endif

void GLFramebuffer::drawSquare(float size, float x_factor)
{
	GLfloat vertices[] = {
		 1.0f,  1.0f,
		-1.0f,  1.0f,
		-1.0f, -1.0f,
		 1.0f, -1.0f,
	};

	GLubyte indices[] = { 0, 1, 2, 3 };

	GLfloat texcoords[] = {
		 1.0, 0.0,
		 0.0, 0.0,
		 0.0, 1.0,
		 1.0, 1.0,
	};

	glPushMatrix();
	glScalef(size * x_factor, size, size);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glDrawElements(GL_QUADS, 4, GL_UNSIGNED_BYTE, indices);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glPopMatrix();
}


void GLFramebuffer::bltOSDBuffer()
{
	/* FIXME: copy each time  */
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mState.pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, mOSDBuffer.size(), &mOSDBuffer[0], GL_STREAM_DRAW_ARB);

	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mState.width, mState.height, GL_BGRA, GL_UNSIGNED_BYTE, 0);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GLFramebuffer::bltDisplayBuffer()
{
	if (!videoDecoder) /* cannot start yet */
		return;
	static bool warn = true;
	cVideo::SWFramebuffer *buf = videoDecoder->getDecBuf();
	if (!buf) {
		if (warn)
			lt_info("GLFB::%s did not get a buffer...\n", __func__);
		warn = false;
		return;
	}
	warn = true;
	int w = buf->width(), h = buf->height();
	if (w == 0 || h == 0)
		return;

	AVRational a = buf->AR();
	if (a.den != 0)
		mVA = static_cast<float>(w * a.num) / h / a.den;

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mState.displaypbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, buf->size(), &(*buf)[0], GL_STREAM_DRAW_ARB);

	glBindTexture(GL_TEXTURE_2D, mState.displaytex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	/* "rate control" mechanism starts here...
	 * this implementation is pretty naive and not working too well, but
	 * better this than nothing... :-) */
	int64_t apts = 0;
	/* 18000 is the magic value for A/V sync in my libao->pulseaudio->intel_hda setup */
	int64_t vpts = buf->pts() + 18000;
	if (audioDecoder)
		apts = audioDecoder->getPts();
	if (apts != last_apts) {
		int rate, dummy1, dummy2;
		if (apts < vpts)
			sleep_us = (sleep_us * 2 + (vpts - apts)*10/9) / 3;
		else if (sleep_us > 1000)
			sleep_us -= 1000;
		last_apts = apts;
		videoDecoder->getPictureInfo(dummy1, dummy2, rate);
		if (rate)
			rate = 2000000 / rate; /* limit to half the frame rate */
		else
			rate = 50000; /* minimum 20 fps */
		if (sleep_us > rate)
			sleep_us = rate;
	}
	lt_debug("vpts: 0x%" PRIx64 " apts: 0x%" PRIx64 " diff: %6.3f sleep_us %d buf %d\n",
			buf->pts(), apts, (buf->pts() - apts)/90000.0, sleep_us, videoDecoder->buf_num);
}

void GLFramebuffer::clear()
{
	/* clears front and back buffer */
	memset(&mOSDBuffer[0], 0, mOSDBuffer.size());
}
