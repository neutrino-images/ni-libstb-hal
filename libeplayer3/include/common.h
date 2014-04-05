#ifndef COMMON_H_
#define COMMON_H_

#include "container.h"
#include "output.h"
#include "manager.h"
#include "playback.h"
#include <pthread.h>
#include <stdint.h>

typedef struct Context_s {
    PlaybackHandler_t *playback;
    ContainerHandler_t *container;
    OutputHandler_t *output;
    ManagerHandler_t *manager;
    int64_t *currentAudioPtsP;
} Context_t;

int container_ffmpeg_update_tracks(Context_t * context, char *filename);
#endif
