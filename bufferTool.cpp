#include "bufferTool.h"

#include <assert.h>
#include <string.h>
#include <inttypes.h>

int BufferManager::useMem(size_t bytes) {
    std::lock_guard<std::mutex> lock(memMutex);
    if (bytes > availMem)
	return 0;
    availMem -= bytes;
    return 1;
}

void BufferManager::freeMem(size_t bytes) {
    std::lock_guard<std::mutex> lock(memMutex);
    availMem += bytes;
}

size_t BufferManager::memAvail() {
    std::lock_guard<std::mutex> lock(memMutex);
    return availMem;
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

void BufferId::print() {
    if (isNull()) {
	printf("<BufferId NULL>");
	return;
    }

    printf("<BufferId %" PRIu8 ":%" PRIu32 ":%" PRIu16 ":%" PRIu32 ":%" PRIu32 ">",
	   stepGroup, step, mergeLevel, group, buf);
}

BufferId BufferManager::finalBufId(uint8_t stepGroup, uint32_t step) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    if (stepGroup >= 2)
	return 0;
    std::vector<StepBuffer> &sl = stepList[stepGroup];
    if (sl.size() <= step)
	return 0;
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
    seq.bufferList.emplace_back(len, positions);
    seq.positions += positions;
    BufferInfo &buf = seq.bufferList[bId.buf];
    uLock.unlock();

    if (!useMem(len)) {
	printf("ran out of memory");
	assert(0);
    }

    if (NULL == (buf.memLoc = (uint8_t *)malloc(len))) {
	printf("malloc failed!");
	assert(0);
    }

    memcpy(buf.memLoc, data, len);
    
    uLock.lock();
    buf.status = BUF_OFFDISK;
    uLock.unlock();
	
    return 1;
}

void BufferManager::fillInBufferStat(BufferId &bId, BufferSequence &seq, BufferInfo &buf, BufferStat *bStat) {
    bStat->data = buf.memLoc;
    bStat->len = buf.compressedLen;
    bStat->positions = buf.positions;
    bStat->buffersRemaining = seq.bufferList.size() - bId.buf - 1;
    bStat->status = buf.status;
}

int BufferManager::getBuffer(BufferId bId, BufferStat *bStat) {
    //printf("getBuffer: ");
    //bId.print();
    BufStatus status;
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    BufferInfo &buf = seq.bufferList[bId.buf];
    status = buf.status;
    if (status != BUF_PROCESSING && buf.memLoc != NULL) {
	fillInBufferStat(bId, seq, buf, bStat);
	buf.accessors++;
	return 1;
    }
    uLock.unlock();

    // handle BUF_PROCESSING or paged out to disk
    assert(0);
    return 0;
}

int BufferManager::getBufferStat(BufferId bId, BufferStat *bStat) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    BufferInfo &buf = seq.bufferList[bId.buf];
    fillInBufferStat(bId, seq, buf, bStat);
    return 1;
}


int BufferManager::returnBuffer(BufferId bId) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);
    BufferInfo &buf = seq.bufferList[bId.buf];
    --buf.accessors;
    if (!buf.accessors || buf.status != BUF_DISKONLY)
	return 1;
    buf.status = BUF_PROCESSING;
    uLock.unlock();
    
    free(buf.memLoc);
    buf.memLoc = NULL;


    uLock.lock();
    buf.status = BUF_DISKONLY;
    return 1;
}

void BufferManager::deleteBufferGroup(BufferId bId) {
    std::unique_lock<std::mutex> uLock(bufDirMutex);
    BufferSequence &seq = traverseToBufferSeq(bId);

    // this assert doesn't make sense anymore... maybe we need a more complex assert?
    //assert(seq.status == SEQ_FINISHED);  
    seq.status = SEQ_DELETED;
    uLock.unlock();
    
    for (unsigned int i = 0; i < seq.bufferList.size(); ++i) {
	BufferInfo &buf = seq.bufferList[i];
	if (buf.memLoc) {
	    free(buf.memLoc);
	    freeMem(buf.compressedLen);
	}
	if ((buf.status == BUF_ONDISK) ||
	    (buf.status == BUF_DISKONLY)) {
	    assert(0);
	}
    }
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


