#ifndef COMMON_H_
#define COMMON_H_

#include "container.h"
#include "output.h"
#include "manager.h"
#include "playback.h"
#include <pthread.h>
#include <stdint.h>

struct Player {
    PlaybackHandler_t *playback;
    ContainerHandler_t *container;
    OutputHandler_t *output;
    ManagerHandler_t *manager;
    int64_t *currentAudioPtsP;
};

int container_ffmpeg_update_tracks(Player * context, const char *filename);
#endif
