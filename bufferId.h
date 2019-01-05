#ifndef bufferId_h
#define bufferId_h

#include <stdint.h>
#include <functional>
#include <string.h>

struct BufferId {
    // used to uniquely ID any buffer and as the keys for a dbm file should I try using such a thing
    // these are instantiated out of logical order for packing efficiency
    //
    // logical hierarchy:
    // stepGroup:    is this a step from beginnning or end for concurrent puzzle solves
    // step:         which puzzle step are we on
    // mergeLevel:   which level of buffer merging are we on
    // group:        a sequence of buffers that can logically be treated as a single buffer
    // buf:          the individual buffer that is constrained by physical memory size
    //
    // nullId:       flag to say that this bufferId is NULL and just a placeholder 
    
    uint32_t step;       // which puzzle step
    uint32_t group;      // which group of buffers within the merge level
    uint32_t buf;        // which buffer within the group
    uint16_t mergeLevel; // 
    uint8_t stepGroup;   // which step group (forward or backward)
    uint8_t nullId;      // if set to 1 this bufferID is just a placeholder


    BufferId(uint8_t stepGroup_=0,
	     uint32_t step_=0,
	     uint16_t mergeLevel_=0,
	     uint32_t group_=0,
	     uint32_t buf_=0,
	     uint8_t nullId_=0) {
	
	stepGroup = stepGroup_;
	step = step_;
	mergeLevel = mergeLevel_;
	group = group_;
	buf = buf_;
	nullId = nullId_;
    }
    
    BufferId(const char *) {  // allows you to declare BufferId(NULL);
	nullId = 1;
    }

    int isNull() {
	return nullId != 0;
    }
    void print();

    bool operator==(const BufferId &b) const {
	return 0 == memcmp(this, &b, sizeof(BufferId));
    }
};

class BufferIdHash {
public:
    size_t operator()(const BufferId &bId) const {
	uint64_t *bPtr = (uint64_t *)&bId;
	std::hash<uint64_t> hashFn;
	return hashFn(bPtr[0]) ^ hashFn(bPtr[1]);
    }
};

class BufferIdEqual {
public:
    bool operator()(const BufferId &b1, const BufferId &b2) const {
	return 0 == memcmp(&b1, &b2, sizeof(BufferId));
    }
};

#endif
