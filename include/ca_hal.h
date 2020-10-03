#if HAVE_DUCKBOX_HARDWARE \
 || HAVE_MIPS_HARDWARE \
 || (HAVE_ARM_HARDWARE \
     && !BOXMODEL_HISILICON \
     && !BOXMODEL_OSMIO4K \
     && !BOXMODEL_OSMIO4KPLUS \
    )
#include "ca_ci.h"
#else
#include "ca.h"
#endif
