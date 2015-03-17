#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/record_td.h"
#elif HAVE_DUCKBOX_HARDWARE
#include "../libduckbox/record_lib.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/record_lib.h"
#elif HAVE_AZBOX_HARDWARE
#include "../azbox/record_lib.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../raspi/record_lib.h"
#else
#include "../generic-pc/record_lib.h"
#endif
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
