#ifndef bufferTool_h
#define bufferTool_h

//#include <gdbm.h>
#include <map>
#include <vector>
#include <mutex>

extern size_t bufferSize;

struct BufferId {
    // used to uniquely ID any buffer and as the keys for the gdbm file
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

    int isNull() {
	return nullId != 0;
    }
    void print();
};


enum BufStatus { BUF_ONDISK, BUF_OFFDISK, BUF_DISKONLY, BUF_PROCESSING };
struct BufferInfo {
    size_t compressedLen;  // the compressed length
    uint64_t positions;  // total number of positions stored in this buffer
    uint8_t *memLoc;  // where this is in memory or NULL if only on disk
    int accessors;  // number of threads currently reading this segment
    BufStatus status; // if this is accessed with a status of BUF_PROCESSING, wait until settled
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
    std::vector<BufferInfo> bufferList;
    uint64_t positions;
    SeqStatus status;

    BufferSequence() { positions = 0; status = SEQ_WORKING; }
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
};
    

class BufferManager {
    // stepList[0] is advancing forward and [1] is going back
    std::vector<StepBuffer> stepList[2];
    size_t bufMem;
    
    // bufDirMutex needs to be held when manipulating structure of buffer directory.
    // but can be released once you have your leaf node.  If you are going to be
    // making changes to it, it should be marked as in progress before releasing the
    // mutex (and the mutex should be reclaimed before changing the status back)
    std::mutex bufDirMutex;

    // memMutex should be held when looking at or updating availMem.  Use the following
    // accessors
    std::mutex memMutex;
    size_t availMem;

    // atomically claim <bytes> of memory from availMem (and return 1) or return 0
    // if there isn't enough available.
    int useMem(size_t bytes);
    
    // atomically release <bytes> of memory back to availmem.  Always succeeds.
    void freeMem(size_t bytes);

    // show how much memory is available.  Use for statistics or hints as to how much
    // memory needs to be freed to save some new buffer.
    size_t memAvail();

    // if/when we start using gdbm as our backing store, this will need held for
    // any thread that reads or writes to gdbm.  (we can get rid of this if we
    // change to using a separate file for each buffer group
    std::mutex gdbmMutex;
    
public:
    /* 
       bufferMem is the amount of ram that can be treated as a ram disk for storing
       completed buffers.  It is assumed that there is enough ram beyond that for
       each thread to have the active buffers it needs loaded into ram.
    */ 
    BufferManager(size_t bufferMem=0) {
	bufMem = bufferMem;
	availMem = bufferMem;
    }

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
    int startBufferGroup(BufferId bId);

    int insertBuffer(BufferId bId, uint8_t *data, size_t len, uint64_t postions);
    void setGroupFinished(BufferId bId);
    
    // setting statuses  (setGroupFinished() could go here too)
    void setGroupStatus(BufferId bId, SeqStatus s);
    

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
    

private:
    BufferSequence & traverseToBufferSeq(BufferId bId);
    void fillInBufferStat(BufferId &bId, BufferSequence &seq, BufferInfo &buf, BufferStat *bStat);
};


#endif
