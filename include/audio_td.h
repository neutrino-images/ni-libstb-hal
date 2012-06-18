#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/audio_td.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/audio_lib.h"
#elif HAVE_AZBOX_HARDWARE
#include "../azbox/audio_lib.h"
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
