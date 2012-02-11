#include <config.h>
#if HAVE_TRIPLEDRAGON
#include "../libtriple/ca_cs.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/ca_cs.h"
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif
