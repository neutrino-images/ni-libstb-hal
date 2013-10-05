#include <config.h>
#if HAVE_GENERIC_HARDWARE
#if BOXMODEL_RASPI
#include "../raspi/glfb.h"
#else
#include "../generic-pc/glfb.h"
#endif
#else
#error glfb.h only works with HAVE_GENERIC_HARDWARE defined
#endif
