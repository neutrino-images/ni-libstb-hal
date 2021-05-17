#ifndef misc_123
#define misc_123

/* some useful things needed by many files ... */

#include <stdint.h>

#define INVALID_PTS_VALUE   0x200000000ll

struct BitPacker_t
{
	uint8_t *Ptr;       /* write pointer */
	unsigned int BitBuffer; /* bitreader shifter */
	int Remaining;      /* number of remaining in the shifter */
};

void PutBits(BitPacker_t *ld, unsigned int code, unsigned int length);
void FlushBits(BitPacker_t *ld);

#endif
