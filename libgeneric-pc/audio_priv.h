/*
 * (C) 2010-2013 Stefan Seyfried
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * private stuff for the audio decoder, only used inside libstb-hal
 */

#include <OpenThreads/Thread>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <ao/ao.h>
}

class ADec : public OpenThreads::Thread
{
public:
	ADec();
	~ADec();
	int Start();
	int Stop();
	int PrepareClipPlay(int ch, int srate, int bits, int le);
	int WriteClip(unsigned char *buffer, int size);
	void getAudioInfo(int &type, int &layer, int &freq, int &bitrate, int &mode);
	int my_read(uint8_t *buf, int buf_size);
	int64_t getPts() { return curr_pts; };
private:
	bool thread_started;
	int64_t curr_pts;
	void run();

	ao_device *adevice;
	ao_sample_format sformat;
	uint8_t *dmxbuf;
	int bufpos;
	AVCodecContext *c;
	AVCodecParameters *p;
};

