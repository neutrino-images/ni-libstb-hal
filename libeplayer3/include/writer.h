#ifndef WRITER_H_
#define WRITER_H_

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include <linux/dvb/stm_ioctls.h>

#define AV_CODEC_ID_INJECTPCM AV_CODEC_ID_PCM_S16LE

class Writer
{
	public:
		static void Register(Writer *w, enum AVCodecID id, video_encoding_t encoding);
		static void Register(Writer *w, enum AVCodecID id, audio_encoding_t encoding);
		static video_encoding_t GetVideoEncoding(enum AVCodecID id);
		static audio_encoding_t GetAudioEncoding(enum AVCodecID id);
		static Writer *GetWriter(enum AVCodecID id, enum AVMediaType codec_type);

		virtual void Init(void) { }
		virtual bool Write(int fd, AVFormatContext *avfc, AVStream *stream, AVPacket *packet, int64_t &pts);

		Writer() { Init (); }
		~Writer() {}
};
#endif
