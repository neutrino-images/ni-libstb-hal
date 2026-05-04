#if HAVE_MIPS_HARDWARE \
 || (HAVE_ARM_HARDWARE \
     && !BOXMODEL_HD60 \
     && !BOXMODEL_MULTIBOX \
     && !BOXMODEL_MULTIBOXSE \
    )
#include "ca_ci.h"
#else
#include "ca.h"
#endif
