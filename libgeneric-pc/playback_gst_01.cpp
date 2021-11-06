/*
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>

#include <pthread.h>
#include <syscall.h>

#include "dmx_lib.h"
#include "audio_lib.h"
#include "video_lib.h"
#include "glfb.h"

#include "playback_gst.h"

#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_PLAYBACK, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_PLAYBACK, this, args)
#define hal_debug_c(args...) _hal_debug(HAL_DEBUG_PLAYBACK, NULL, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_PLAYBACK, NULL, args)

static const char *FILENAME = "[playback.cpp]";

#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#include <gst/interfaces/xoverlay.h>

typedef enum
{
	GST_PLAY_FLAG_VIDEO         = 0x00000001,
	GST_PLAY_FLAG_AUDIO         = 0x00000002,
	GST_PLAY_FLAG_TEXT          = 0x00000004,
	GST_PLAY_FLAG_VIS           = 0x00000008,
	GST_PLAY_FLAG_SOFT_VOLUME   = 0x00000010,
	GST_PLAY_FLAG_NATIVE_AUDIO  = 0x00000020,
	GST_PLAY_FLAG_NATIVE_VIDEO  = 0x00000040,
	GST_PLAY_FLAG_DOWNLOAD      = 0x00000080,
	GST_PLAY_FLAG_BUFFERING     = 0x000000100
} GstPlayFlags;


GstElement *m_gst_playbin = NULL;
GstElement *audioSink = NULL;
GstElement *videoSink = NULL;
gchar *uri = NULL;
GstTagList *m_stream_tags = 0;
static int end_eof = 0;

extern GLFramebuffer *glfb;

gint match_sinktype(GstElement *element, gpointer type)
{
	return strcmp(g_type_name(G_OBJECT_TYPE(element)), (const char *)type);
}
GstBusSyncReply Gst_bus_call(GstBus *bus, GstMessage *msg, gpointer user_data)
{
	gchar *sourceName;

	// source
	GstObject *source;
	source = GST_MESSAGE_SRC(msg);

	if (!GST_IS_OBJECT(source))
		return GST_BUS_DROP;

	sourceName = gst_object_get_name(source);

	switch (GST_MESSAGE_TYPE(msg))
	{
		case GST_MESSAGE_EOS:
		{
			g_message("End-of-stream");
			end_eof = 1;
			break;
		}

		case GST_MESSAGE_ERROR:
		{
			gchar *debug;
			GError *err;
			gst_message_parse_error(msg, &err, &debug);
			g_free(debug);
			hal_info_c("%s:%s - GST_MESSAGE_ERROR: %s (%i) from %s\n", FILENAME, __FUNCTION__, err->message, err->code, sourceName);
			if (err->domain == GST_STREAM_ERROR)
			{
				if (err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND)
				{
					if (g_strrstr(sourceName, "videosink"))
						hal_info_c("%s:%s - GST_MESSAGE_ERROR: videosink\n", FILENAME, __FUNCTION__); // FIXME: how shall playback handle this event???
					else if (g_strrstr(sourceName, "audiosink"))
						hal_info_c("%s:%s - GST_MESSAGE_ERROR: audioSink\n", FILENAME, __FUNCTION__); // FIXME: how shall playback handle this event???
				}
			}
			g_error_free(err);

			end_eof = 1; // NOTE: just to exit

			break;
		}

		case GST_MESSAGE_INFO:
		{
			gchar *debug;
			GError *inf;

			gst_message_parse_info(msg, &inf, &debug);
			g_free(debug);
			if (inf->domain == GST_STREAM_ERROR && inf->code == GST_STREAM_ERROR_DECODE)
			{
				if (g_strrstr(sourceName, "videosink"))
					hal_info_c("%s:%s - GST_MESSAGE_INFO: videosink\n", FILENAME, __FUNCTION__); // FIXME: how shall playback handle this event???
			}
			g_error_free(inf);
			break;
		}

		case GST_MESSAGE_TAG:
		{
			GstTagList *tags, *result;
			gst_message_parse_tag(msg, &tags);

			result = gst_tag_list_merge(m_stream_tags, tags, GST_TAG_MERGE_REPLACE);
			if (result)
			{
				if (m_stream_tags)
					gst_tag_list_free(m_stream_tags);
				m_stream_tags = result;
			}

			const GValue *gv_image = gst_tag_list_get_value_index(tags, GST_TAG_IMAGE, 0);
			if (gv_image)
			{
				GstBuffer *buf_image;
				buf_image = gst_value_get_buffer(gv_image);
				int fd = open("/tmp/.id3coverart", O_CREAT | O_WRONLY | O_TRUNC, 0644);
				if (fd >= 0)
				{
					int ret = write(fd, GST_BUFFER_DATA(buf_image), GST_BUFFER_SIZE(buf_image));
					close(fd);
					hal_info_c("%s:%s - GST_MESSAGE_INFO: cPlayback::state /tmp/.id3coverart %d bytes written\n", FILENAME, __FUNCTION__, ret);
				}
				//FIXME: how shall playback handle this event???
			}
			gst_tag_list_free(tags);
			hal_info_c("%s:%s - GST_MESSAGE_INFO: update info tags\n", FILENAME, __FUNCTION__); // FIXME: how shall playback handle this event???
			break;
		}

		case GST_MESSAGE_STATE_CHANGED:
		{
			if (GST_MESSAGE_SRC(msg) != GST_OBJECT(m_gst_playbin))
				break;

			GstState old_state, new_state;
			gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);

			if (old_state == new_state)
				break;
			hal_info_c("%s:%s - GST_MESSAGE_STATE_CHANGED: state transition %s -> %s\n", FILENAME, __FUNCTION__, gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));

			GstStateChange transition = (GstStateChange)GST_STATE_TRANSITION(old_state, new_state);

			switch (transition)
			{
				case GST_STATE_CHANGE_NULL_TO_READY:
				{
				}
				break;
				case GST_STATE_CHANGE_READY_TO_PAUSED:
				{
					GstIterator *children;
					if (audioSink)
					{
						gst_object_unref(GST_OBJECT(audioSink));
						audioSink = NULL;
					}

					if (videoSink)
					{
						gst_object_unref(GST_OBJECT(videoSink));
						videoSink = NULL;
					}
					children = gst_bin_iterate_recurse(GST_BIN(m_gst_playbin));
					audioSink = GST_ELEMENT_CAST(gst_iterator_find_custom(children, (GCompareFunc)match_sinktype, (gpointer)"GstDVBAudioSink"));
					videoSink = GST_ELEMENT_CAST(gst_iterator_find_custom(children, (GCompareFunc)match_sinktype, (gpointer)"GstDVBVideoSink"));
					gst_iterator_free(children);

				}
				break;
				case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
				{
				}
				break;
				case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
				{
				}
				break;
				case GST_STATE_CHANGE_PAUSED_TO_READY:
				{
					if (audioSink)
					{
						gst_object_unref(GST_OBJECT(audioSink));
						audioSink = NULL;
					}
					if (videoSink)
					{
						gst_object_unref(GST_OBJECT(videoSink));
						videoSink = NULL;
					}
				}
				break;
				case GST_STATE_CHANGE_READY_TO_NULL:
				{
				}
				break;
			}
			break;
		}
#if 0
		case GST_MESSAGE_ELEMENT:
		{
			if (gst_structure_has_name(gst_message_get_structure(msg), "prepare-xwindow-id"))
			{
				// set window id
				gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(GST_MESSAGE_SRC(msg)), glfb->getWindowID());

				// reshape window
				gst_x_overlay_set_render_rectangle(GST_X_OVERLAY(GST_MESSAGE_SRC(msg)), 0, 0, glfb->getOSDWidth(), glfb->getOSDHeight());

				// sync frames
				gst_x_overlay_expose(GST_X_OVERLAY(GST_MESSAGE_SRC(msg)));
			}
		}
#endif
		break;
		default:
			break;
	}

	return GST_BUS_DROP;
}


cPlayback::cPlayback(int num)
{
	hal_info("%s:%s\n", FILENAME, __FUNCTION__);
	const gchar *nano_str;
	guint major, minor, micro, nano;

	gst_init(NULL, NULL);

	gst_version(&major, &minor, &micro, &nano);

	if (nano == 1)
		nano_str = "(CVS)";
	else if (nano == 2)
		nano_str = "(Prerelease)";
	else
		nano_str = "";

	hal_info("%s:%s - This program is linked against GStreamer %d.%d.%d %s\n",
		FILENAME, __FUNCTION__, major, minor, micro, nano_str);

	mAudioStream = 0;
	mSpeed = 0;

	playing = false;
	playstate = STATE_STOP;
}

cPlayback::~cPlayback()
{
	hal_info("%s:%s\n", FILENAME, __FUNCTION__);
	//FIXME: all deleting stuff is done in Close()
}

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	hal_info("%s: PlayMode %d\n", __func__, PlayMode);
	return true;
}

// used by movieplay
void cPlayback::Close(void)
{
	hal_info("%s:%s\n", FILENAME, __FUNCTION__);

	Stop();

	// disconnect bus handler
	if (m_gst_playbin)
	{
		// disconnect sync handler callback
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_gst_playbin));
		gst_bus_set_sync_handler(bus, NULL, NULL);
		gst_object_unref(bus);
		hal_info("%s:%s - GST bus handler closed\n", FILENAME, __FUNCTION__);
	}

	if (m_stream_tags)
		gst_tag_list_free(m_stream_tags);

	// close gst
	if (m_gst_playbin)
	{
		if (audioSink)
		{
			gst_object_unref(GST_OBJECT(audioSink));
			audioSink = NULL;

			hal_info("%s:%s - GST audio Sink closed\n", FILENAME, __FUNCTION__);
		}

		if (videoSink)
		{
			gst_object_unref(GST_OBJECT(videoSink));
			videoSink = NULL;

			hal_info("%s:%s - GST video Sink closed\n", FILENAME, __FUNCTION__);
		}

		// unref m_gst_playbin
		gst_object_unref(GST_OBJECT(m_gst_playbin));
		hal_info("%s:%s - GST playbin closed\n", FILENAME, __FUNCTION__);

		m_gst_playbin = NULL;
	}
}

// start
bool cPlayback::Start(std::string filename, std::string headers)
{
	return Start((char *) filename.c_str(), 0, 0, 0, 0, 0, headers);
}

bool cPlayback::Start(char *filename, int /*vpid*/, int /*vtype*/, int /*apid*/, int /*ac3*/, int /*duration*/, std::string headers)
{
	hal_info("%s:%s\n", FILENAME, __FUNCTION__);

	mAudioStream = 0;

	//create playback path
	char file[400] = {""};
	bool isHTTP = false;

	if (!strncmp("http://", filename, 7))
	{
		isHTTP = true;
	}
	else if (!strncmp("file://", filename, 7))
	{
		isHTTP = false;
	}
	else if (!strncmp("upnp://", filename, 7))
	{
		isHTTP = true;
	}
	else if (!strncmp("rtmp://", filename, 7))
	{
		isHTTP = true;
	}
	else if (!strncmp("rtsp://", filename, 7))
	{
		isHTTP = true;
	}
	else if (!strncmp("mms://", filename, 6))
	{
		isHTTP = true;
	}
	else
		strcat(file, "file://");

	strcat(file, filename);

	if (isHTTP)
		uri = g_uri_escape_string(filename, G_URI_RESERVED_CHARS_GENERIC_DELIMITERS, true);
	else
		uri = g_filename_to_uri(filename, NULL, NULL);

	hal_info("%s:%s - filename=%s\n", FILENAME, __FUNCTION__, filename);

	// create gst pipeline
	m_gst_playbin = gst_element_factory_make("playbin2", "playbin");

	if (m_gst_playbin)
	{
		hal_info("%s:%s - m_gst_playbin\n", FILENAME, __FUNCTION__);

		guint flags;
		g_object_get(G_OBJECT(m_gst_playbin), "flags", &flags, NULL);
		/* avoid video conversion, let the (hardware) sinks handle that */
		flags |= GST_PLAY_FLAG_NATIVE_VIDEO;
		/* volume control is done by hardware */
		flags &= ~GST_PLAY_FLAG_SOFT_VOLUME;

		g_object_set(G_OBJECT(m_gst_playbin), "uri", uri, NULL);
		g_object_set(G_OBJECT(m_gst_playbin), "flags", flags, NULL);

		//gstbus handler
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_gst_playbin));
		gst_bus_set_sync_handler(bus, Gst_bus_call, NULL);
		gst_object_unref(bus);

		// state playing
		gst_element_set_state(GST_ELEMENT(m_gst_playbin), GST_STATE_PLAYING);

		playing = true;
		playstate = STATE_PLAY;
	}
	else
	{
		hal_info("%s:%s - failed to create GStreamer pipeline!, sorry we can not play\n", FILENAME, __FUNCTION__);
		playing = false;

		return false;
	}

	g_free(uri);

	// set buffer size
	/* increase the default 2 second / 2 MB buffer limitations to 5s / 5MB */
	int m_buffer_size = 5 * 1024 * 1024;
	//g_object_set(G_OBJECT(m_gst_playbin), "buffer-duration", 5LL * GST_SECOND, NULL);
	g_object_set(G_OBJECT(m_gst_playbin), "buffer-size", m_buffer_size, NULL);

	return true;
}

bool cPlayback::Play(void)
{
	hal_info("%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	if (playing == true)
		return true;

	if (m_gst_playbin)
	{
		gst_element_set_state(GST_ELEMENT(m_gst_playbin), GST_STATE_PLAYING);

		playing = true;
		playstate = STATE_PLAY;
	}
	hal_info("%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	return playing;
}

bool cPlayback::Stop(void)
{
	if (playing == false)
		return false;
	hal_info("%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	// stop
	if (m_gst_playbin)
	{
		gst_element_set_state(m_gst_playbin, GST_STATE_NULL);
	}

	playing = false;

	hal_info("%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	playstate = STATE_STOP;

	return true;
}

bool cPlayback::SetAPid(int pid, bool /*ac3*/)
{
	hal_info("%s: pid %i\n", __func__, pid);

	int current_audio;

	if (pid != mAudioStream)
	{
		g_object_set(G_OBJECT(m_gst_playbin), "current-audio", pid, NULL);
		printf("%s: switched to audio stream %i\n", __FUNCTION__, pid);
		mAudioStream = pid;
	}

	return true;
}

void cPlayback::trickSeek(int ratio)
{
	bool validposition = false;
	gint64 pos = 0;
	int position;
	int duration;

	if (GetPosition(position, duration))
	{
		validposition = true;
		pos = position;
	}

	gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);

	if (validposition)
	{
		if (ratio >= 0.0)
			gst_element_seek(m_gst_playbin, ratio, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SKIP), GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_SET, -1);
		else
			gst_element_seek(m_gst_playbin, ratio, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SKIP), GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, pos);
	}
}

bool cPlayback::SetSpeed(int speed)
{
	hal_info("%s:%s speed %d\n", FILENAME, __FUNCTION__, speed);

	if (playing == false)
		return false;

	if (m_gst_playbin)
	{
		// pause
		if (speed == 0)
		{
			gst_element_set_state(m_gst_playbin, GST_STATE_PAUSED);
			//trickSeek(0);
			playstate = STATE_PAUSE;
		}
		// play/continue
		else if (speed == 1)
		{
			trickSeek(1);
			//gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);
			//
			playstate = STATE_PLAY;
		}
		//ff
		else if (speed > 1)
		{
			trickSeek(speed);
			//
			playstate = STATE_FF;
		}
		//rf
		else if (speed < 0)
		{
			trickSeek(speed);
			//
			playstate = STATE_REW;
		}
	}

	mSpeed = speed;

	return true;
}

bool cPlayback::SetSlow(int slow)
{
	hal_info("%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	if (playing == false)
		return false;

	if (m_gst_playbin)
	{
		trickSeek(0.5);
	}

	playstate = STATE_SLOW;

	mSpeed = slow;

	return true;
}

bool cPlayback::GetSpeed(int &speed) const
{
	speed = mSpeed;

	return true;
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
{
	if (playing == false)
		return false;

	//EOF
	if (end_eof)
	{
		end_eof = 0;
		return false;
	}

	if (m_gst_playbin)
	{
		//position
		GstFormat fmt = GST_FORMAT_TIME; //Returns time in nanosecs

		gint64 pts = 0;
		unsigned long long int sec = 0;

		gst_element_query_position(m_gst_playbin, &fmt, &pts);
		position = pts / 1000000.0;

		// duration
		GstFormat fmt_d = GST_FORMAT_TIME; //Returns time in nanosecs
		double length = 0;
		gint64 len;

		gst_element_query_duration(m_gst_playbin, &fmt_d, &len);
		length = len / 1000000.0;
		if (length < 0)
			length = 0;

		duration = (int)(length);
	}

	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	hal_info("%s: pos %d abs %d playing %d\n", __func__, position, absolute, playing);

	if (playing == false)
		return false;

	gint64 time_nanoseconds;
	gint64 pos;
	GstFormat fmt = GST_FORMAT_TIME;

	if (m_gst_playbin)
	{
		gst_element_query_position(m_gst_playbin, &fmt, &pos);
		time_nanoseconds = pos + (position * 1000000.0);
		if (time_nanoseconds < 0)
			time_nanoseconds = 0;

		gst_element_seek(m_gst_playbin, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, time_nanoseconds, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
	}

	return true;
}

void cPlayback::FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string *language)
{
	hal_info("%s:%s\n", FILENAME, __FUNCTION__);

	if (m_gst_playbin)
	{
		gint i, n_audio = 0;
		//GstStructure * structure = NULL;

		// get audio
		g_object_get(m_gst_playbin, "n-audio", &n_audio, NULL);
		printf("%s: %d audio\n", __FUNCTION__, n_audio);

		if (n_audio == 0)
			return;

		for (i = 0; i < n_audio; i++)
		{
			// apids
			apids[i] = i;

			GstPad *pad = 0;
			g_signal_emit_by_name(m_gst_playbin, "get-audio-pad", i, &pad);
			GstCaps *caps = gst_pad_get_negotiated_caps(pad);
			if (!caps)
				continue;

			GstStructure *structure = gst_caps_get_structure(caps, 0);
			//const gchar *g_type = gst_structure_get_name(structure);

			//if (!structure)
			//return atUnknown;
			//ac3flags[0] = 0;

			// ac3flags
			if (gst_structure_has_name(structure, "audio/mpeg"))
			{
				gint mpegversion, layer = -1;

				if (!gst_structure_get_int(structure, "mpegversion", &mpegversion))
					//return atUnknown;
					ac3flags[i] = 0;

				switch (mpegversion)
				{
					case 1:
					/*
					{
						gst_structure_get_int (structure, "layer", &layer);
						if ( layer == 3 )
							return atMP3;
						else
							return atMPEG;
							ac3flags[0] = 4;
						break;
					}
					*/
						ac3flags[i] = 4;
					case 2:
						//return atAAC;
						ac3flags[i] = 5;
					case 4:
						//return atAAC;
						ac3flags[i] = 5;
					default:
						//return atUnknown;
						ac3flags[i] = 0;
				}
			}
			else if (gst_structure_has_name(structure, "audio/x-ac3") || gst_structure_has_name(structure, "audio/ac3"))
				//return atAC3;
				ac3flags[i] = 1;
			else if (gst_structure_has_name(structure, "audio/x-dts") || gst_structure_has_name(structure, "audio/dts"))
				//return atDTS;
				ac3flags[i] = 6;
			else if (gst_structure_has_name(structure, "audio/x-raw-int"))
				//return atPCM;
				ac3flags[i] = 0;

			gst_caps_unref(caps);
		}

		// numpids
		*numpida = i;
	}
}

void cPlayback::getMeta()
{
	if (playing)
		return;
}

bool cPlayback::SyncAV(void)
{
	hal_info("%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	if (playing == false)
		return false;

	return true;
}

void cPlayback::RequestAbort()
{
}

void cPlayback::FindAllSubs(int * /*pids*/, unsigned int * /*supp*/, unsigned int *num, std::string * /*lang*/)
{
	printf("%s:%s\n", FILENAME, __func__);
	*num = 0;
}

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();
}

void cPlayback::GetTitles(std::vector<int> &playlists, std::vector<std::string> &titles, int &current)
{
	playlists.clear();
	titles.clear();
	current = 0;
}

void cPlayback::SetTitle(int /*title*/)
{
}

bool cPlayback::SelectSubtitles(int pid, std::string charset)
{
	printf("%s:%s pid %i, charset: %s\n", FILENAME, __func__, pid, charset.c_str());
	return true;
}

void cPlayback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	keys.clear();
	values.clear();
}

void cPlayback::FindAllTeletextsubtitlePids(int *, unsigned int *numpids, std::string *, int *, int *)
{
	*numpids = 0;
}

void cPlayback::FindAllSubtitlePids(int * /*pids*/, unsigned int *numpids, std::string * /*language*/)
{
	*numpids = 0;
}

bool cPlayback::SetSubtitlePid(int /*pid*/)
{
	return true;
}

void cPlayback::GetPts(uint64_t &/*pts*/)
{
}

bool cPlayback::SetTeletextPid(int /*pid*/)
{
	return true;
}

uint64_t cPlayback::GetReadCount()
{
	return 0;
}
