#include "bufferId.h"

#include <functional>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

size_t hash(BufferId bId) {
    uint64_t *bPtr = (uint64_t *)&bId;
    
    std::hash<uint64_t> hashFn;

    return hashFn(bPtr[0]) ^ hashFn(bPtr[1]);
}


bool equal(BufferId b1, BufferId b2) {
    return 0 == memcmp(&b1, &b2, sizeof(BufferId));
}

void BufferId::print() {
    if (isNull()) {
	printf("<BufferId NULL>");
	return;
    }

    printf("<BufferId %" PRIu8 ":%" PRIu32 ":%" PRIu16 ":%" PRIu32 ":%" PRIu32 ">",
	   stepGroup, step, mergeLevel, group, buf);
}

