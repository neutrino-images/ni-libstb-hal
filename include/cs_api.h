#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/cs_api.h"
#elif HAVE_DUCKBOX_HARDWARE
#include "../libduckbox/cs_api.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/cs_api.h"
#elif HAVE_ARM_HARDWARE
#include "../libarmbox/cs_api.h"
#elif HAVE_AZBOX_HARDWARE
#include "../azbox/cs_api.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../raspi/cs_api.h"
#else
#include "../generic-pc/cs_api.h"
#endif
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
