#ifndef bufferTool_h
#define bufferTool_h

#include "bufferId.h"
#include "bufferToolDisk.h"
#include "bufIdAddOrderSet.h"

#include <map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>

extern size_t bufferSize;

enum BufStatus { BUF_ONDISK, BUF_OFFDISK, BUF_DISKONLY, BUF_PROCESSING };
struct BufferInfo {
    size_t compressedLen;  // the compressed length
    uint64_t positions;  // total number of positions stored in this buffer
    uint8_t *memLoc;  // where this is in memory or NULL if only on disk
    int accessors;  // number of threads currently reading this segment
    BufStatus status; // if this is accessed with a status of BUF_PROCESSING, wait until settled
    off_t offset;   // offset in the file if saved to disk
    
    BufferInfo(size_t cLen, uint64_t pos) {
	compressedLen = cLen;
	positions = pos;
	memLoc = NULL;
	accessors = 0;
	status = BUF_PROCESSING;
    }	
};

enum SeqStatus { SEQ_WORKING, SEQ_FINISHED, SEQ_MERGING, SEQ_DELETE_QUEUED, SEQ_DELETED };

struct BufferSequence {
    // we regularly wish to hold on to BufferInfos for longer than we'd like to hold on
    // to the bufDirMutex lock so we've changed this to a vector of pointers.
    std::vector<BufferInfo *> bufferList;
    uint64_t positions;
    SeqStatus status;
    char fullStep;

    BufferSequence(int fullStep_=0) { positions = 0; status = SEQ_WORKING; fullStep = fullStep_;}
};

struct MergeLevel {
    std::vector<BufferSequence> sequenceList;
};

struct StepBuffer {
    std::vector<MergeLevel> levelList;
};


// filled in by BufferManager::getBuffer()
struct BufferStat {
    BufStatus status;
    uint8_t *data;
    size_t len;
    uint64_t positions;
    uint32_t buffersRemaining;
};
    
    
struct BGroupStat {
    size_t bufCount;
    uint64_t positions;
    SeqStatus status;
    char fullStep;  // is this the final buffer group
};
    

class BufferManager {
    // stepList[0] is advancing forward and [1] is going back
    std::vector<StepBuffer> stepList[2];
    size_t bufMem;
    SimpleDiskBuffer sdb;
    
    // bufDirMutex needs to be held when manipulating structure of buffer directory.
    // but can be released once you have your leaf node.  If you are going to be
    // making changes to it, it should be marked as in progress before releasing the
    // mutex (and the mutex should be reclaimed before changing the status back)
    std::mutex bufDirMutex;
    // bufDirCondition should be triggered any time a buffer switches from BUF_PROCESSING to
    // any other "stable" status.
    std::condition_variable bufDirCondition;

    // waits for buffer status to be something other than BUF_PROCESSING (using
    // bufDirCondition).  Returns 1 if the lock had to be released or 0 otherwise
    int waitForSettledStatus(BufferInfo &b, std::unique_lock<std::mutex> &lock);

    // sets buffer status.  if setting status to something other than BUF_PROCESSING
    // trigger bufDirCondition.
    void setBufStatus(BufferInfo &b, BufStatus s);
    
    // memMutex should be held when looking at or updating availMem.  Use the following
    // accessors
    std::mutex memMutex;
    // this condition gets triggered when memory is freed.  We wait on this condition
    // if there is insufficient memory available
    std::condition_variable memCondition;
    // this condition is triggered when memory is used.  The caching threads wait on this
    // condition.
    std::condition_variable memUsedCondition;
    
    // where we keep track of the available memory
    size_t availMem;

    // this is the target amount of memory available.  When availMem goes below targetMem
    // the caching threads will try to do something about this.
    size_t targetMem;

    // atomically claim <bytes> of memory from availMem.  If there isn't enough memory available
    // useMem will wait until there is enough.
    void useMem(size_t bytes);

    // atomically claim <bytes> of memory from availMem (and return 1) or return 0
    // if there isn't enough available.
    int useMemNoWait(size_t bytes);
    
    // atomically release <bytes> of memory back to availmem.  Always succeeds.
    void freeMem(size_t bytes);

    // show how much memory is available.  Use for statistics or hints as to how much
    // memory needs to be freed to save some new buffer.
    size_t memAvail();

    // returns how much memory is in use beyond the memory target or 0 if the memory used
    // is less than or equal to the target.
    size_t targetMemDeficiency();

    // bIdSet is used for finding buffers that are available to be pushed to disk.
    // these should provide the oldest buffers that have not been deleted or written
    // to disk.
    BufIdSet bIdSet; 

    // the threads that write buffers to disk
    std::vector<std::thread> cacheThreads;

    bool stopCacheThreadsFlag;
    
public:
    /* 
       bufferMem is the amount of ram that can be treated as a ram disk for storing
       completed buffers.  It is assumed that there is enough ram beyond that for
       each thread to have the active buffers it needs loaded into ram.
    */ 
    BufferManager(size_t bufferMem=0) {
	bufMem = bufferMem;
	availMem = bufferMem;
	sdb.setup("js2cache");
    }

    

    // set the available buffer memory
    void setMem(size_t bufferMem) {
	// if this is setting the buffer memory lower than it was previously
	// it can take available memory negative.
	size_t diff = bufferMem - bufMem;
	bufMem += diff;
	freeMem(diff);
    }

    // for creating a buffer group

    // start a new buffer group and get the buffer ID
    int appendBufferGroup(BufferId *bId);

    // or start a new buffer group by specifying the buffer ID
    int startBufferGroup(BufferId bId, int fullStep=0);

    int insertBuffer(BufferId bId, uint8_t *data, size_t len, uint64_t postions);
    void setGroupFinished(BufferId bId);
    
    // setting statuses  (setGroupFinished() could go here too)
    void setGroupStatus(BufferId bId, SeqStatus s);
    void setGroupFullStep(BufferId bId);  // set that this group is the final merge group of the step after all dup removal
    

    // getting info
    
    // get info on a buffer
    int getBufferStat(BufferId bId, BufferStat *bStat);

    // get info on a buffer group
    int groupFinished(BufferId bId);
    int getGroupStat(BufferId bId, BGroupStat *bgStat);

    // get info on a merge level
    uint32_t groupCountInLevel(BufferId bId);

    // get number of merge levels in a step
    uint16_t levelCountInStep(BufferId bId);

    // get the BufferId for the final buffer group of a step
    BufferId finalBufId(BufferId);
    BufferId finalBufId(uint8_t stepGroup, uint32_t step);

    
    // getting a buffer  (you have to return it when you're done)
    int getBuffer(BufferId bId, BufferStat *bStat);
    int returnBuffer(BufferId bId);

    // deleting a buffer group
    void deleteBufferGroup(BufferId bId);

    // set up and clean up on disk buffers.
    int setupDiskBuffer(char *path);
    int cleanupDiskBuffer();

    // get a buffer that is available to be written to disk.
    // The buffer will be set as PROCESSING before being returned
    //
    // returns 1 if bId and bStat are valid or 0 if no buffers are available to be written.
    //
    // After the buffer has been cached call markWritten() so that the status is updated.
    int getBufferToCache(BufferId *bId, BufferStat *bStat);
    void markWritten(BufferId bId);
    
    // try and cache a buffer to disk.
    int writeABuffer();

    // caching control threads.  Only call these from the main thread (ie they're not thread safe!)
    
    // start a disk caching thread 
    int startCachingThread();

    // this is what gets run by startCachingThread()
    void cachingThreadRun();

    // stop all caching threads and wait for them to finish
    void stopAllCachingThreads();

    // set the target amount of buffers in memory.  If there is more memory in use,
    // the caching thread(s) will try to push buffers that are not currently in use
    // to disk.  If all of the worker threads are stopped, setting this to 0 should
    // encourage the caching thread(s) to cache all extant buffers.
    void setTargetMemAvail(size_t mem);

    // wait for the caching threads to reach their target.
    void waitForCachingTarget();

    // wait for any of the events the caching thread needs to worry about
    // returns 1 if buffers need to be written or 0 if the caching thread
    // needs to quit
    int waitForCachingNeededOrStopEvent();

private:
    BufferSequence & traverseToBufferSeq(BufferId bId);
    void fillInBufferStat(BufferId &bId, BufferSequence &seq, BufferInfo &buf, BufferStat *bStat);
};


#endif
