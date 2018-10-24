#ifndef bitset_h
#define bitset_h

// stolen and adapted from http://c-faq.com/misc/bitsets.html


#include <limits.h>		/* for CHAR_BIT */
#include <string.h>             /* for memset */

#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)
#define BITRESETALL(a, nb) memset(a, 0, BITNSLOTS(nb))






#endif
