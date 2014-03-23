#ifndef misc_123
#define misc_123

#include <dirent.h>

/* some useful things needed by many files ... */

/* ***************************** */
/* Types                         */
/* ***************************** */

typedef struct BitPacker_s {
    unsigned char *Ptr;		/* write pointer */
    unsigned int BitBuffer;	/* bitreader shifter */
    int Remaining;		/* number of remaining in the shifter */
} BitPacker_t;

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define INVALID_PTS_VALUE                       0x200000000ull

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

void PutBits(BitPacker_t * ld, unsigned int code, unsigned int length);
void FlushBits(BitPacker_t * ld);

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static inline char *getExtension(char *name)
{
    if (name) {
	char *ext = strrchr(name, '.');
	if (ext)
	    return ext + 1;
    }
    return NULL;
}
#endif
