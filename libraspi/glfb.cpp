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

	The GLFB namespace is just because it's already established by the
	generic-pc implementation.
*/

#include <vector>
#include <OpenThreads/Condition>

#include "glfb.h"
#include "bcm_host.h"

#include "hal_debug.h"

#define hal_debug_c(args...) _hal_debug(HAL_DEBUG_INIT, NULL, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_INIT, NULL, args)
#define hal_debug(args...) _hal_debug(HAL_DEBUG_INIT, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_INIT, this, args)

/* I don't want to assert right now */
#define CHECK(x) if (!(x)) { hal_info("GLFB: %s:%d warning: %s\n", __func__, __LINE__, #x); }

/* TODO: encapsulate this into pdata? */
static DISPMANX_RESOURCE_HANDLE_T res[2];
static uint32_t                   vc_img_ptr[2];
static DISPMANX_UPDATE_HANDLE_T   update;
static DISPMANX_ELEMENT_HANDLE_T  element;
static DISPMANX_DISPLAY_HANDLE_T  display;
static DISPMANX_MODEINFO_T        info;
static VC_RECT_T                  dst_rect;
static void *image;
static int curr_res;
static int pitch;
static VC_IMAGE_TYPE_T type = VC_IMAGE_ARGB8888;

static OpenThreads::Mutex blit_mutex;
static OpenThreads::Condition blit_cond;

static bool goodbye = false;	/* if set main loop is left */
static bool ready = false;	/* condition predicate */

static int width;		/* width and height, fixed for a framebuffer instance */
static int height;

GLFramebuffer::GLFramebuffer(int x, int y)
{
	width = x;
	height = y;

	/* linux framebuffer compat mode */
	si.bits_per_pixel = 32;
	si.xres = si.xres_virtual = width;
	si.yres = si.yres_virtual = height;
	si.blue.length = si.green.length = si.red.length = si.transp.length = 8;
	si.blue.offset = 0;
	si.green.offset = 8;
	si.red.offset = 16;
	si.transp.offset = 24;

	OpenThreads::Thread::start();
	while (!ready)
		usleep(1);
}

GLFramebuffer::~GLFramebuffer()
{
	goodbye = true;
	blit(); /* wake up thread */
	OpenThreads::Thread::join();
}

void GLFramebuffer::run()
{
	hal_set_threadname("hal:fbuff");
	setup();
	ready = true; /* signal that setup is finished */
	blit_mutex.lock();
	while (!goodbye)
	{
		blit_cond.wait(&blit_mutex);
		blit_osd();
	}
	blit_mutex.unlock();
	hal_info("GLFB: GL thread stopping\n");
}

void GLFramebuffer::blit()
{
	blit_mutex.lock();
	blit_cond.signal();
	blit_mutex.unlock();
}

void GLFramebuffer::setup()
{
	hal_info("GLFB: raspi OMX fb setup\n");
	int ret;
	VC_RECT_T src_rect, dsp_rect; /* source and display size will not change. period. */
	pitch = ALIGN_UP(width * 4, 32);
	/* broadcom example code has this ALIGN_UP in there for a reasin, I suppose */
	if (pitch != width * 4)
		hal_info("GLFB: WARNING: width not a multiple of 8? I doubt this will work...\n");

	/* global alpha nontransparent (255) */
	VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FROM_SOURCE, 255, 0 };

	bcm_host_init();

	display = vc_dispmanx_display_open(0);
	ret = vc_dispmanx_display_get_info(display, &info);
	CHECK(ret == 0);
	/* 32bit FB depth, *2 because tuxtxt uses a shadow buffer */
	osd_buf.resize(pitch * height * 2);
	hal_info("GLFB: Display is %d x %d, FB is %d x %d, memory size %d\n",
		info.width, info.height, width, height, osd_buf.size());
	image = &osd_buf[0];
	/* initialize to half-transparent grey */
	memset(image, 0x7f, osd_buf.size());
	for (int i = 0; i < 2; i++)
	{
		res[i] = vc_dispmanx_resource_create(type, width, height, &vc_img_ptr[i]);
		CHECK(res[i]);
	}
	vc_dispmanx_rect_set(&dst_rect, 0, 0, width, height);
	ret = vc_dispmanx_resource_write_data(res[curr_res], type, pitch, image, &dst_rect);
	CHECK(ret == 0);
	update = vc_dispmanx_update_start(10);
	CHECK(update);
	vc_dispmanx_rect_set(&src_rect, 0, 0, width << 16, height << 16);
	vc_dispmanx_rect_set(&dsp_rect, 0, 0, info.width, info.height);
	element = vc_dispmanx_element_add(update,
			display,
			2000 /*layer*/,
			&dsp_rect,
			res[curr_res],
			&src_rect,
			DISPMANX_PROTECTION_NONE,
			&alpha,
			NULL,
			DISPMANX_NO_ROTATE);
	ret = vc_dispmanx_update_submit_sync(update);
	CHECK(ret == 0);
	curr_res = !curr_res;
}

void GLFramebuffer::blit_osd()
{
	int ret;
	ret = vc_dispmanx_resource_write_data(res[curr_res], type, pitch, image, &dst_rect);
	CHECK(ret == 0);
	update = vc_dispmanx_update_start(10);
	CHECK(update);
	ret = vc_dispmanx_element_change_source(update, element, res[curr_res]);
	CHECK(ret == 0);
	ret = vc_dispmanx_update_submit_sync(update);
	CHECK(ret == 0);
	curr_res = !curr_res;
}
