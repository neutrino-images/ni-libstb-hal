#include <config.h>
#if HAVE_ARM_HARDWARE
#include "../libarmbox/audio_lib.h"
#elif HAVE_MIPS_HARDWARE
#include "../libmipsbox/audio_lib.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../libraspi/audio_lib.h"
#else
#include "../libgeneric-pc/audio_lib.h"
#endif
#else
#error no valid hardware defined
#endif
