#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <stdio.h>
#include <stdint.h>

#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

typedef enum {
    OUTPUT_INIT,
    OUTPUT_ADD,
    OUTPUT_DEL,
    OUTPUT_CAPABILITIES,
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
    OUTPUT_SWITCH,
    OUTPUT_SLOWMOTION,
    OUTPUT_AUDIOMUTE,
    OUTPUT_REVERSE,
    OUTPUT_DISCONTINUITY_REVERSE,
    OUTPUT_GET_FRAME_COUNT,
} OutputCmd_t;

typedef struct {
    uint8_t *data;
    unsigned int len;

    int uNoOfChannels;
    int uSampleRate;
    int uBitsPerSample;
    int bLittleEndian;

    uint64_t pts;

    char *type;

    /* context from ffmpeg */
    AVFormatContext *avfc;
    /* stream from ffmpeg */
    AVStream *stream;
} AudioVideoOut_t;

struct Context_s;
typedef struct Context_s Context_t;

typedef struct Output_s {
    char *Name;
    int (*Command) (Context_t *, OutputCmd_t, void *);
    int (*Write) (Context_t *, AudioVideoOut_t *privateData);
    char **Capabilities;

} Output_t;

extern Output_t LinuxDvbOutput;
extern Output_t SubtitleOutput;

typedef struct OutputHandler_s {
    char *Name;
    Output_t *audio;
    Output_t *video;
    int (*Command) (Context_t *, OutputCmd_t, void *);
} OutputHandler_t;

#endif
