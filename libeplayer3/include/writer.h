#ifndef WRITER_H_
#define WRITER_H_

#include <stdio.h>
#include <stdint.h>

typedef enum { eNone, eAudio, eVideo, eGfx } eWriterType_t;

typedef struct {
    int fd;
    uint8_t *data;
    unsigned int len;
    uint64_t Pts;
    uint8_t *private_data;
    unsigned int private_size;
    unsigned int FrameRate;
    unsigned int FrameScale;
    unsigned int Width;
    unsigned int Height;
    unsigned char Version;
} WriterAVCallData_t;

typedef struct WriterCaps_s {
    char *name;
    eWriterType_t type;
    char *textEncoding;
    /* fixme: revise if this is an enum! */
    int dvbEncoding;
} WriterCaps_t;

typedef struct Writer_s {
    int (*reset) ();
    int (*writeData) (WriterAVCallData_t *);
    int (*writeReverseData) (WriterAVCallData_t *);
    WriterCaps_t *caps;
} Writer_t;

extern Writer_t WriterAudioIPCM;
extern Writer_t WriterAudioPCM;
extern Writer_t WriterAudioMP3;
extern Writer_t WriterAudioMPEGL3;
extern Writer_t WriterAudioAC3;
extern Writer_t WriterAudioEAC3;
extern Writer_t WriterAudioAAC;
extern Writer_t WriterAudioDTS;
extern Writer_t WriterAudioWMA;
extern Writer_t WriterAudioFLAC;
extern Writer_t WriterAudioVORBIS;

extern Writer_t WriterVideoMPEG2;
extern Writer_t WriterVideoMPEGH264;
extern Writer_t WriterVideoH264;
extern Writer_t WriterVideoWMV;
extern Writer_t WriterVideoDIVX;
extern Writer_t WriterVideoFOURCC;
extern Writer_t WriterVideoMSCOMP;
extern Writer_t WriterVideoH263;
extern Writer_t WriterVideoFLV;
extern Writer_t WriterVideoVC1;

Writer_t *getWriter(char *encoding);

Writer_t *getDefaultVideoWriter();
Writer_t *getDefaultAudioWriter();

#endif
