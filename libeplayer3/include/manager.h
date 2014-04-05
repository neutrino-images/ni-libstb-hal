#ifndef MANAGER_H_
#define MANAGER_H_

#include <stdio.h>
#include <stdint.h>
#include <string>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

typedef enum {
    MANAGER_ADD,
    MANAGER_LIST,
    MANAGER_GET,
    MANAGER_GETNAME,
    MANAGER_SET,
    MANAGER_GETENCODING,
    MANAGER_DEL,
    MANAGER_GET_TRACK,
    MANAGER_INIT_UPDATE
} ManagerCmd_t;

typedef enum {
    eTypeES,
    eTypePES
} eTrackTypeEplayer;

typedef struct Track_s {
    std::string Name;
    const char *Encoding;
    int Id;

    /* new field for ffmpeg - add at the end so no problem
     * can occur with not changed srt saa container
     */
    char *language;

    /* length of track */
    int64_t duration;
//CHECK    int64_t pts;

    /* context from ffmpeg */
    AVFormatContext *avfc;
    /* stream from ffmpeg */
    AVStream *stream;

    int pending;
    int is_static;
    long long int chapter_start;
    long long int chapter_end;
    Track_s() : Encoding(NULL), Id(0), language(NULL), duration(-1), avfc(NULL), stream(NULL), pending(0), is_static(0), chapter_start(0), chapter_end(0) {}
} Track_t;

struct Context_s;
typedef struct Context_s Context_t;

typedef struct Manager_s {
    const char *Name;
    int (*Command) ( Context_t *, ManagerCmd_t, void *);
    const char **Capabilities;

} Manager_t;

typedef struct ManagerHandler_s {
    const char *Name;
    Manager_t *audio;
    Manager_t *video;
    Manager_t *subtitle;
    Manager_t *teletext;
    Manager_t *chapter;
} ManagerHandler_t;

void freeTrack(Track_t * track);
void copyTrack(Track_t * to, Track_t * from);

#endif
