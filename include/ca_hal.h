#if HAVE_MIPS_HARDWARE \
 || (HAVE_ARM_HARDWARE \
     && !BOXMODEL_HD60 \
     && !BOXMODEL_MULTIBOX \
     && !BOXMODEL_MULTIBOXSE \
     && !BOXMODEL_OSMIO4K \
     && !BOXMODEL_OSMIO4KPLUS \
    )
#include "ca_ci.h"
#else
#include "ca.h"
#endif
