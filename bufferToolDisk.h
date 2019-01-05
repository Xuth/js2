#ifndef bufferToolDisk_h
#define bufferToolDisk_h

#include "bufferId.h"

#include <stdint.h>
#include <stddef.h>


// this interface will need to be fleshed out further to support checkpointing and resuming

class SimpleDiskBuffer {
    char *path;
    int nameAlloc;  // size of memory to alloc when constructing a path

public:

    // initial setup of this class and of any disk structure.
    int setup(const char *_path);

    // we may not wish to clear a disk buffer if we are saving an intermediate step
    void clear();

    // read into memory a buffer that has been previously saved to disk
    uint8_t *fetchBuffer(BufferId bId, size_t len);
    
    // when done with a buffer that has been loaded off disk, clean up and (possibly) free the memory
    void releaseBuffer(uint8_t *buf, size_t len);

    // save a buffer to disk
    void writeBuffer(BufferId bId, uint8_t *buf, size_t len);

    // delete a buffer from disk
    void deleteBuffer(BufferId bId);
};

#endif
