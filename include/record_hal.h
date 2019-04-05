#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/record_td.h"
#elif HAVE_DUCKBOX_HARDWARE
#include "../libduckbox/record_lib.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/record_lib.h"
#elif HAVE_ARM_HARDWARE
#include "../libarmbox/record_lib.h"
#elif HAVE_MIPS_HARDWARE
#include "../libmipsbox/record_lib.h"
#elif HAVE_AZBOX_HARDWARE
#include "../libazbox/record_lib.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../libraspi/record_lib.h"
#else
#include "../libgeneric-pc/record_lib.h"
#endif
#else
#error no valid hardware defined
#endif
