#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/playback_td.h"
#elif HAVE_DUCKBOX_HARDWARE
#include "../libduckbox/playback_libeplayer3.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/playback_libeplayer3.h"
#elif HAVE_ARM_HARDWARE
#if ENABLE_GSTREAMER_10
#include "../libarmbox/playback_gst.h"
#else
#if BOXMODEL_HD60
#include "../libarmbox/playback_hisilicon.h"
#else
#include "../libarmbox/playback_libeplayer3.h"
#endif
#endif
#elif HAVE_AZBOX_HARDWARE
#include "../libazbox/playback_lib.h"
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
