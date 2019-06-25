/*
	Framebuffer implementation using clutter https://developer.gnome.org/clutter/
	Copyright (C) 2016 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>

	based on the openGL framebuffer implementation
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

	TODO:	AV-Sync code is "experimental" at best
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

#include <clutter/x11/clutter-x11.h>

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
	mVA = mOA;	/* initial aspect ratios are from the FB resolution, those */
	_mVA = mVA;	/* will be updated by the videoDecoder functions anyway */
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
	input_fd = open("/tmp/neutrino.input", O_RDWR|O_CLOEXEC|O_NONBLOCK);
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
	mKeyMap[CLUTTER_KEY_Up]    = KEY_UP;
	mKeyMap[CLUTTER_KEY_Down]  = KEY_DOWN;
	mKeyMap[CLUTTER_KEY_Left]  = KEY_LEFT;
	mKeyMap[CLUTTER_KEY_Right] = KEY_RIGHT;

	mKeyMap[CLUTTER_KEY_F1] = KEY_RED;
	mKeyMap[CLUTTER_KEY_F2] = KEY_GREEN;
	mKeyMap[CLUTTER_KEY_F3] = KEY_YELLOW;
	mKeyMap[CLUTTER_KEY_F4] = KEY_BLUE;

	mKeyMap[CLUTTER_KEY_F5] = KEY_WWW;
	mKeyMap[CLUTTER_KEY_F6] = KEY_SUBTITLE;
	mKeyMap[CLUTTER_KEY_F7] = KEY_MOVE;
	mKeyMap[CLUTTER_KEY_F8] = KEY_SLEEP;

	mKeyMap[CLUTTER_KEY_Page_Up]   = KEY_PAGEUP;
	mKeyMap[CLUTTER_KEY_Page_Down] = KEY_PAGEDOWN;

	mKeyMap[CLUTTER_KEY_Return] = KEY_OK;
	mKeyMap[CLUTTER_KEY_Escape] = KEY_EXIT;
	mKeyMap['e']  = KEY_EPG;
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

static ClutterActor *stage = NULL;
static ClutterActor *fb_actor = NULL;
static ClutterActor *vid_actor = NULL;
static ClutterTimeline *tl = NULL;
void GLFramebuffer::run()
{
	int argc = 1;
	int x = glfb_priv->mState.width;
	int y = glfb_priv->mState.height;
	/* some dummy commandline for GLUT to be happy */
	char *a = (char *)"neutrino";
	char **argv = (char **)malloc(sizeof(char *) * 2);
	argv[0] = a;
	argv[1] = NULL;
	hal_info("GLFB: GL thread starting x %d y %d\n", x, y);
	if (clutter_init(&argc, &argv) != CLUTTER_INIT_SUCCESS) {
		hal_info("GLFB: error initializing clutter\n");
		return;
	}
	hal_info("GLFB: %s:%d\n", __func__, __LINE__);
	ClutterColor stage_color = { 0, 0, 0, 255 };
	stage = clutter_stage_new();
	clutter_actor_set_size(stage, x, y);
	clutter_actor_set_background_color(stage, &stage_color);
	clutter_actor_set_content_gravity(stage, CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT);
	//g_signal_connect(stage, "destroy", G_CALLBACK(clutter_main_quit), NULL);
	g_signal_connect(stage, "key-press-event", G_CALLBACK(GLFbPC::keyboardcb), (void *)1);
	g_signal_connect(stage, "key-release-event", G_CALLBACK(GLFbPC::keyboardcb), NULL);
	clutter_stage_set_user_resizable(CLUTTER_STAGE (stage), TRUE);
	clutter_actor_grab_key_focus(stage);
	clutter_actor_show(stage);

	/* 32bit FB depth, *2 because tuxtxt uses a shadow buffer */
	int fbmem = x * y * 4 * 2;
	osd_buf.resize(fbmem);
	hal_info("GLFB: OSD buffer set to %d bytes at 0x%p\n", fbmem, osd_buf.data());

	/* video plane is below FB plane, so it comes first */
	vid_actor = clutter_actor_new();
	ClutterContent *fb = clutter_image_new();
	/* osd_buf, because it starts up black */
	if (!clutter_image_set_data(CLUTTER_IMAGE(fb), osd_buf.data(), COGL_PIXEL_FORMAT_BGR_888, x, y, x*3, NULL)) {
		hal_info("GLFB::%s clutter_image_set_data failed? (vid)\n", __func__);
		_exit(1); /* life is hard */
	}
	clutter_actor_set_content(vid_actor, fb);
	g_object_unref(fb);
	clutter_actor_set_size(vid_actor, x, y);
	clutter_actor_set_position(vid_actor, 0, 0);
	clutter_actor_add_constraint(vid_actor, clutter_bind_constraint_new(stage, CLUTTER_BIND_WIDTH, 0));
	clutter_actor_add_constraint(vid_actor, clutter_bind_constraint_new(stage, CLUTTER_BIND_HEIGHT, 0));
	clutter_actor_add_constraint(vid_actor, clutter_bind_constraint_new(stage, CLUTTER_BIND_X, 0));
	clutter_actor_add_constraint(vid_actor, clutter_bind_constraint_new(stage, CLUTTER_BIND_Y, 0));
	clutter_actor_set_content_gravity(vid_actor, CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT);
	clutter_actor_set_pivot_point(vid_actor, 0.5, 0.5);
	clutter_actor_add_child(stage, vid_actor);
	clutter_actor_show(vid_actor);

	fb_actor = clutter_actor_new();
	fb = clutter_image_new();
	if (!clutter_image_set_data(CLUTTER_IMAGE(fb), osd_buf.data(), COGL_PIXEL_FORMAT_BGRA_8888, x, y, x*4, NULL)) {
		hal_info("GLFB::%s clutter_image_set_data failed? (osd)\n", __func__);
		_exit(1); /* life is hard */
	}
	clutter_actor_set_content(fb_actor, fb);
	g_object_unref(fb);
	clutter_actor_set_size(fb_actor, x, y);
	clutter_actor_set_position(fb_actor, 0, 0);
	clutter_actor_add_constraint(fb_actor, clutter_bind_constraint_new(stage, CLUTTER_BIND_WIDTH, 0));
	clutter_actor_add_constraint(fb_actor, clutter_bind_constraint_new(stage, CLUTTER_BIND_HEIGHT, 0));
	clutter_actor_add_constraint(fb_actor, clutter_bind_constraint_new(stage, CLUTTER_BIND_X, 0));
	clutter_actor_add_constraint(fb_actor, clutter_bind_constraint_new(stage, CLUTTER_BIND_Y, 0));
	clutter_actor_set_content_gravity(fb_actor, CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT);
	clutter_actor_add_child(stage, fb_actor);
	clutter_actor_show(fb_actor);

	glfb_priv->mInitDone = true; /* signal that setup is finished */
	tl = clutter_timeline_new(100);
	g_signal_connect(tl, "new-frame", G_CALLBACK(GLFbPC::rendercb), NULL);
	clutter_timeline_set_repeat_count(tl, -1);
	clutter_timeline_start(tl);
	clutter_main();
	hal_info("GLFB: GL thread stopping\n");
}

/* static */ void GLFbPC::rendercb()
{
	glfb_priv->render();
}

/* static */ bool GLFbPC::keyboardcb(ClutterActor * /*actor*/, ClutterEvent *event, gpointer user_data)
{
	guint key = clutter_event_get_key_symbol (event);
	int keystate = user_data ? 1 : 0;
	hal_info_c("GLFB::%s: 0x%x, %d\n", __func__, key, keystate);

	struct input_event ev;
	if (key == 'f' && keystate)
	{
		hal_info_c("GLFB::%s: toggle fullscreen %s\n", __func__, glfb_priv->mFullscreen?"off":"on");
		glfb_priv->mFullscreen = !(glfb_priv->mFullscreen);
		glfb_priv->mReInit = true;
		return true;
	}
	std::map<int, int>::const_iterator i = glfb_priv->mKeyMap.find(key);
	if (i == glfb_priv->mKeyMap.end())
		return true;
	ev.code  = i->second;
	ev.value = keystate; /* key own */
	ev.type  = EV_KEY;
	gettimeofday(&ev.time, NULL);
	hal_debug_c("GLFB::%s: pushing 0x%x\n", __func__, ev.code);
	write(glfb_priv->input_fd, &ev, sizeof(ev));
	return true;
}

int sleep_us = 30000;

void GLFbPC::render()
{
	if(mShutDown)
		clutter_main_quit();

	mReInitLock.lock();
	if (mReInit)
	{
		int xoff = 0;
		int yoff = 0;
		mVAchanged = true;
		mReInit = false;
#if 0
		mX = &_mX[mFullscreen];
		mY = &_mY[mFullscreen];
#endif
		*mX = *mY * mOA.num / mOA.den;
		if (mFullscreen) {
			clutter_stage_set_fullscreen(CLUTTER_STAGE(stage), TRUE);
			clutter_actor_show(stage);
			clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));
		} else {
			clutter_stage_set_fullscreen(CLUTTER_STAGE(stage), FALSE);
//			*mX = *mY * mOA.num / mOA.den;
			clutter_actor_set_size(stage, *mX, *mY);
		}
		hal_info("%s: reinit mX:%d mY:%d xoff:%d yoff:%d fs %d\n",
			__func__, *mX, *mY, xoff, yoff, mFullscreen);
	}
	mReInitLock.unlock();

	bltDisplayBuffer(); /* decoded video stream */
	if (mState.blit) {
		/* only blit manually after fb->blit(), this helps to find missed blit() calls */
		mState.blit = false;
		hal_debug("GLFB::%s blit!\n", __func__);
		bltOSDBuffer(); /* OSD */
	}

	if (mVAchanged)
	{
		mVAchanged = false;
		zoom = 1.0;
		float xzoom = 1.0;
		//xscale = 1.0;
		int cmp = av_cmp_q(mVA, mOA);
		const AVRational a149 = { 14, 9 };
		switch (cmp) {
			default:
			case INT_MIN:	/* invalid */
			case 0:		/* identical */
				hal_debug("%s: mVA == mOA (or fullscreen mode :-)\n", __func__);
				break;
			case 1:		/* mVA > mOA -- video is wider than display */
				hal_debug("%s: mVA > mOA\n", __func__);
				switch (mCrop) {
					case DISPLAY_AR_MODE_PANSCAN:
						zoom = av_q2d(mVA) / av_q2d(mOA);
						break;
					case DISPLAY_AR_MODE_LETTERBOX:
						break;
					case DISPLAY_AR_MODE_PANSCAN2:
						zoom = av_q2d(a149) / av_q2d(mOA);
						break;
					case DISPLAY_AR_MODE_NONE:
						xzoom = av_q2d(mOA) / av_q2d(mVA);
						zoom = av_q2d(mVA) / av_q2d(mOA);
						break;
					default:
						break;
				}
				break;
			case -1:	/* mVA < mOA -- video is taller than display */
				hal_debug("%s: mVA < mOA\n", __func__);
				switch (mCrop) {
					case DISPLAY_AR_MODE_LETTERBOX:
						break;
					case DISPLAY_AR_MODE_PANSCAN2:
						if (av_cmp_q(a149, mOA) < 0) {
							zoom = av_q2d(mVA) * av_q2d(a149) / av_q2d(mOA);
							break;
						}
						/* fallthrough for output format 14:9 */
					case DISPLAY_AR_MODE_PANSCAN:
						zoom = av_q2d(mOA) / av_q2d(mVA);
						break;
					case DISPLAY_AR_MODE_NONE:
						xzoom = av_q2d(mOA) / av_q2d(mVA);
						break;
					default:
						break;
				}
				break;
		}
		hal_debug("zoom: %f xscale: %f xzoom: %f\n", zoom, xscale,xzoom);
		clutter_actor_set_scale(vid_actor, xscale*zoom*xzoom, zoom);
	}
	clutter_timeline_stop(tl);
	clutter_timeline_set_delay(tl, sleep_us/1000);
	clutter_timeline_start(tl);
}

void GLFbPC::bltOSDBuffer()
{
	// hal_info("%s\n", __func__);
	int x = glfb_priv->mState.width;
	int y = glfb_priv->mState.height;
	ClutterContent *fb = clutter_image_new();
	if (!clutter_image_set_data(CLUTTER_IMAGE(fb), osd_buf->data(), COGL_PIXEL_FORMAT_BGRA_8888, x, y, x*4, NULL)) {
		hal_info("GLFB::%s clutter_image_set_data failed?\n", __func__);
		_exit(1); /* life is hard */
	}
	clutter_actor_set_content(fb_actor, fb);
	g_object_unref(fb);
	clutter_actor_show(fb_actor);
}

void GLFbPC::bltDisplayBuffer()
{
	// hal_info("GLFB::%s vdec: %p\n", __func__, vdec);
	if (!videoDecoder) /* cannot start yet */
		return;
	static bool warn = true;
	cVideo::SWFramebuffer *buf = videoDecoder->getDecBuf();
	if (!buf) {
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
	if (a.den != 0 && a.num != 0 && av_cmp_q(a, _mVA)) {
		_mVA = a;
		/* _mVA is the raw buffer's aspect, mVA is the real scaled output aspect */
		av_reduce(&mVA.num, &mVA.den, w * a.num, h * a.den, INT_MAX);
		// mVA.num: 16 mVA.den: 9 w: 720 h: 576
		// 16*576/720/9 = 1.42222
		xscale = (double)mVA.num*h/(double)mVA.den/w;
		mVAchanged = true;
	}

	ClutterContent *fb = clutter_image_new();
	if (!clutter_image_set_data(CLUTTER_IMAGE(fb), &(*buf)[0], COGL_PIXEL_FORMAT_BGR_888, w, h, w*3, NULL)) {
		hal_info("GLFB::%s clutter_image_set_data failed?\n", __func__);
		_exit(1); /* life is hard */
	}
	clutter_actor_set_content(vid_actor, fb);
	g_object_unref(fb);
	clutter_actor_show(vid_actor);

	/* "rate control" mechanism starts here...
	 * this implementation is pretty naive and not working too well, but
	 * better this than nothing... :-) */
	int64_t apts = 0;
	int64_t vpts = buf->pts();
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
			buf->pts(), apts, (buf->pts() - apts)/90000.0, sleep_us, videoDecoder->buf_num);
}
