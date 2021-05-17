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

    TODO:   AV-Sync code is "experimental" at best
*/

#include "config.h"
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
#include "glfb_priv.h"
#include "video_lib.h"
#include "audio_lib.h"

#include "hal_debug.h"

#define hal_debug_c(args...) _hal_debug(HAL_DEBUG_INIT, NULL, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_INIT, NULL, args)
#define hal_debug(args...) _hal_debug(HAL_DEBUG_INIT, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_INIT, this, args)


extern cVideo *videoDecoder;
extern cAudio *audioDecoder;

/* the private class that does stuff only needed inside libstb-hal.
 * is used e.g. by cVideo... */
GLFbPC *glfb_priv = NULL;

GLFramebuffer::GLFramebuffer(int x, int y)
{
	Init();
	glfb_priv = new GLFbPC(x, y, osd_buf);
	si = glfb_priv->getScreenInfo();
	start();
	while (!glfb_priv->mInitDone)
		usleep(1);
}

GLFramebuffer::~GLFramebuffer()
{
	glfb_priv->mShutDown = true;
	join();
	delete glfb_priv;
	glfb_priv = NULL;
}

void GLFramebuffer::blit()
{
	glfb_priv->blit();
}

GLFbPC::GLFbPC(int x, int y, std::vector<unsigned char> &buf): mReInit(true), mShutDown(false), mInitDone(false)
{
	osd_buf = &buf;
	mState.width  = x;
	mState.height = y;
	mX = &_mX[0];
	mY = &_mY[0];
	*mX = x;
	*mY = y;
	av_reduce(&mOA.num, &mOA.den, x, y, INT_MAX);
	mVA = mOA;  /* initial aspect ratios are from the FB resolution, those */
	_mVA = mVA; /* will be updated by the videoDecoder functions anyway */
	mVAchanged = true;
	mCrop = DISPLAY_AR_MODE_PANSCAN;
	zoom = 1.0;
	xscale = 1.0;
	const char *tmp = getenv("GLFB_FULLSCREEN");
	mFullscreen = !!(tmp);

	mState.blit = true;
	last_apts = 0;

	/* linux framebuffer compat mode */
	si.bits_per_pixel = 32;
	si.xres = mState.width;
	si.xres_virtual = si.xres;
	si.yres = mState.height;
	si.yres_virtual = si.yres;
	si.blue.length = 8;
	si.blue.offset = 0;
	si.green.length = 8;
	si.green.offset = 8;
	si.red.length = 8;
	si.red.offset = 16;
	si.transp.length = 8;
	si.transp.offset = 24;

	unlink("/tmp/neutrino.input");
	mkfifo("/tmp/neutrino.input", 0600);
	input_fd = open("/tmp/neutrino.input", O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if (input_fd < 0)
		hal_info("%s: could not open /tmp/neutrino.input FIFO: %m\n", __func__);
	initKeys();
}

GLFbPC::~GLFbPC()
{
	mShutDown = true;
	if (input_fd >= 0)
		close(input_fd);
	osd_buf->clear();
}

void GLFbPC::initKeys()
{
	/*
	   Keep in sync with initKeys() in clutterfb.cpp
	*/

	mSpecialMap[GLUT_KEY_UP]    = KEY_UP;
	mSpecialMap[GLUT_KEY_DOWN]  = KEY_DOWN;
	mSpecialMap[GLUT_KEY_LEFT]  = KEY_LEFT;
	mSpecialMap[GLUT_KEY_RIGHT] = KEY_RIGHT;

	mSpecialMap[GLUT_KEY_F1]  = KEY_RED;
	mSpecialMap[GLUT_KEY_F2]  = KEY_GREEN;
	mSpecialMap[GLUT_KEY_F3]  = KEY_YELLOW;
	mSpecialMap[GLUT_KEY_F4]  = KEY_BLUE;

	mSpecialMap[GLUT_KEY_F5]  = KEY_RECORD;
	mSpecialMap[GLUT_KEY_F6]  = KEY_PLAY;
	mSpecialMap[GLUT_KEY_F7]  = KEY_PAUSE;
	mSpecialMap[GLUT_KEY_F8]  = KEY_STOP;

	mSpecialMap[GLUT_KEY_F9]  = KEY_FORWARD;
	mSpecialMap[GLUT_KEY_F10] = KEY_REWIND;
	mSpecialMap[GLUT_KEY_F11] = KEY_NEXT;
	mSpecialMap[GLUT_KEY_F12] = KEY_PREVIOUS;

	mSpecialMap[GLUT_KEY_PAGE_UP]   = KEY_PAGEUP;
	mSpecialMap[GLUT_KEY_PAGE_DOWN] = KEY_PAGEDOWN;

	mKeyMap[0x0d] = KEY_OK;
	mKeyMap[0x1b] = KEY_EXIT;

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

	mKeyMap['+']  = KEY_VOLUMEUP;
	mKeyMap['-']  = KEY_VOLUMEDOWN;
	mKeyMap['.']  = KEY_MUTE;
	mKeyMap['a']  = KEY_AUDIO;
	mKeyMap['e']  = KEY_EPG;
	//     ['f']    is reserved to toggle fullscreen;
	mKeyMap['g']  = KEY_GAMES;
	mKeyMap['h']  = KEY_HELP;
	mKeyMap['i']  = KEY_INFO;
	mKeyMap['m']  = KEY_MENU;
	mKeyMap['p']  = KEY_POWER;
	mKeyMap['r']  = KEY_RADIO;
	mKeyMap['s']  = KEY_SUBTITLE;
	mKeyMap['t']  = KEY_TV;
	mKeyMap['v']  = KEY_VIDEO;
	mKeyMap['z']  = KEY_SLEEP;

	/* shift keys */
	mKeyMap['F']  = KEY_FAVORITES;
	mKeyMap['M']  = KEY_MODE;
	mKeyMap['S']  = KEY_SAT;
	mKeyMap['T']  = KEY_TEXT;
	mKeyMap['W']  = KEY_WWW;
}

void GLFramebuffer::run()
{
	int argc = 1;
	int x = glfb_priv->mState.width;
	int y = glfb_priv->mState.height;
	/* some dummy commandline for GLUT to be happy */
	char const *argv[2] = { "neutrino", 0 };
	hal_info("GLFB: GL thread starting x %d y %d\n", x, y);
	glutInit(&argc, const_cast<char **>(argv));
	glutInitWindowSize(x, y);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutCreateWindow("Neutrino");
	/* 32bit FB depth, *2 because tuxtxt uses a shadow buffer */
	int fbmem = x * y * 4 * 2;
	osd_buf.resize(fbmem);
	hal_info("GLFB: OSD buffer set to %d bytes at 0x%p\n", fbmem, osd_buf.data());
	glfb_priv->mInitDone = true; /* signal that setup is finished */

	/* init the good stuff */
	GLenum err = glewInit();
	if (err == GLEW_OK)
	{
		if ((!GLEW_VERSION_1_5) || (!GLEW_EXT_pixel_buffer_object) || (!GLEW_ARB_texture_non_power_of_two))
		{
			hal_info("GLFB: Sorry, your graphics card is not supported. "
			    "Needs at least OpenGL 1.5, pixel buffer objects and NPOT textures.\n");
			hal_info("incompatible graphics card: %m");
			_exit(1); /* Life is hard */
		}
		else
		{
			glutSetCursor(GLUT_CURSOR_NONE);
			glutDisplayFunc(GLFbPC::rendercb);
			glutKeyboardFunc(GLFbPC::keyboardcb);
			glutSpecialFunc(GLFbPC::specialcb);
			glutReshapeFunc(GLFbPC::resizecb);
			glfb_priv->setupGLObjects(); /* needs GLEW prototypes */
			glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
			glutMainLoop();
			glfb_priv->releaseGLObjects();
		}
	}
	else
		hal_info("GLFB: error initializing glew: %d\n", err);
	hal_info("GLFB: GL thread stopping\n");
}

#if 0
void GLFbPC::setupCtx()
{
	int argc = 1;
	/* some dummy commandline for GLUT to be happy */
	char const *argv[2] = { "neutrino", 0 };
	hal_info("GLFB: GL thread starting x %d y %d\n", mX[0], mY[0]);
	glutInit(&argc, const_cast<char **>(argv));
	glutInitWindowSize(mX[0], mY[0]);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutCreateWindow("Neutrino");
}

void GLFbPC::setupOSDBuffer()
{
	/* the OSD buffer size can be decoupled from the actual
	   window size since the GL can blit-stretch with no
	   trouble at all, ah, the luxury of ignorance... */
	// mMutex.lock();
	if (mState.width && mState.height)
	{
		/* 32bit FB depth, *2 because tuxtxt uses a shadow buffer */
		int fbmem = mState.width * mState.height * 4 * 2;
		osd_buf->resize(fbmem);
		hal_info("GLFB: OSD buffer set to %d bytes at 0x%p\n", fbmem, osd_buf->data());
	}
}
#endif

void GLFbPC::setupGLObjects()
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


void GLFbPC::releaseGLObjects()
{
	glDeleteBuffers(1, &mState.pbo);
	glDeleteBuffers(1, &mState.displaypbo);
	glDeleteTextures(1, &mState.osdtex);
	glDeleteTextures(1, &mState.displaytex);
}


/* static */ void GLFbPC::rendercb()
{
	glfb_priv->render();
}


/* static */ void GLFbPC::keyboardcb(unsigned char key, int /*x*/, int /*y*/)
{
	hal_debug_c("GLFB::%s: 0x%x\n", __func__, key);
	struct input_event ev;
	if (key == 'f')
	{
		hal_info_c("GLFB::%s: toggle fullscreen %s\n", __func__, glfb_priv->mFullscreen ? "off" : "on");
		glfb_priv->mFullscreen = !(glfb_priv->mFullscreen);
		glfb_priv->mReInit = true;
		return;
	}
	std::map<unsigned char, int>::const_iterator i = glfb_priv->mKeyMap.find(key);
	if (i == glfb_priv->mKeyMap.end())
		return;
	ev.code  = i->second;
	ev.value = 1; /* key own */
	ev.type  = EV_KEY;
	gettimeofday(&ev.time, NULL);
	hal_debug_c("GLFB::%s: pushing 0x%x\n", __func__, ev.code);
	write(glfb_priv->input_fd, &ev, sizeof(ev));
	ev.value = 0; /* neutrino is stupid, so push key up directly after key down */
	write(glfb_priv->input_fd, &ev, sizeof(ev));
}

/* static */ void GLFbPC::specialcb(int key, int /*x*/, int /*y*/)
{
	hal_debug_c("GLFB::%s: 0x%x\n", __func__, key);
	struct input_event ev;
	std::map<int, int>::const_iterator i = glfb_priv->mSpecialMap.find(key);
	if (i == glfb_priv->mSpecialMap.end())
		return;
	ev.code  = i->second;
	ev.value = 1;
	ev.type  = EV_KEY;
	gettimeofday(&ev.time, NULL);
	hal_debug_c("GLFB::%s: pushing 0x%x\n", __func__, ev.code);
	write(glfb_priv->input_fd, &ev, sizeof(ev));
	ev.value = 0;
	write(glfb_priv->input_fd, &ev, sizeof(ev));
}

int sleep_us = 30000;

void GLFbPC::render()
{
	if (mShutDown)
		glutLeaveMainLoop();

	mReInitLock.lock();
	if (mReInit)
	{
		int xoff = 0;
		int yoff = 0;
		mVAchanged = true;
		mReInit = false;
		mX = &_mX[mFullscreen];
		mY = &_mY[mFullscreen];
		if (mFullscreen)
		{
			int x = glutGet(GLUT_SCREEN_WIDTH);
			int y = glutGet(GLUT_SCREEN_HEIGHT);
			*mX = x;
			*mY = y;
			AVRational a = { x, y };
			if (av_cmp_q(a, mOA) < 0)
				*mY = x * mOA.den / mOA.num;
			else if (av_cmp_q(a, mOA) > 0)
				*mX = y * mOA.num / mOA.den;
			xoff = (x - *mX) / 2;
			yoff = (y - *mY) / 2;
			glutFullScreen();
		}
		else
			*mX = *mY * mOA.num / mOA.den;
		hal_info("%s: reinit mX:%d mY:%d xoff:%d yoff:%d fs %d\n",
		    __func__, *mX, *mY, xoff, yoff, mFullscreen);
		glViewport(xoff, yoff, *mX, *mY);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		float aspect = static_cast<float>(*mX) / *mY;
		float osdaspect = static_cast<float>(mOA.den) / mOA.num;

		glOrtho(aspect * -osdaspect, aspect * osdaspect, -1.0, 1.0, -1.0, 1.0);
		glClearColor(0.0, 0.0, 0.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	mReInitLock.unlock();
	if (!mFullscreen && (*mX != glutGet(GLUT_WINDOW_WIDTH) || *mY != glutGet(GLUT_WINDOW_HEIGHT)))
		glutReshapeWindow(*mX, *mY);

	bltDisplayBuffer(); /* decoded video stream */
	if (mState.blit)
	{
		/* only blit manually after fb->blit(), this helps to find missed blit() calls */
		mState.blit = false;
		hal_debug("GLFB::%s blit!\n", __func__);
		bltOSDBuffer(); /* OSD */
	}

	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mVAchanged)
	{
		mVAchanged = false;
		zoom = 1.0;
		xscale = 1.0;
		int cmp = (mCrop == DISPLAY_AR_MODE_NONE) ? 0 : av_cmp_q(mVA, mOA);
		const AVRational a149 = { 14, 9 };
		switch (cmp)
		{
			default:
			case INT_MIN:   /* invalid */
			case 0:     /* identical */
				hal_debug("%s: mVA == mOA (or fullscreen mode :-)\n", __func__);
				break;
			case 1:     /* mVA > mOA -- video is wider than display */
				hal_debug("%s: mVA > mOA\n", __func__);
				xscale = av_q2d(mVA) / av_q2d(mOA);
				switch (mCrop)
				{
					case DISPLAY_AR_MODE_PANSCAN:
						break;
					case DISPLAY_AR_MODE_LETTERBOX:
						zoom = av_q2d(mOA) / av_q2d(mVA);
						break;
					case DISPLAY_AR_MODE_PANSCAN2:
						zoom = av_q2d(mOA) / av_q2d(a149);
						break;
					default:
						break;
				}
				break;
			case -1:    /* mVA < mOA -- video is taller than display */
				hal_debug("%s: mVA < mOA\n", __func__);
				xscale = av_q2d(mVA) / av_q2d(mOA);
				switch (mCrop)
				{
					case DISPLAY_AR_MODE_LETTERBOX:
						break;
					case DISPLAY_AR_MODE_PANSCAN2:
						if (av_cmp_q(a149, mOA) < 0)
						{
							zoom = av_q2d(mVA) * av_q2d(a149) / av_q2d(mOA);
							break;
						}
					/* fallthrough for output format 14:9 */
					case DISPLAY_AR_MODE_PANSCAN:
						zoom = av_q2d(mOA) / av_q2d(mVA);
						break;
					default:
						break;
				}
				break;
		}
	}
	glBindTexture(GL_TEXTURE_2D, mState.displaytex);
	drawSquare(zoom, xscale);
	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	drawSquare(1.0, -100);

	glFlush();
	glutSwapBuffers();

	GLuint err = glGetError();
	if (err != 0)
		hal_info("GLFB::%s: GLError:%d 0x%04x\n", __func__, err, err);
	if (sleep_us > 0)
		usleep(sleep_us);
	glutPostRedisplay();
}

/* static */ void GLFbPC::resizecb(int w, int h)
{
	glfb_priv->checkReinit(w, h);
}

void GLFbPC::checkReinit(int x, int y)
{
	static int last_x = 0, last_y = 0;

	mReInitLock.lock();
	if (!mFullscreen && !mReInit && (x != *mX || y != *mY))
	{
		if (x != *mX && abs(x - last_x) > 2)
		{
			*mX = x;
			*mY = *mX * mOA.den / mOA.num;
		}
		else if (y != *mY && abs(y - last_y) > 2)
		{
			*mY = y;
			*mX = *mY * mOA.num / mOA.den;
		}
		mReInit = true;
	}
	mReInitLock.unlock();
	last_x = x;
	last_y = y;
}

void GLFbPC::drawSquare(float size, float x_factor)
{
	GLfloat vertices[] =
	{
		1.0f,  1.0f,
		-1.0f,  1.0f,
		-1.0f, -1.0f,
		1.0f, -1.0f,
	};

	GLubyte indices[] = { 0, 1, 2, 3 };

	GLfloat texcoords[] =
	{
		1.0, 0.0,
		0.0, 0.0,
		0.0, 1.0,
		1.0, 1.0,
	};
	if (x_factor > -99.0)   /* x_factor == -100 => OSD */
	{
		if (videoDecoder &&
		    videoDecoder->pig_x > 0 && videoDecoder->pig_y > 0 &&
		    videoDecoder->pig_w > 0 && videoDecoder->pig_h > 0)
		{
			/* these calculations even consider cropping and panscan mode
			 * maybe this could be done with some clever opengl tricks? */
			double w2 = (double)mState.width * 0.5l;
			double h2 = (double)mState.height * 0.5l;
			double x = (double)(videoDecoder->pig_x - w2) / w2 / x_factor / size;
			double y = (double)(h2 - videoDecoder->pig_y) / h2 / size;
			double w = (double)videoDecoder->pig_w / w2;
			double h = (double)videoDecoder->pig_h / h2;
			x += ((1.0l - x_factor * size) / 2.0l) * w / x_factor / size;
			y += ((size - 1.0l) / 2.0l) * h / size;
			vertices[0] = x + w;        /* top right x */
			vertices[1] = y;        /* top right y */
			vertices[2] = x;        /* top left x */
			vertices[3] = y;        /* top left y */
			vertices[4] = x;        /* bottom left x */
			vertices[5] = y - h;        /* bottom left y */
			vertices[6] = vertices[0];  /* bottom right x */
			vertices[7] = vertices[5];  /* bottom right y */
		}
	}
	else
		x_factor = 1.0; /* OSD */

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


void GLFbPC::bltOSDBuffer()
{
	/* FIXME: copy each time  */
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mState.pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, osd_buf->size(), osd_buf->data(), GL_STREAM_DRAW_ARB);

	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mState.width, mState.height, GL_BGRA, GL_UNSIGNED_BYTE, 0);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GLFbPC::bltDisplayBuffer()
{
	if (!videoDecoder) /* cannot start yet */
		return;
	static bool warn = true;
	cVideo::SWFramebuffer *buf = videoDecoder->getDecBuf();
	if (!buf)
	{
		if (warn)
			hal_info("GLFB::%s did not get a buffer...\n", __func__);
		warn = false;
		return;
	}
	warn = true;
	int w = buf->width(), h = buf->height();
	if (w == 0 || h == 0)
		return;

	AVRational a = buf->AR();
	if (a.den != 0 && a.num != 0 && av_cmp_q(a, _mVA))
	{
		_mVA = a;
		/* _mVA is the raw buffer's aspect, mVA is the real scaled output aspect */
		av_reduce(&mVA.num, &mVA.den, w * a.num, h * a.den, INT_MAX);
		mVAchanged = true;
	}

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
	if (apts != last_apts)
	{
		int rate, dummy1, dummy2;
		if (apts < vpts)
			sleep_us = (sleep_us * 2 + (vpts - apts) * 10 / 9) / 3;
		else if (sleep_us > 1000)
			sleep_us -= 1000;
		last_apts = apts;
		videoDecoder->getPictureInfo(dummy1, dummy2, rate);
		if (rate > 0)
			rate = 2000000 / rate; /* limit to half the frame rate */
		else
			rate = 50000; /* minimum 20 fps */
		if (sleep_us > rate)
			sleep_us = rate;
		else if (sleep_us < 1)
			sleep_us = 1;
	}
	hal_debug("vpts: 0x%" PRIx64 " apts: 0x%" PRIx64 " diff: %6.3f sleep_us %d buf %d\n",
	    buf->pts(), apts, (buf->pts() - apts) / 90000.0, sleep_us, videoDecoder->buf_num);
}
