/* compatibility header for tripledragon. I'm lazy, so I just left it
   as "cs_api.h" so that I don't need too many ifdefs in the code */

#ifndef __CS_API_H__
#define __CS_API_H__

#include "init.h"
#include <config.h>

typedef void (*cs_messenger) (unsigned int msg, unsigned int data);

inline void cs_api_init()
{
	hal_api_init();
};

inline void cs_api_exit()
{
	hal_api_exit();
};

#define cs_malloc_uncached	malloc
#define cs_free_uncached	free

// Callback function helpers
#if HAVE_DUCKBOX_HARDWARE \
 || HAVE_MIPS_HARDWARE \
 || (HAVE_ARM_HARDWARE \
     && !BOXMODEL_HD60 \
     && !BOXMODEL_OSMIO4K \
     && !BOXMODEL_OSMIO4KPLUS \
    )
void cs_register_messenger(cs_messenger messenger);
#else
static inline void cs_register_messenger(cs_messenger) { return; };
#endif
static inline void cs_deregister_messenger(void) { return; };

/* compat... HD1 seems to be version 6. everything newer ist > 6... */
static inline unsigned int cs_get_revision(void) { return 1; };
static inline unsigned int cs_get_chip_type(void) { return 0; };
extern int cnxt_debug;

#endif // __CS_API_H__
