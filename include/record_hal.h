#include <config.h>
#if HAVE_ARM_HARDWARE
#include "../libarmbox/record_lib.h"
#elif HAVE_MIPS_HARDWARE
#include "../libmipsbox/record_lib.h"
#elif HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../libraspi/record_lib.h"
#else
#include "../libgeneric-pc/record_lib.h"
#endif
#else
#error no valid hardware defined
#endif
