/* compatibility header for tripledragon. I'm lazy, so I just left it
   as "cs_api.h" so that I don't need too many ifdefs in the code */

#ifndef __CS_API_H_
#define __CS_API_H_

#include "init_lib.h"
typedef void (*cs_messenger) (unsigned int msg, unsigned int data);

inline void cs_api_init()
{
	init_td_api();
};

inline void cs_api_exit()
{
	shutdown_td_api();
};

#define cs_malloc_uncached	malloc
#define cs_free_uncached	free

// Callback function helpers
void cs_register_messenger(cs_messenger messenger);
static inline void cs_deregister_messenger(void) { return; };

/* compat... HD1 seems to be version 6. everything newer ist > 6... */
static inline unsigned int cs_get_revision(void) { return 1; };
static inline unsigned int cs_get_chip_type(void) { return 0; };
extern int cnxt_debug;
#endif //__CS_API_H_
