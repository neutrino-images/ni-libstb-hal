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

#include "audio_lib.h"
#include "video_lib.h"

#include "playback_gst.h"

#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_PLAYBACK, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_PLAYBACK, this, args)
#define hal_debug_c(args...) _hal_debug(HAL_DEBUG_PLAYBACK, NULL, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_PLAYBACK, NULL, args)

static const char * FILENAME = "[playback_gst.cpp]";
extern cVideo * videoDecoder;
extern cAudio * audioDecoder;

#include <gst/gst.h>
#include <gst/tag/tag.h>
#include <gst/mpegts/mpegts.h>
#include <gst/pbutils/missing-plugins.h>

typedef enum
{
	GST_PLAY_FLAG_VIDEO             = (1 << 0),
	GST_PLAY_FLAG_AUDIO             = (1 << 1),
	GST_PLAY_FLAG_TEXT              = (1 << 2),
	GST_PLAY_FLAG_VIS               = (1 << 3),
	GST_PLAY_FLAG_SOFT_VOLUME       = (1 << 4),
	GST_PLAY_FLAG_NATIVE_AUDIO      = (1 << 5),
	GST_PLAY_FLAG_NATIVE_VIDEO      = (1 << 6),
	GST_PLAY_FLAG_DOWNLOAD          = (1 << 7),
	GST_PLAY_FLAG_BUFFERING         = (1 << 8),
	GST_PLAY_FLAG_DEINTERLACE       = (1 << 9),
	GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10),
	GST_PLAY_FLAG_FORCE_FILTERS     = (1 << 11),
} GstPlayFlags;


GstElement * m_gst_playbin = NULL;
GstElement * audioSink = NULL;
GstElement * videoSink = NULL;
gchar * uri = NULL;
GstTagList * m_stream_tags = NULL;
pthread_mutex_t mutex_tag_ist;

static int end_eof = 0;
#define HTTP_TIMEOUT 30
// taken from record.h
#define REC_MAX_APIDS 10
int real_apids[REC_MAX_APIDS];

gint match_sinktype(const GValue *velement, const gchar *type)
{
	GstElement *element = GST_ELEMENT_CAST(g_value_get_object(velement));
	return strcmp(g_type_name(G_OBJECT_TYPE(element)), type);
}

void resetPids()
{
	for (unsigned int i = 0; i < REC_MAX_APIDS; i++) {
		real_apids[i] = 0;
	}
}

void processMpegTsSection(GstMpegtsSection* section)
{
	resetPids();
	int cnt = 0;
    if (section->section_type == GST_MPEGTS_SECTION_PMT) {
        const GstMpegtsPMT* pmt = gst_mpegts_section_get_pmt(section);
        for (guint i = 0; i < pmt->streams->len; ++i) {
            const GstMpegtsPMTStream* stream = static_cast<const GstMpegtsPMTStream*>(g_ptr_array_index(pmt->streams, i));
			if (stream->stream_type == 0x05 || stream->stream_type >= 0x80) {
				hal_info_c( "%s:%s Audio Stream pid: %d\n", FILENAME, __FUNCTION__, stream->pid);
				real_apids[cnt] = stream->pid;
				cnt++;
			}
		}
	}
}

void playbinNotifySource(GObject *object, GParamSpec *param_spec, gpointer user_data)
{
	GstElement *source = NULL;
	cPlayback *_this = (cPlayback*)user_data;
	g_object_get(object, "source", &source, NULL);

	if (source)
	{
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "timeout") != 0)
		{
			GstElementFactory *factory = gst_element_get_factory(source);
			if (factory)
			{
				const gchar *sourcename = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
				if (!strcmp(sourcename, "souphttpsrc"))
				{
					g_object_set(G_OBJECT(source), "timeout", HTTP_TIMEOUT, NULL);
				}
			}
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "ssl-strict") != 0)
		{
			g_object_set(G_OBJECT(source), "ssl-strict", FALSE, NULL);
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "user-agent") != 0 && !_this->user_agent.empty())
		{
			g_object_set(G_OBJECT(source), "user-agent", _this->user_agent.c_str(), NULL);
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "extra-headers") != 0 && !_this->extra_headers.empty())
		{
			GstStructure *extras = gst_structure_new_empty("extras");
			size_t pos = 0;
			while (pos != std::string::npos)
			{
				std::string name, value;
				size_t start = pos;
				size_t len = std::string::npos;
				pos = _this->extra_headers.find('=', pos);
				if (pos != std::string::npos)
				{
					len = pos - start;
					pos++;
					name = _this->extra_headers.substr(start, len);
					start = pos;
					len = std::string::npos;
					pos = _this->extra_headers.find('&', pos);
					if (pos != std::string::npos)
					{
						len = pos - start;
						pos++;
					}
					value = _this->extra_headers.substr(start, len);
				}
				if (!name.empty() && !value.empty())
				{
					GValue header;
					hal_info_c( "%s:%s setting extra-header '%s:%s'\n", FILENAME, __FUNCTION__, name.c_str(), value.c_str());
					memset(&header, 0, sizeof(GValue));
					g_value_init(&header, G_TYPE_STRING);
					g_value_set_string(&header, value.c_str());
					gst_structure_set_value(extras, name.c_str(), &header);
				}
				else
				{
					hal_info_c( "%s:%s Invalid header format %s\n", FILENAME, __FUNCTION__, _this->extra_headers.c_str());
					break;
				}
			}
			if (gst_structure_n_fields(extras) > 0)
			{
				g_object_set(G_OBJECT(source), "extra-headers", extras, NULL);
			}
			gst_structure_free(extras);
		}
		gst_object_unref(source);
	}
}

GstBusSyncReply Gst_bus_call(GstBus *bus, GstMessage *msg, gpointer user_data)
{
	// source
	GstObject * source;
	source = GST_MESSAGE_SRC(msg);

	if (!GST_IS_OBJECT(source))
		return GST_BUS_DROP;

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
		gchar * debug;
		GError *err;
		gst_message_parse_error(msg, &err, &debug);
		g_free (debug);
		gchar * sourceName = gst_object_get_name(source);
		hal_info_c( "%s:%s - GST_MESSAGE_ERROR: %s (%i) from %s\n", FILENAME, __FUNCTION__, err->message, err->code, sourceName );
		if ( err->domain == GST_STREAM_ERROR )
		{
			if ( err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND )
			{
				if ( g_strrstr(sourceName, "videosink") )
					hal_info_c( "%s:%s - GST_MESSAGE_ERROR: videosink\n", FILENAME, __FUNCTION__ ); //FIXME: how shall playback handle this event???
				else if ( g_strrstr(sourceName, "audiosink") )
					hal_info_c( "%s:%s - GST_MESSAGE_ERROR: audioSink\n", FILENAME, __FUNCTION__ ); //FIXME: how shall playback handle this event???
			}
		}
		g_error_free(err);
		if(sourceName)
			g_free(sourceName);

		end_eof = 1; 		// NOTE: just to exit

		break;
	}

	case GST_MESSAGE_INFO:
	{
		gchar *debug;
		GError *inf;

		gst_message_parse_info (msg, &inf, &debug);
		g_free (debug);
		if ( inf->domain == GST_STREAM_ERROR && inf->code == GST_STREAM_ERROR_DECODE )
		{
			gchar * sourceName = gst_object_get_name(source);
			if ( g_strrstr(sourceName, "videosink") )
				hal_info_c( "%s:%s - GST_MESSAGE_INFO: videosink\n", FILENAME, __FUNCTION__ ); //FIXME: how shall playback handle this event???
			if(sourceName)
				g_free(sourceName);

		}
		g_error_free(inf);
		break;
	}

	case GST_MESSAGE_TAG:
	{
		GstTagList *tags = NULL, *result = NULL;
		gst_message_parse_tag(msg, &tags);

		if(tags == NULL)
			break;
		if(!GST_IS_TAG_LIST(tags))
			break;

		pthread_mutex_lock (&mutex_tag_ist);

		result = gst_tag_list_merge(m_stream_tags, tags, GST_TAG_MERGE_REPLACE);
		if (result)
		{
			if (m_stream_tags && gst_tag_list_is_equal(m_stream_tags, result))
			{
				gst_tag_list_unref(tags);
				gst_tag_list_unref(result);

				pthread_mutex_unlock (&mutex_tag_ist);
				break;
			}
			if (m_stream_tags)
				gst_tag_list_unref(m_stream_tags);
			m_stream_tags = gst_tag_list_copy(result);
			gst_tag_list_unref(result);
		}

		pthread_mutex_unlock (&mutex_tag_ist);

		const GValue *gv_image = gst_tag_list_get_value_index(tags, GST_TAG_IMAGE, 0);
		if ( gv_image )
		{
			GstBuffer *buf_image;
			GstSample *sample;
			sample = (GstSample *)g_value_get_boxed(gv_image);
			buf_image = gst_sample_get_buffer(sample);
			int fd = open("/tmp/.id3coverart", O_CREAT|O_WRONLY|O_TRUNC, 0644);
			if (fd >= 0)
			{
				guint8 *data;
				gsize size;
				GstMapInfo map;
				gst_buffer_map(buf_image, &map, GST_MAP_READ);
				data = map.data;
				size = map.size;
				int ret = write(fd, data, size);
				gst_buffer_unmap(buf_image, &map);
				close(fd);
				hal_info_c("%s:%s - /tmp/.id3coverart %d bytes written\n", FILENAME, __FUNCTION__, ret);
			}
		}
		if (tags)
			gst_tag_list_unref(tags);
		hal_debug_c( "%s:%s - GST_MESSAGE_INFO: update info tags\n", FILENAME, __FUNCTION__);  //FIXME: how shall playback handle this event???
		break;
	}
    case GST_MESSAGE_ELEMENT:
	{
		GstMpegtsSection* section = gst_message_parse_mpegts_section(msg);
		if (section) {
			processMpegTsSection(section);
			gst_mpegts_section_unref(section);
		}
	}
	case GST_MESSAGE_STATE_CHANGED:
	{
		if(GST_MESSAGE_SRC(msg) != GST_OBJECT(m_gst_playbin))
			break;

		GstState old_state, new_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);

		if(old_state == new_state)
			break;
		hal_info_c( "%s:%s - GST_MESSAGE_STATE_CHANGED: state transition %s -> %s\n", FILENAME, __FUNCTION__, gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));

		GstStateChange transition = (GstStateChange)GST_STATE_TRANSITION(old_state, new_state);

		switch(transition)
		{
		case GST_STATE_CHANGE_NULL_TO_READY:
		{
		}	break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
		{
			GstIterator *children;
			GValue r = G_VALUE_INIT;

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
			if (children && gst_iterator_find_custom(children, (GCompareFunc)match_sinktype, &r, (gpointer)"GstDVBAudioSink"))
			{
				audioSink = GST_ELEMENT_CAST(g_value_dup_object (&r));
				g_value_unset (&r);
				hal_info_c( "%s %s - audio sink created\n", FILENAME, __FUNCTION__);
			}

			gst_iterator_free(children);
			children = gst_bin_iterate_recurse(GST_BIN(m_gst_playbin));
			if (children && gst_iterator_find_custom(children, (GCompareFunc)match_sinktype, &r, (gpointer)"GstDVBVideoSink"))
			{
				videoSink = GST_ELEMENT_CAST(g_value_dup_object (&r));
				g_value_unset (&r);
				hal_info_c( "%s %s - video sink created\n", FILENAME, __FUNCTION__);
			}
			gst_iterator_free(children);

		}
		break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		{
		}	break;
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		{
		}	break;
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
		}	break;
		}
		break;
	}
	break;
	default:
		break;
	}

	return GST_BUS_DROP;
}


cPlayback::cPlayback(int num)
{
	hal_info( "%s:%s\n", FILENAME, __FUNCTION__);
	const gchar *nano_str;
	guint major, minor, micro, nano;

	gst_init(NULL, NULL);

	gst_version (&major, &minor, &micro, &nano);

	gst_mpegts_initialize();

	pthread_mutex_init (&mutex_tag_ist, NULL);

	if (nano == 1)
		nano_str = "(CVS)";
	else if (nano == 2)
		nano_str = "(Prerelease)";
	else
		nano_str = "";

	hal_info( "%s:%s - This program is linked against GStreamer %d.%d.%d %s\n",
	         FILENAME, __FUNCTION__,
	         major, minor, micro, nano_str);

	mAudioStream = 0;
	mSpeed = 0;

	playing = false;
	playstate = STATE_STOP;
	decoders_closed = false;
	first = false;
}

cPlayback::~cPlayback()
{
	hal_info( "%s:%s\n", FILENAME, __FUNCTION__);
	//FIXME: all deleting stuff is done in Close()
	pthread_mutex_lock (&mutex_tag_ist);
	if (m_stream_tags)
		gst_tag_list_unref(m_stream_tags);
	pthread_mutex_unlock (&mutex_tag_ist);
}

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	hal_info("%s: PlayMode %d\n", __func__, PlayMode);

	if (PlayMode != PLAYMODE_TS)
	{
		audioDecoder->closeDevice();
		videoDecoder->closeDevice();
		decoders_closed = true;
	}

	init_jump = -1;
	return true;
}

// used by movieplay
void cPlayback::Close(void)
{
	hal_info( "%s:%s\n", FILENAME, __FUNCTION__);

	Stop();

	// disconnect bus handler
	if (m_gst_playbin)
	{
		// disconnect sync handler callback
		GstBus * bus = gst_pipeline_get_bus(GST_PIPELINE (m_gst_playbin));
		gst_bus_set_sync_handler(bus, NULL, NULL, NULL);
		if (bus)
			gst_object_unref(bus);
		hal_info( "%s:%s - GST bus handler closed\n", FILENAME, __FUNCTION__);
	}

	// close gst
	if (m_gst_playbin)
	{
		if (audioSink)
		{
			gst_object_unref(GST_OBJECT(audioSink));
			audioSink = NULL;

			hal_info( "%s:%s - GST audio Sink closed\n", FILENAME, __FUNCTION__);
		}

		if (videoSink)
		{
			gst_object_unref(GST_OBJECT(videoSink));
			videoSink = NULL;

			hal_info( "%s:%s - GST video Sink closed\n", FILENAME, __FUNCTION__);
		}

		// unref m_gst_playbin
		gst_object_unref (GST_OBJECT (m_gst_playbin));
		hal_info( "%s:%s - GST playbin closed\n", FILENAME, __FUNCTION__);

		m_gst_playbin = NULL;

		if (decoders_closed)
		{
			audioDecoder->openDevice();
			videoDecoder->openDevice();
			decoders_closed = false;
		}
	}

}

// start
bool cPlayback::Start(std::string filename, std::string headers)
{
	return Start((char*) filename.c_str(),0,0,0,0,0, headers);
}

bool cPlayback::Start(char *filename, int /*vpid*/, int /*vtype*/, int /*apid*/, int /*ac3*/, int /*duration*/, std::string headers)
{
	hal_info( "%s:%s\n", FILENAME, __FUNCTION__);

	if (!headers.empty())
		extra_headers = headers;
	else
		extra_headers.clear();

	resetPids();

	mAudioStream = 0;
	init_jump = -1;

	pthread_mutex_lock (&mutex_tag_ist);
	if (m_stream_tags)
		gst_tag_list_unref(m_stream_tags);
	m_stream_tags = NULL;
	pthread_mutex_unlock (&mutex_tag_ist);

	unlink("/tmp/.id3coverart");

	//create playback path
	bool isHTTP = false;

	if(!strncmp("http://", filename, 7))
	{
		isHTTP = true;
	}
	else if(!strncmp("https://", filename, 8))
	{
		isHTTP = true;
	}
	else if(!strncmp("file://", filename, 7))
	{
		isHTTP = false;
	}
	else if(!strncmp("upnp://", filename, 7))
	{
		isHTTP = true;
	}
	else if(!strncmp("rtmp://", filename, 7))
	{
		isHTTP = true;
	}
	else if(!strncmp("rtsp://", filename, 7))
	{
		isHTTP = true;
	}
	else if(!strncmp("mms://", filename, 6))
	{
		isHTTP = true;
	}

	if (isHTTP)
		uri = g_strdup_printf ("%s", filename);
	else
		uri = g_filename_to_uri(filename, NULL, NULL);

	hal_info("%s:%s - filename=%s\n", FILENAME, __FUNCTION__, filename);

	guint flags =	GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_NATIVE_VIDEO;

	/* increase the default 2 second / 2 MB buffer limitations to 5s / 5MB */
	int m_buffer_size = 5*1024*1024;

	// create gst pipeline
	m_gst_playbin = gst_element_factory_make ("playbin", "playbin");

	if(m_gst_playbin)
	{
		hal_info("%s:%s - m_gst_playbin\n", FILENAME, __FUNCTION__);

		if(isHTTP)
		{
			g_signal_connect (G_OBJECT (m_gst_playbin), "notify::source", G_CALLBACK (playbinNotifySource), this);

			// set buffer size
			g_object_set(G_OBJECT(m_gst_playbin), "buffer-size", m_buffer_size, NULL);
			g_object_set(G_OBJECT(m_gst_playbin), "buffer-duration", 5LL * GST_SECOND, NULL);
			flags |= GST_PLAY_FLAG_BUFFERING;
		}

		g_object_set(G_OBJECT (m_gst_playbin), "flags", flags, NULL);

		g_object_set(G_OBJECT (m_gst_playbin), "uri", uri, NULL);

		//gstbus handler
		GstBus * bus = gst_pipeline_get_bus( GST_PIPELINE(m_gst_playbin) );
		gst_bus_set_sync_handler(bus, Gst_bus_call, NULL, NULL);
		if (bus)
			gst_object_unref(bus);

		first = true;

		// state playing
		if(isHTTP)
		{
			gst_element_set_state(GST_ELEMENT(m_gst_playbin), GST_STATE_PLAYING);
			playstate = STATE_PLAY;
		}
		else
		{
			gst_element_set_state(GST_ELEMENT(m_gst_playbin), GST_STATE_PAUSED);
			playstate = STATE_PAUSE;
		}

		playing = true;
	}
	else
	{
		hal_info("%s:%s - failed to create GStreamer pipeline!, sorry we can not play\n", FILENAME, __FUNCTION__);
		playing = false;

		return false;
	}

	g_free(uri);

	return true;
}

bool cPlayback::Play(void)
{
	hal_info( "%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	if(playing == true)
		return true;

	if(m_gst_playbin)
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
	if(playing == false)
		return false;
	hal_info( "%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	// stop
	if(m_gst_playbin)
	{
		gst_element_set_state(m_gst_playbin, GST_STATE_NULL);
	}

	playing = false;

	hal_info( "%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	playstate = STATE_STOP;

	return true;
}

bool cPlayback::SetAPid(int pid, bool /*ac3*/)
{
	hal_info("%s: pid %i\n", __func__, pid);

	int to_audio = pid;

	for (unsigned int i = 0; i < REC_MAX_APIDS; i++) {
		if (real_apids[i])
			if (real_apids[i] == pid)
				to_audio = i;
	}

	if(to_audio != mAudioStream)
	{
		g_object_set (G_OBJECT (m_gst_playbin), "current-audio", to_audio, NULL);
		printf("%s: switched to audio stream %i\n", __FUNCTION__, to_audio);
		mAudioStream = to_audio;
	}

	return true;
}

void cPlayback::trickSeek(int ratio)
{
	GstFormat fmt = GST_FORMAT_TIME;
	gint64 pos = 0;

	gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);

	if (gst_element_query_position(m_gst_playbin, fmt, &pos))
	{
		if(ratio >= 0.0)
			gst_element_seek(m_gst_playbin, ratio, fmt, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SKIP), GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_SET, -1);
		else
			gst_element_seek(m_gst_playbin, ratio, fmt, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SKIP), GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, pos);
	}
}

bool cPlayback::SetSpeed(int speed)
{
	hal_info( "%s:%s speed %d\n", FILENAME, __FUNCTION__, speed);

	if (!decoders_closed)
	{
		audioDecoder->closeDevice();
		videoDecoder->closeDevice();
		decoders_closed = true;
		usleep(500000);
	}

	if(playing == false)
		return false;

	if(m_gst_playbin)
	{
		// pause
		if(speed == 0)
		{
			gst_element_set_state(m_gst_playbin, GST_STATE_PAUSED);
			//trickSeek(0);
			playstate = STATE_PAUSE;
		}
		// play/continue
		else if(speed == 1)
		{
			trickSeek(1);
			//gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);
			//
			playstate = STATE_PLAY;
		}
		//ff
		else if(speed > 1)
		{
			trickSeek(speed);
			//
			playstate = STATE_FF;
		}
		//rf
		else if(speed < 0)
		{
			trickSeek(speed);
			//
			playstate = STATE_REW;
		}

		if (init_jump > -1)
		{
			SetPosition(init_jump,true);
			init_jump = -1;
		}
	}

	mSpeed = speed;

	return true;
}

bool cPlayback::SetSlow(int slow)
{
	hal_info( "%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	if(playing == false)
		return false;

	if(m_gst_playbin)
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
	if(playing == false)
		return false;

	//EOF
	if(end_eof)
	{
		end_eof = 0;
		return false;
	}

	if(m_gst_playbin)
	{
		//position
		GstFormat fmt = GST_FORMAT_TIME; //Returns time in nanosecs
		gint64 pts = 0;
		if (audioSink || videoSink)
		{
			g_signal_emit_by_name(audioSink ? audioSink : videoSink, "get-decoder-time", &pts);
			if (!GST_CLOCK_TIME_IS_VALID(pts))
			{
				hal_info( "%s - %d failed\n", __FUNCTION__, __LINE__);
			}
		}
		else
		{
			if(!gst_element_query_position(m_gst_playbin, fmt, &pts))
			{
				hal_info( "%s - %d failed\n", __FUNCTION__, __LINE__);
			}
		}
		position = pts /  1000000.0;
		// duration
		GstFormat fmt_d = GST_FORMAT_TIME; //Returns time in nanosecs
		double length = 0;
		gint64 len;

		gst_element_query_duration(m_gst_playbin, fmt_d, &len);
		length = len / 1000000.0;
		if(length < 0)
			length = 0;

		duration = (int)(length);
	}

	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	hal_info("%s: pos %d abs %d playing %d\n", __func__, position, absolute, playing);


	if(m_gst_playbin)
	{
		if(first){
			GstState state;
			gst_element_get_state(m_gst_playbin, &state, NULL, GST_CLOCK_TIME_NONE);
			if ( (state == GST_STATE_PAUSED) && first)
			{
				init_jump = position;
				first = false;
				return false;
			}
		}

		gint64 time_nanoseconds;
		gint64 pos;
		GstFormat fmt = GST_FORMAT_TIME;
		if (!absolute)
		{
			gst_element_query_position(m_gst_playbin, fmt, &pos);
			time_nanoseconds = pos + (position * 1000000.0);
			if(time_nanoseconds < 0)
				time_nanoseconds = 0;
		}
		else
		{
			time_nanoseconds = position * 1000000.0;
		}

		gst_element_seek(m_gst_playbin, 1.0, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), GST_SEEK_TYPE_SET, time_nanoseconds, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
	}

	return true;
}

void cPlayback::FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string * language)
{
	hal_info( "%s:%s\n", FILENAME, __FUNCTION__);

	language->clear();
	*numpida = 0;

	if(m_gst_playbin)
	{
		gint i, n_audio = 0;

		// get audio
		g_object_get (m_gst_playbin, "n-audio", &n_audio, NULL);
		hal_info("%s: %d audio\n", __FUNCTION__, n_audio);

		if(n_audio == 0)
			return;

		for (i = 0; i < n_audio && i < REC_MAX_APIDS; i++)
		{
			// apids
			apids[i]= real_apids[i] ? real_apids[i] : i;

			GstPad * pad = 0;
			g_signal_emit_by_name (m_gst_playbin, "get-audio-pad", i, &pad);

			GstCaps * caps = gst_pad_get_current_caps(pad);
			if (pad)
				gst_object_unref(pad);

			if (!caps)
				continue;

			GstStructure * structure = gst_caps_get_structure(caps, 0);
			GstTagList * tags = NULL;
			gchar * g_lang = NULL;

			// ac3flags
			if ( gst_structure_has_name (structure, "audio/mpeg"))
			{
				gint mpegversion;

				if (!gst_structure_get_int (structure, "mpegversion", &mpegversion))
					ac3flags[i] = 0;

				switch (mpegversion)
				{
				case 1:
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
			else if ( gst_structure_has_name (structure, "audio/x-ac3") || gst_structure_has_name (structure, "audio/ac3") )
				//return atAC3;
				ac3flags[i] = 1;
			else if ( gst_structure_has_name (structure, "audio/x-dts") || gst_structure_has_name (structure, "audio/dts") )
				//return atDTS;
				ac3flags[i] = 6;
			else if ( gst_structure_has_name (structure, "audio/x-raw-int") )
				//return atPCM;
				ac3flags[i] = 0;

			if (caps)
				gst_caps_unref(caps);

			//(ac3flags[i] > 2) ?	ac3flags[i] = 1 : ac3flags[i] = 0;

			g_signal_emit_by_name (m_gst_playbin, "get-audio-tags", i, &tags);
			if (tags)
			{
				if (GST_IS_TAG_LIST(tags) && gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &g_lang))
				{
					std::string slang;
					if (gst_tag_check_language_code(g_lang))
						slang = gst_tag_get_language_name(g_lang);
					else
						slang = g_lang;
					if (slang.empty())
						language[i] = "unk";
					else
						language[i] = slang.c_str();
					hal_info("%s: language:%s\n", __FUNCTION__, language[i].c_str());
					g_free(g_lang);
				}
				gst_tag_list_free(tags);
			}
		}
		*numpida=i;
	}
}

void cPlayback::getMeta()
{
	if(playing)
		return;
}

bool cPlayback::SyncAV(void)
{
	hal_info( "%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);

	if(playing == false )
		return false;

	return true;
}

void cPlayback::RequestAbort()
{
}

void cPlayback::FindAllSubs(uint16_t *, unsigned short *, uint16_t *numpida, std::string *)
{
	printf("%s:%s\n", FILENAME, __func__);
	*numpida = 0;
}

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();
}

bool cPlayback::SelectSubtitles(int pid)
{
	printf("%s:%s pid %i\n", FILENAME, __func__, pid);
	return true;
}

void cPlayback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	keys.clear();
	values.clear();

	GstTagList *meta_list = NULL;
	pthread_mutex_lock (&mutex_tag_ist);

	if(m_stream_tags){
		meta_list = gst_tag_list_copy(m_stream_tags);
	}

	pthread_mutex_unlock (&mutex_tag_ist);

	if (meta_list == NULL)
		return;

	if (gst_tag_list_is_empty(meta_list)){
		gst_tag_list_unref(meta_list);
		return;
	}

	for (guint i = 0, icnt = gst_tag_list_n_tags(meta_list); i < icnt; i++)
	{
		const gchar *name = gst_tag_list_nth_tag_name(meta_list, i);
		if (!name)
		{
			continue;
		}

		for (guint j = 0, jcnt = gst_tag_list_get_tag_size(meta_list, name); j < jcnt; j++)
		{
			const GValue *val;

			val = gst_tag_list_get_value_index(meta_list, name, j);

			if (val == NULL)
				continue;

			if (G_VALUE_HOLDS_STRING(val))
			{
				keys.push_back(name);
				values.push_back(g_value_get_string(val));
			}
			else if (G_VALUE_HOLDS_UINT(val))
			{
				char buffer [50];
				keys.push_back(name);
				sprintf (buffer, "%u", g_value_get_uint(val));
				values.push_back(buffer);
			}
			else if (G_VALUE_HOLDS_DOUBLE(val))
			{
				char buffer [50];
				keys.push_back(name);
				sprintf (buffer, "%f", g_value_get_double(val));
				values.push_back(buffer);
			}
			else if (G_VALUE_HOLDS_BOOLEAN(val))
			{
				keys.push_back(name);
				values.push_back(g_value_get_boolean(val) ? "true" : "false");
			}
			else if (GST_VALUE_HOLDS_DATE_TIME(val))
			{
				GstDateTime *dt = (GstDateTime *) g_value_get_boxed(val);
				keys.push_back(name);
				values.push_back(gst_date_time_to_iso8601_string(dt));
			}
			else if (G_VALUE_HOLDS(val, G_TYPE_DATE))
			{
				keys.push_back(name);
				values.push_back(gst_value_serialize(val));
			}

		}
	}
	gst_tag_list_unref(meta_list);
	printf("%s:%s %d tags found\n", FILENAME, __func__, (int)keys.size());
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

int cPlayback::GetAPid(void)
{
	gint current_audio = 0;
	g_object_get (m_gst_playbin, "current-audio", &current_audio, NULL);
	hal_info("%s: %d audio\n", __FUNCTION__, current_audio);
	return real_apids[current_audio] ? real_apids[current_audio] : current_audio;
}

int cPlayback::GetVPid(void)
{
	gint current_video = 0;
	g_object_get (m_gst_playbin, "current-video", &current_video, NULL);
	hal_info("%s: %d video\n", __FUNCTION__, current_video);
	return current_video;
}

int cPlayback::GetSubtitlePid(void)
{
	return 0;
}

AVFormatContext *cPlayback::GetAVFormatContext()
{
	return NULL;
}

void cPlayback::ReleaseAVFormatContext()
{
}
