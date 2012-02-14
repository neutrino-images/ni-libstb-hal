#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/playback_td.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/playback_libeplayer3.h"
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
