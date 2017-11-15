#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/audio_td.h"
#elif HAVE_DUCKBOX_HARDWARE
#include "../libduckbox/audio_lib.h"
#include "../libduckbox/audio_mixer.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/audio_lib.h"
#include "../libspark/audio_mixer.h"
#elif HAVE_ARM_HARDWARE
#include "../libarmbox/audio_lib.h"
#elif HAVE_AZBOX_HARDWARE
#include "../azbox/audio_lib.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../raspi/audio_lib.h"
#else
#include "../generic-pc/audio_lib.h"
#endif
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
