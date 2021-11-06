#include <config.h>
#if HAVE_ARM_HARDWARE
#include "../libarmbox/playback_libeplayer3.h"
#elif HAVE_MIPS_HARDWARE
#include "../libmipsbox/playback_libeplayer3.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../libraspi/playback_lib.h"
#else
#if ENABLE_GSTREAMER
#include "../libgeneric-pc/playback_gst.h"
#else
#include "../libgeneric-pc/playback_lib.h"
#endif
#endif
#else
#error no valid hardware defined
#endif
