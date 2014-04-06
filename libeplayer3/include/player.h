#ifndef COMMON_H_
#define COMMON_H_

#include "container.h"
#include "output.h"
#include "manager.h"
#include "playback.h"
#include <pthread.h>
#include <stdint.h>

class Player {
	public: //FIXME
	    PlaybackHandler_t *playback;
	    ContainerHandler_t *container;
	    int64_t *currentAudioPtsP;
	public:
	    Player();
	    ~Player();

	    Output output;
	    Manager manager;
};

int container_ffmpeg_update_tracks(Player * context, const char *filename);
#endif
