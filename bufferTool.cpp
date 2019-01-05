#include "bufferTool.h"
#include "bufferToolDisk.h"
#include "js2.h"

#include <assert.h>
#include <string.h>

//#define DEBUGPRINT

void BufferManager::useMem(size_t bytes) {
    std::unique_lock<std::mutex> lock(memMutex);
    while(1) {
	if (bytes <= availMem) {
	    availMem -= bytes;
	    memUsedCondition.notify_all();
	    return;
	}
	memCondition.wait(lock);
    }
}

int BufferManager::useMemNoWait(size_t bytes) {
    std::lock_guard<std::mutex> lock(memMutex);
    if (bytes > availMem)
	return 0;
    availMem -= bytes;
    return 1;
}

void BufferManager::freeMem(size_t bytes) {
    std::lock_guard<std::mutex> lock(memMutex);
    availMem += bytes;
    memCondition.notify_all();
}

size_t BufferManager::memAvail() {
    std::lock_guard<std::mutex> lock(memMutex);
    return availMem;
}

void BufferManager::setTargetMemAvail(size_t bytes) {
    std::lock_guard<std::mutex> lock(memMutex);
    targetMem = bytes;
    memUsedCondition.notify_all();
}

size_t BufferManager::targetMemDeficiency() {
    std::lock_guard<std::mutex> lock(memMutex);
    if (availMem >= targetMem)
	return 0;
    return targetMem - availMem;
}
    


int BufferManager::waitForSettledStatus(BufferInfo &b, std::unique_lock<std::mutex> &lock) {
    // while we're waiting on a settled buffer status, the bufferInfo structure might
    // get moved out from under us so we need to
    int ret = 0;
    while(1) {
	if (b.status != BUF_PROCESSING)
	    return ret;
	ret = 1;
	bufDirCondition.wait(lock);
    }
}

void BufferManager::setBufStatus(BufferInfo &b, BufStatus s) {
    b.status = s;
    if (s != BUF_PROCESSING)
	bufDirCondition.notify_all();
}

int BufferManager::startBufferGroup(BufferId bId, int fullStep) {
    std::lock_guard<std::mutex> lock(bufDirMutex);
    
    std::vector<StepBuffer> &sl = stepList[bId.stepGroup];
    if (bId.step+1 > sl.size()) {
	assert(bId.step == sl.size());
	sl.emplace_back();
    }
    StepBuffer &step = sl[bId.step];
    
    if (bId.mergeLevel+1 > (int)step.levelList.size()) {
	assert(bId.mergeLevel == step.levelList.size());
	step.levelList.emplace_back();
    }
    MergeLevel &level = step.levelList[bId.mergeLevel];
    
    assert(bId.group == level.sequenceList.size());
    level.sequenceList.emplace_back(fullStep);

    return 1;
}
    
int BufferManager::appendBufferGroup(BufferId *bId) {
    std::lock_guard<std::mutex> lock(bufDirMutex);
    
    std::vector<StepBuffer> &sl = stepList[bId->stepGroup];
    if (bId->step+1 > sl.size()) {
	assert(bId->step == sl.size());
	sl.emplace_back();
    }
    StepBuffer &step = sl[bId->step];
    
    if (bId->mergeLevel+1 > (int)step.levelList.size()) {
	assert(bId->mergeLevel == step.levelList.size());
	step.levelList.emplace_back();
    }
    MergeLevel &level = step.levelList[bId->mergeLevel];
    
    bId->group = level.sequenceList.size();
    bId->buf = 0;
    level.sequenceList.emplace_back();

    return 1;
}
    
BufferSequence & BufferManager::traverseToBufferSeq(BufferId bId) {
    std::vector<StepBuffer> &sl = stepList[bId.stepGroup];
    StepBuffer &step = sl[bId.step];
    MergeLevel &level = step.levelList[bId.mergeLevel];
    BufferSequence &seq = level.sequenceList[bId.group];
    return seq;
}

uint32_t BufferManager::groupCountInLevel(BufferId bId) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    if (bId.stepGroup >= 2)
	return 0;
    std::vector<StepBuffer> &sl = stepList[bId.stepGroup];
    if (sl.size() <= bId.step)
	return 0;
    StepBuffer &step = sl[bId.step];
    if (step.levelList.size() <= bId.mergeLevel)
	return 0;
    MergeLevel &level = step.levelList[bId.mergeLevel];
    return level.sequenceList.size();
}

BufferId BufferManager::finalBufId(BufferId bId) {
    return finalBufId(bId.stepGroup, bId.step);
}

BufferId BufferManager::finalBufId(uint8_t stepGroup, uint32_t step) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    std::vector<StepBuffer> &sl = stepList[stepGroup];
    StepBuffer &stepBuf = sl[step];

    //BufferId ret = BufferId(stepGroup, step, stepBuf.levelList.size() - 1, 0, 0);
    //printf("finalBufId: ");
    //ret.print();
    //printf("\n");
    
    return BufferId(stepGroup, step, stepBuf.levelList.size() - 1, 0, 0);
}
    
uint16_t BufferManager::levelCountInStep(BufferId bId) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    if (bId.stepGroup >= 2)
	return 0;
    std::vector<StepBuffer> &sl = stepList[bId.stepGroup];
    if (sl.size() <= bId.step)
	return 0;
    StepBuffer &step = sl[bId.step];
    return step.levelList.size();
}
    
int BufferManager::insertBuffer(BufferId bId,
				uint8_t *data, size_t len,
				uint64_t positions) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    assert(bId.buf == seq.bufferList.size());
    BufferInfo *bufPtr = new BufferInfo(len, positions);
    seq.bufferList.emplace_back(bufPtr);
    seq.positions += positions;
    BufferInfo &buf = *bufPtr;
    uLock.unlock();

    useMem(len);
    buf.memLoc = (uint8_t *)malloc(len);
    test(buf.memLoc != NULL, "can't malloc in insertBuffer()");
    memcpy(buf.memLoc, data, len);
    
    uLock.lock();
    buf.status = BUF_OFFDISK;
    uLock.unlock();

    bIdSet.add(bId);
	
    return 1;
}

void BufferManager::fillInBufferStat(BufferId &bId, BufferSequence &seq, BufferInfo &buf, BufferStat *bStat) {
    bStat->data = buf.memLoc;
    bStat->len = buf.compressedLen;
    bStat->positions = buf.positions;
    uint64_t s = seq.bufferList.size();
//    bStat->buffersRemaining = seq.bufferList.size() - bId.buf - 1;
    bStat->buffersRemaining = s - bId.buf - 1;
    test(bStat->buffersRemaining < 20000, "buffersRemaining count is absurd");
    bStat->status = buf.status;
}

int BufferManager::getBuffer(BufferId bId, BufferStat *bStat) {
    #ifdef DEBUGPRINT
    printf("calling getBuffer ");
    bId.print();
    printf(", memAvail: %lu\n", memAvail());
    #endif
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    test(seq.bufferList.size() > bId.buf, "Trying to access buffer beyond length of buffer group");
    BufferInfo &buf = *seq.bufferList[bId.buf];
    int unlocked = waitForSettledStatus(buf, uLock);
    if (buf.memLoc != NULL) {
	if (unlocked)
	    seq = traverseToBufferSeq(bId);
	fillInBufferStat(bId, seq, buf, bStat);
	buf.accessors++;
	return 1;
    }
    setBufStatus(buf, BUF_PROCESSING);
    uLock.unlock();

    useMem(buf.compressedLen);
    buf.memLoc = sdb.fetchBuffer(bId, buf.compressedLen);
    test(buf.memLoc != NULL, "failed to fetch buffer from disk");
    buf.accessors++;
    uLock.lock();
    fillInBufferStat(bId, traverseToBufferSeq(bId), buf, bStat);
    setBufStatus(buf, BUF_ONDISK);
    return 1;
}

int BufferManager::getBufferToCache(BufferId *retBId, BufferStat *bStat) {
    BufferId firstBId("None");
    while(1) {
	std::unique_lock<std::mutex> uLock(bufDirMutex);
	BufferId bId = bIdSet.pop();

	// do some bookkeeping to make sure that we have a valid bId and that
	// we haven't gone full circle around the bufferIds.  The current check 
	// for looping isn't sufficient if their are multiple writers.
	if (bId.isNull())
	    return 0;
	if (firstBId.isNull())
	    firstBId = bId;
	else if (firstBId == bId) {
	    bIdSet.add(bId);
	    return 0;
	}

        #ifdef DEBUGPRINT
	printf("got buffer ");
	bId.print();
	printf(" to cache\n");
	#endif
	
	BufferSequence &seq = traverseToBufferSeq(bId);
	// the following sanity check can fail in a race condition
	//test(seq.status != SEQ_DELETED, "invalid seq status (DELETED) getting buffer to cache");
	BufferInfo &buf = *seq.bufferList[bId.buf];
	test(buf.status != BUF_ONDISK, "invalid buffer status (ONDISK) getting buffer to cache");
	test(buf.status != BUF_DISKONLY, "invalid buffer status (DISKONLY) getting buffer to cache");

	// check if the buffer is currently in use
	if ((buf.status == BUF_PROCESSING) ||
	    (buf.accessors != 0)) {

            #ifdef DEBUGPRINT
	    printf("buffer ");
	    bId.print();
	    printf(" in use... putting back.\n");
	    #endif
	    bIdSet.add(bId);
	    uLock.unlock();
	    continue;
	}

	#ifdef DEBUGPRINT
	printf("actually caching buffer ");
	bId.print();
	printf("\n");
	#endif
	setBufStatus(buf, BUF_PROCESSING);
	fillInBufferStat(bId, traverseToBufferSeq(bId), buf, bStat);
	*retBId = bId;
	return 1;
    }
}

void BufferManager::markWritten(BufferId bId) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    BufferInfo &buf = *seq.bufferList[bId.buf];
    setBufStatus(buf, BUF_DISKONLY);
    free(buf.memLoc);
    buf.memLoc = NULL;

    uLock.unlock();  // don't hold multiple locks
    freeMem(buf.compressedLen);
}	
	

int BufferManager::writeABuffer() {
    BufferId bId;
    BufferStat bStat;
    
    if (getBufferToCache(&bId, &bStat)) {
	sdb.writeBuffer(bId, bStat.data, bStat.len);
	markWritten(bId);
	return 1;
    }
    return 0;
}
	

int BufferManager::getBufferStat(BufferId bId, BufferStat *bStat) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    BufferInfo &buf = *seq.bufferList[bId.buf];
    fillInBufferStat(bId, seq, buf, bStat);
    return 1;
}


int BufferManager::returnBuffer(BufferId bId) {
    #ifdef DEBUGPRINT
    printf("calling returnBuffer ");
    bId.print();
    printf("\n");
    #endif
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    BufferInfo &buf = *seq.bufferList[bId.buf];
    --buf.accessors;
    if (buf.accessors)
	return 1;

    if (buf.status == BUF_OFFDISK)
	return 1;

    setBufStatus(buf, BUF_PROCESSING);
    uLock.unlock();

    sdb.releaseBuffer(buf.memLoc, buf.compressedLen);
    buf.memLoc = NULL;
    freeMem(buf.compressedLen);

    uLock.lock();
    setBufStatus(buf, BUF_DISKONLY);
    return 1;
}

void BufferManager::deleteBufferGroup(BufferId bId) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);

    // this assert doesn't make sense anymore... maybe we need a more complex assert?
    //assert(seq.status == SEQ_FINISHED);  
    seq.status = SEQ_DELETED;
    for (uint32_t i = 0; i < seq.bufferList.size(); ++i) {
	BufferInfo &buf = *seq.bufferList[i];
	bId.buf = i;
	if (waitForSettledStatus(buf, uLock))
	    seq = traverseToBufferSeq(bId);
				   
	#ifdef DEBUGPRINT
	printf("deleting buffer ");
	bId.print();
	printf("\n");
	printf("status: %d, accessors: %d\n", buf.status, buf.accessors);
	#endif
	
	test(buf.accessors == 0, "there are accessors on a buffer being deleted!");
	test(buf.status != BUF_ONDISK, "buffer to be deleted has status ONDISK");

	setBufStatus(buf, BUF_PROCESSING);
	if (buf.memLoc) {
	    bIdSet.del(bId);
	}
    }

    uint32_t bufCount = seq.bufferList.size();
    uLock.unlock();
    
    for (unsigned int i = 0; i < bufCount; ++i) {
	uLock.lock();
	seq = traverseToBufferSeq(bId);
	BufferInfo *bufPtr = seq.bufferList[i];
	seq.bufferList[i] = NULL;
	uLock.unlock();
	
	bId.buf = i;
	if (bufPtr->memLoc) {
	    free(bufPtr->memLoc);
	    freeMem(bufPtr->compressedLen);
	} else {
	    sdb.deleteBuffer(bId);
	}
	delete bufPtr;
    }
    uLock.lock();
    seq = traverseToBufferSeq(bId);
    seq.bufferList.clear();
    seq.bufferList.shrink_to_fit();
}
	
void BufferManager::setGroupFinished(BufferId bId) {
    std::lock_guard<std::mutex> lock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    assert(seq.status == SEQ_WORKING);
    seq.status = SEQ_FINISHED;
}

void BufferManager::setGroupStatus(BufferId bId, SeqStatus s) {
    std::lock_guard<std::mutex> lock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    seq.status = s;
}
    
int BufferManager::groupFinished(BufferId bId) {
    std::lock_guard<std::mutex> lock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    //printf("groupFinished: ");
    //bId.print();
    //printf(" status: %d\n", seq.status);
    return (seq.status == SEQ_FINISHED);
}

int BufferManager::getGroupStat(BufferId bId, BGroupStat *bgStat) {
    std::lock_guard<std::mutex> lock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    bgStat->bufCount = seq.bufferList.size();
    bgStat->positions = seq.positions;
    bgStat->status = seq.status;
    bgStat->fullStep = seq.fullStep;
    return 1;
}


int BufferManager::startCachingThread() {
    stopCacheThreadsFlag = false;
    cacheThreads.emplace_back(&BufferManager::cachingThreadRun, this);
    return 1;
}


void BufferManager::cachingThreadRun() {
    printf("Starting caching thread\n");
    while(1) {
	if (waitForCachingNeededOrStopEvent())
	    writeABuffer();
	else
	    return;
    }
}

void BufferManager::waitForCachingTarget() {
    std::unique_lock<std::mutex> lock(memMutex);
    while(1) {
	if (availMem >= targetMem)
	    return;
	memCondition.wait(lock);
    }
}

int BufferManager::waitForCachingNeededOrStopEvent() {
    std::unique_lock<std::mutex> lock(memMutex);
    while(1) {
	if (stopCacheThreadsFlag)
	    return 0;
	if (availMem <= targetMem)
	    return 1;
	memUsedCondition.wait(lock);
    }
}

void BufferManager::stopAllCachingThreads() {
    std::unique_lock<std::mutex> lock(memMutex);
    stopCacheThreadsFlag = true;
    lock.unlock();
    memUsedCondition.notify_all();
    for (unsigned i = 0; i < cacheThreads.size(); ++i) {
	cacheThreads[i].join();
    }
}
