#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/dmx_td.h"
#elif HAVE_DUCKBOX_HARDWARE
#include "../libduckbox/dmx_lib.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/dmx_lib.h"
#elif HAVE_AZBOX_HARDWARE
#include "../azbox/dmx_lib.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../raspi/dmx_lib.h"
#else
#include "../generic-pc/dmx_lib.h"
#endif
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
