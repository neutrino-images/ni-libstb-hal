#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <stdio.h>
#include <stdint.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

typedef enum {
    OUTPUT_INIT,
    OUTPUT_ADD,
    OUTPUT_DEL,
    OUTPUT_PLAY,
    OUTPUT_STOP,
    OUTPUT_PAUSE,
    OUTPUT_OPEN,
    OUTPUT_CLOSE,
    OUTPUT_FLUSH,
    OUTPUT_CONTINUE,
    OUTPUT_FASTFORWARD,
    OUTPUT_AVSYNC,
    OUTPUT_CLEAR,
    OUTPUT_PTS,
    OUTPUT_SLOWMOTION,
    OUTPUT_AUDIOMUTE,
    OUTPUT_REVERSE,
    OUTPUT_DISCONTINUITY_REVERSE,
    OUTPUT_GET_FRAME_COUNT,
} OutputCmd_t;

struct Player;

typedef struct Output_s {
    const char *Name;
    int (*Command) (Player *, OutputCmd_t, const char *);
    bool (*Write) (AVFormatContext *avfc, AVStream *stream, AVPacket *packet, int64_t &Pts);
    const char **Capabilities;

} Output_t;

extern Output_t LinuxDvbOutput;
extern Output_t SubtitleOutput;

typedef struct OutputHandler_s {
    const char *Name;
    Output_t *audio;
    Output_t *video;
    int (*Command) (Player *, OutputCmd_t, const char *);
} OutputHandler_t;

#endif
