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

#ifndef __glthread__
#define __glthread__
#include <OpenThreads/Thread>
#include <vector>
#include <linux/fb.h> /* for screeninfo etc. */

class GLFramebuffer : public OpenThreads::Thread
{
	public:
		GLFramebuffer(int x, int y);
		~GLFramebuffer();
		std::vector<unsigned char> *getOSDBuffer() { return &osd_buf; } /* pointer to OSD bounce buffer */
		void blit();
		fb_var_screeninfo getScreenInfo() { return si; }

	private:
		void *pdata;	/* not yet used */
		fb_var_screeninfo si;
		std::vector<unsigned char> osd_buf; /* silly bounce buffer */
		void run();	/* for OpenThreads::Thread */

		void setup();
		void blit_osd();
};
#endif
