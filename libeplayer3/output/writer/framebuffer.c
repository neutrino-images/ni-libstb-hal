/*
 * framebuffer output/writer handling.
 *
 * This is a hacky implementation of a framebuffer output for the subtitling.
 * This is ment as a POV, later this should be implemented in enigma2 and
 * neutrino.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <memory.h>
#include <asm/types.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>

#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "writer.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define FB_DEBUG

#ifdef FB_DEBUG

static short debug_level = 0;

#define fb_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define fb_printf(level, fmt, x...)
#endif

#ifndef FB_SILENT
#define fb_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define fb_err(fmt, x...)
#endif

#define _r(c) ((c)>>24)
#define _g(c) (((c)>>16)&0xFF)
#define _b(c) (((c)>>8)&0xFF)
#define _a(c) ((c)&0xFF)

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */


/* ***************************** */
/* Writer Functions              */
/* ***************************** */

static int reset()
{
	return 0;
}

static int writeData(void* _call)
{
	int res = 0;

	WriterFBCallData_t* call = (WriterFBCallData_t*) _call;

	fb_printf(100, "\n");

	if (!call)
	{
		fb_err("call data is NULL...\n");
		return 0;
	}

	if (!call->destination)
	{
		fb_err("frame buffer == NULL. ignoring ...\n");
		return 0;
	}

	int dst_stride = call->destStride/sizeof(uint32_t);
	int dst_delta = dst_stride - call->Width;
	uint32_t *dst = call->destination + call->y * dst_stride + call->x;

	if (call->data)
	{
		int src_delta = call->Stride - call->Width;
		unsigned char *src = call->data;
		static uint32_t last_color = 0, colortable[256];

		if (last_color != call->color) {
			// call->color is rgba, our spark frame buffer is argb
			uint32_t c = call->color >> 8, a = 255 - (call->color & 0xff);
			int i;
			for (i = 0; i < 256; i++) {
				uint32_t k = (a * i) >> 8;
				colortable[i] = k ? (c | (k << 24)) : 0;
			}
			last_color = call->color;
		}

		fb_printf(100, "x		%d\n", call->x);
		fb_printf(100, "y		%d\n", call->y);
		fb_printf(100, "width		%d\n", call->Width);
		fb_printf(100, "height		%d\n", call->Height);
		fb_printf(100, "stride		%d\n", call->Stride);
		fb_printf(100, "color		0x%.8x\n", call->color);
		fb_printf(100, "data		%p\n", call->data);
		fb_printf(100, "dest		%p\n", call->destination);
		fb_printf(100, "dest.stride	%d\n", call->destStride);

		unsigned char *src_final = src + call->Height * call->Width;
		for (; src < src_final; dst += dst_delta, src += src_delta) {
			u_char *src_end = src + call->Width;
			for (; src < src_end; dst++, src++) {
				uint32_t c = colortable[*src];
				if (c)
					*dst = c;
			}
		}
	} else {
		uint32_t *dst_final = dst + call->Width + call->Height * dst_stride;
		for (; dst < dst_final; dst += dst_delta) {
			uint32_t *dst_end = dst + call->Width;
			for (; dst < dst_end; dst++)
				*dst = 0;
		}
	}

	fb_printf(100, "< %d\n", res);
	return res;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */
static WriterCaps_t caps = {
	"framebuffer",
	eGfx,
	"framebuffer",
	0
};

struct Writer_s WriterFramebuffer = {
	&reset,
	&writeData,
	NULL,
	&caps
};
