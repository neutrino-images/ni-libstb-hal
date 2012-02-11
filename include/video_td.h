#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/video_td.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/video_lib.h"
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
