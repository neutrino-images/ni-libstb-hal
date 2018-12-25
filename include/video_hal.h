#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/video_td.h"
#elif HAVE_DUCKBOX_HARDWARE
#include "../libduckbox/video_lib.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/video_lib.h"
#elif HAVE_ARM_HARDWARE
#include "../libarmbox/video_lib.h"
#include "../libarmbox/hdmi_cec.h"
#elif HAVE_AZBOX_HARDWARE
#include "../libazbox/video_lib.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../libraspi/video_lib.h"
#else
#include "../libgeneric-pc/video_lib.h"
#endif
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif

#if STB_HAL_VIDEO_HAS_GETSCREENIMAGE
#define SCREENSHOT 1
#endif
