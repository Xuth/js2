#include "js2.h"
#include "readParm.h"
#include "showWin.h"
#include "bitset.h"
#include "taskQueue.h"
#include "taskManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <vector>
#include <assert.h>

char TITLE_TXT[] = "js2 (formerly jim'slide)";
char VERSION_TXT[] = "version 1.5";
char COPYRIGHT_TXT[] = "copyright Jim Leonard (Xuth) 1999 - 2018";
char LICENSE_TXT[] = "this code may be distributed under the BSD 4-Clause License included in the file LICENSE";

// puzzle defining globals
PieceTypeDef *pieceTypes;
PosType *posTypePool;
PieceDef *piece;
PosType *startLocs;   // starting locations for each piece




WinList winList;
WallList wallList;

int numPiece;
int numSpace;
int numType;
int bitsPerType;
int compressedPuzSize;
int xsize;
int ysize;
int startLoc;
int endLoc;
int totSize;
int concurrent = 0;
int winOnly = 0;
int useLetters = 0;
int hexPrint = 0;
int noGraph = 0;
long int bigmem = 4000000000;
long int smallmem = 2000000;
int directed;
int cutoffStep;
int checkFullTree;
int numThreads = 3;
int diskThreads = 1;
int cachePercent=50;


PieceIdType *baseBoard;

PosType DIR[4];

time_t startTime;

PieceTypeType uncompressMask;

BufferManager bufMan;

WorkerThread *workerList;
std::vector<std::thread> threads;

int main(int argc, char *argv[]) {
    time(&startTime);
    printf("%s\n", TITLE_TXT);
    printf("%s\n", VERSION_TXT);
    printf("%s\n", COPYRIGHT_TXT);
    printf("%s\n", LICENSE_TXT);
    setvbuf(stdout, NULL, _IONBF, 0);
    
    readParms(argc, argv);
    doInits();

    workerList = new WorkerThread[numThreads];
    for (int i = 0; i < numThreads; ++i) {
	printf("starting thread %d\n", i);
	threads.emplace_back(&WorkerThread::run, &workerList[i]);
    }

    bufMan.setTargetMemAvail(bigmem * cachePercent / 100);
    for (int i = 0; i < diskThreads; ++i)
	bufMan.startCachingThread();

    TaskManager taskMan;
    taskMan.run();
    
}

// effectively assert but prints a potentially useful string if the assert fails
void test(int t, const char *s) {
    if(!t) {
	printf("%s\n", s);
	assert(0);
    }
}


void printCompressed(uint8_t *buf) {
    for (int i = 0; i < compressedPuzSize; ++i) {
	int v = buf[i];
	printf("%02x ", v);
    }
}

WorkerThread::WorkerThread() {
    // allocate memory.  Ideally all of the thread specific memory should be
    // allocated in one small block that has a chance of getting dropped into
    // L1 cache.
    // with the exception of posBuffer which we'll tack on the end
    
    size_t locMemSize = numPiece * sizeof(PosType);
    // max possible number of pieces a piece can move about is numSpace+1
    // depending on the puzzle locList might be better as a bitset
    size_t locListMemSize = (numSpace+1) * sizeof(PosType);
    size_t boardMemSize = totSize * sizeof(PieceIdType);
    size_t placedMemSize = BITNSLOTS(numPiece);
    
    size_t size = locMemSize + locListMemSize + boardMemSize + placedMemSize + smallmem;

    char *mem = (char *)malloc(size);
    test(mem != NULL, "Can't malloc in WorkerThread");

    // hand out memory in order of largest object type
    curLoc = (PosType *)mem;
    mem += locMemSize;
    locList = (PosType *)mem;
    mem += locListMemSize;
    workBoard = (PieceIdType *)mem;
    mem += boardMemSize;
    placed = (uint8_t *)mem;
    mem += placedMemSize;
    posBuffer = (uint8_t *)mem;

    writePtr = posBuffer + writePtrOffset;
}

WorkerThread::~WorkerThread() {
    free(curLoc);
}

void WorkerThread::copyToCurLoc(PosType *locs) {
    memcpy(curLoc, locs, numPiece * sizeof(PosType));
}

int posX(int xyPos) {
    return xyPos % xsize - 1;
}
int posY(int xyPos) {
    return xyPos / xsize - 1;
}
void WorkerThread::initBoardFromLocs() {
    memcpy(workBoard, baseBoard, totSize * sizeof(PieceIdType));
    for (int i = 0; i < numPiece; ++i) {
	int startOffset = curLoc[i];
	PieceTypeDef *ptd = &pieceTypes[piece[i].pType];
	for (int j = 0; j < ptd->numBloc; ++j) {
	    int offset = startOffset + ptd->xyoffset[j];
	    if (workBoard[offset] != SPACE_CHAR) {
		printf("initial positions overlap\n");
		printf("offending piece at (%d, %d) ", posX(offset), posY(offset));
		printf("overlaps at (%d, %d)\n", posX(offset+ptd->xyoffset[j]), posY(offset+ptd->xyoffset[j]));
		exit(0);
	    }

	    workBoard[offset] = i;
	}
    }
}
	
void WorkerThread::printBoard() {
    for (int y = 0; y < ysize; ++y) {
	for (int x = 0; x < xsize; ++x) {
	    int pieceNum = workBoard[x + y * xsize];
	    if (hexPrint) {
		switch(pieceNum) {
		case WALL_CHAR:
		    printf("## ");
		    break;
		case SPACE_CHAR:
		    printf("   ");
		    break;
		default:
		    printf("%02x ", pieceNum);
		}
	    } else {
		switch(pieceNum) {
		case WALL_CHAR:
		    printf("# ");
		    break;
		case SPACE_CHAR:
		    printf("  ");
		    break;
		default:
		    printf("%c ", pieceNum2Char(pieceNum));
		}
	    }
	}
	printf("\n");
    }
    printf("\n");
}

char pieceNum2Char(int pn) {
    if (piece[pn].label) {
	return piece[pn].label;
    }

    if (useLetters) {
	if (pn < 26)
	    return pn + 'A';
	else if (pn < 52)
	    return pn - 26 + 'a';
	else if (pn < 62)
	    return pn - 52 + '0';
	else
	    return '^';
    }

    /* default - use numbers */
    if (pn < 10)
	return pn + '0';
    if (pn < 36)
	return pn - 10 + 'A';
    if (pn < 62)
	return pn - 36 + 'a';
    else
	return '^';
}

void WorkerThread::callRenumber() {
    renumberPieces(workBoard, curLoc);
}

void WorkerThread::renumberAndFindDiff(PieceIdType *movedPiece, PosType *movedUp, PosType *movedRight) {
    renumberPieces(workBoard, curLoc);
    findDiff(curLoc, movedPiece, movedUp, movedRight);
    resetPrevLoc(curLoc);
}

void WorkerThread::compressBoard() {
    uint8_t *compressed = writePtr;
    BITRESETALL(placed, numPiece);
    *compressed = 0;
    int curBit = 0;
    
    for (PieceIdType *loc = &workBoard[startLoc]; loc < &workBoard[endLoc]; ++loc) {
	int pType = 0;
	if (*loc == WALL_CHAR)
	    continue;
	if (*loc != SPACE_CHAR) {
	    if (BITTEST(placed, *loc))
		continue;
	    BITSET(placed, *loc);
	    pType = piece[*loc].pType + 1;  // space get's zero
	}

	*compressed |= pType << curBit;
	curBit += bitsPerType;
	if (curBit > 7) {
	    ++compressed;
	    curBit -= 8;
	    if (curBit)
		*compressed = pType >> (bitsPerType - curBit);
	    else
		*compressed = 0;
	}
    }
//    for (int i = 0; i < compressedPuzSize; ++i)
//	printf("%02x ", writePtr[i]);
//    printf("\n");

    writePtr += compressedPuzSize;
}


void WorkerThread::uncompressBoard(uint8_t *compressed) {
    memcpy(workBoard, baseBoard, totSize * sizeof(PieceIdType));
    BITRESETALL(placed, numPiece);
    int curBit = 0;

    for (PieceIdType *loc = &workBoard[startLoc];
	 loc < &workBoard[endLoc];
	 ++loc) {

	if (*loc != SPACE_CHAR) // ie this space has already been placed
	    continue;

	// peel a type off of the compressed board info
	PieceTypeType pType = *compressed >> curBit;
	curBit += bitsPerType;
	if (curBit > 7) {
	    curBit -= 8;
	    ++compressed;
	    if (curBit)
		pType |= *compressed << (bitsPerType - curBit);
	}
	pType &= uncompressMask;

	if (!pType) // ie we should be placing a space
	    continue;  // board is already marked with a space in this loc.  Do nothing!

	--pType;
	int i;
	for (i = 0; i < numPiece; ++i) {  // find an available piece of the right type
	    if (piece[i].pType != pType) // is this piece the right type?
		continue;
	    if (BITTEST(placed, i)) // has this piece been placed?
		continue;

	    curLoc[i] = loc - workBoard;
	    BITSET(placed, i);

	    PieceTypeDef *ptp = &pieceTypes[pType];
	    
	    for (int b = 0; b < ptp->numBloc; ++b)
		loc[ptp->xyoffset[b]] = i;
	    break;
	}

	test(i != numPiece, "Can't place a piece in uncompress()");
    }
}


int qscmp(const void *s1, const void *s2) {
    return memcmp(s1, s2, compressedPuzSize);
}

void WorkerThread::insertBuffer() {
    if (writePtr == posBuffer + writePtrOffset) // nothing in the buffer but wish to close it.  Do nothing.
	return;
    // sort
    qsort(&posBuffer[writePtrOffset],
	  (writePtr - posBuffer - writePtrOffset) / compressedPuzSize,
	  compressedPuzSize, qscmp);

    // dedup and compress
    unsigned int posCount;
    CompressTool ct;
    INIT_COMPRESSTOOL(ct);
    size_t compressedLen = ct.compressDedupSortedBuf(posBuffer, writePtr, &posCount);

    // write the buffer
    test(bufMan.appendBufferGroup(&bId), "can't append new buffergroup in insertBuffer");
    test(bufMan.insertBuffer(bId, posBuffer, compressedLen, posCount), "failed to insertBuffer");
    bufMan.setGroupFinished(bId);

    TaskItem t;
    t.type = TaskItem::FinishedGroup;
    t.bufGroup = TaskFinishedBufGroup(bId);
    DoneQueue.add(t);

    // reset to continue
    writePtr = posBuffer + writePtrOffset;
    bId.buf++;
}


void WorkerThread::processBuffer(BufferId inBId, uint64_t offset, uint64_t count,
				 BufferId outBId) {
    bId = outBId;

    ComprehensiveReadCompressTool crct;
    INIT_CRCT(crct);
    crct.start(inBId);
    
    while(offset) {
	crct.getNext();
	offset--;
    }
	
    for (unsigned int i = 0; i < count; ++i) {
	uint8_t *in = crct.getNext();
	test(in != NULL, "not enough positions to process in processBuffer");

	uncompressBoard(in);

	processBoard();

    }
    // return the input buffers and clean up the output buffers
    crct.finish();
    insertBuffer();
}
    
void WorkerThread::printBuffer(BufferId inBId, uint64_t offset, uint64_t count) {
    ComprehensiveReadCompressTool crct;
    INIT_CRCT(crct);
    crct.start(inBId);
    
    while(offset) {
	crct.getNext();
	offset--;
    }
	
    for (unsigned int i = 0; i < count; ++i) {
	uint8_t *in = crct.getNext();
	test(in != NULL, "not enough positions to process in processBuffer");

	uncompressBoard(in);

	printBoard();
    }
    // return the input buffers and clean up the output buffers
    crct.finish();
}
void WorkerThread::printBuffer(BufferId inBId) {
    ComprehensiveReadCompressTool crct;
    INIT_CRCT(crct);
    crct.start(inBId);

    while(crct.getNext()) {
	uncompressBoard(crct.c);

	for (int i = 0; i < compressedPuzSize; ++i)
	    printf("%02x ", crct.c[i]);
	printf("\n");

	printBoard();
    }
    // return the input buffers and clean up the output buffers
    crct.finish();
}
    
	
void WorkerThread::processBoard() {
   for (pieceNum = 0; pieceNum < numPiece; ++pieceNum) {
       pPtr = &pieceTypes[piece[pieceNum].pType];
       pStartLoc = curLoc[pieceNum];

       ResetLocList();
       AddLocList(pStartLoc);
       if (pPtr->numBloc == 1)
	   TryPiece_1();
       else
	   TryPiece();
   }
}


void WorkerThread::TryPiece_1() {
    /* do horizontal first */
    if (pPtr->moveX) {
	TryMove_1(1);
	
	TryMove_1(-1);
    }
    
    /* now do vertical */
    if (pPtr->moveY) {
	TryMove_1(xsize);
	
	TryMove_1(-xsize);
    }
}

void WorkerThread::TryMove_1(int offset) {
    /* because we're only moving 1 block we can take some liberties */
    /* that we couldn't normally take and some of the following vars */
    /* take on meanings slightly different from their counterparts */
    PosType savedLoc = curLoc[pieceNum];
    PosType newLoc = savedLoc + offset;
    PieceIdType *newBoardLoc = workBoard + newLoc;

    /* see if we can move the piece */
    if (*newBoardLoc != SPACE_CHAR)
	return;

    // see if we've previously been here
    if (!AddLocList(newLoc))
	return;
    
    PieceIdType *savedBoardLoc = workBoard + savedLoc;;

    // move the piece
    *savedBoardLoc = SPACE_CHAR;
    *newBoardLoc = pieceNum;
    
    curLoc[pieceNum] = newLoc;

    /* check if xstep or ystep is active on this piece and act on it as apppropriate */
    if (!pPtr->xstep || (!((newLoc % xsize - pStartLoc % xsize) % pPtr->xstep)))
	if (!pPtr->ystep || (!((newLoc / xsize - pStartLoc / xsize) % pPtr->ystep))) {
	    /* copy the board to the move queue if applicable */
	    enqueueBoard();
	}

    TryPiece_1();
    curLoc[pieceNum] = savedLoc;

    /* move the piece back to be a good behaved routine */
    *newBoardLoc = SPACE_CHAR;
    *savedBoardLoc = pieceNum;
}

void WorkerThread::TryPiece() {
    /* do horizontal first */
    if (pPtr->moveX) {
	TryMove(MOVE_RIGHT);
	
	TryMove(MOVE_LEFT);
    }
    
    /* now do vertical */
    if (pPtr->moveY) {
	TryMove(MOVE_UP);
	
	TryMove(MOVE_DOWN);
    }
}

void WorkerThread::TryMove(int direction) {
    PosType savedLoc = curLoc[pieceNum];
    PosType newLoc = curLoc[pieceNum] + DIR[direction];
    PieceIdType *curBoardLoc = workBoard + savedLoc;

    // see if there's space to move this piece
    int i;
    for (i = 0; i < pPtr->moveBlock[direction]; ++i)
	if (curBoardLoc[pPtr->moveTo[direction][i]] != SPACE_CHAR)
	    break;
    if (i != pPtr->moveBlock[direction])
	// so far we've done no actual work so if we don't have space to move this piece just return.
	return;

    if (!AddLocList(newLoc))  // also stop if we've been here before
	return;

    // move the piece
    for (i = 0; i < pPtr->moveBlock[direction]; ++i) {
	curBoardLoc[pPtr->moveTo[direction][i]] = pieceNum;
	curBoardLoc[pPtr->moveFrom[direction][i]] = SPACE_CHAR;
    }

    curLoc[pieceNum] = newLoc;

    /* check if xstep or ystep is active on this piece and act on it as apppropriate */
    if (!pPtr->xstep || (!((newLoc % xsize - pStartLoc % xsize) % pPtr->xstep)))
	if (!pPtr->ystep || (!((newLoc / xsize - pStartLoc / xsize) % pPtr->ystep))) {
	    /* copy the board to the move queue if applicable */
	    enqueueBoard();
	}

    // iterate further over this piece
    TryPiece();

    // now back out our changes
    curLoc[pieceNum] = savedLoc;

    for (i = 0; i < pPtr->moveBlock[direction]; ++i) {
	curBoardLoc[pPtr->moveTo[direction][i]] = SPACE_CHAR;
	curBoardLoc[pPtr->moveFrom[direction][i]] = pieceNum;
    }
}




void WorkerThread::enqueueBoard() {
    if (writePtr - posBuffer + compressedPuzSize >= smallmem) 
	insertBuffer();
    compressBoard();
}

void CompressTool::initOutBuf(uint8_t *buf, size_t len) {
    r = buf;
    end = buf + len;
}

int CompressTool::compressDedupAdd(uint8_t *pos, int first) {
    /*
      returns 0 if position not added because buffer is full
      returns 1 if successfully added
      returns 2 if not added because of a duplicate
      
      if a duplicate is attempted to be added this will always return 2 (and not 0)
    */
    if (first) {
	memcpy(c, pos, compressedPuzSize);
	*(r++) = 0;
	memmove(r, pos, compressedPuzSize);
	r += compressedPuzSize;
	return 1;
    }

    CompressLenType b;
    for (b = 0; b < compressedPuzSize; ++b)
	if (c[b] != pos[b])
	    break;
    if (b == compressedPuzSize) {// ie duplicate position
	return 2;
    }
    // see how long the entry is and if it will fit on the buffer
    int entryLen = 1;  // byte copy of b or flag to say that we need a full length copy
    if (b > 254)  // if true we need space for the full length copy
	entryLen += sizeof(CompressLenType);
    entryLen += compressedPuzSize - b;  // and the non-duplicate bytes
    if (r + entryLen >= end)
	return 0;
    
    if (b > 254) {
	*(r++) = 255;
	memcpy(r, &b, sizeof(CompressLenType));
	r += sizeof(CompressLenType);
    } else {
	*(r++) = b;
    }
	
    memmove(r, pos+b, compressedPuzSize - b);
    memcpy(c+b, pos+b, compressedPuzSize - b);
    r += compressedPuzSize - b;
    return 1;
}

size_t CompressTool::compressDedupSortedBuf(uint8_t *buf, uint8_t *endBuf, unsigned int *positions) {
    initOutBuf(buf, endBuf - buf);
    int first = 1;
    *positions = 0;

    uint8_t *bufPtr = buf + writePtrOffset;
    for ( ; bufPtr < endBuf; bufPtr += compressedPuzSize) {
	int res = compressDedupAdd(bufPtr, first);
	first = 0;
	test(res != 0, "compressDedupSortedBuf should never get a result of 0 from CompressTool\n");
	if (res == 1)
	    *positions += 1;
    }
    return r - buf;
}

void CompressTool::initBuf(uint8_t *buf, size_t len) {
    r = buf;
    end = buf + len;
}

uint8_t *CompressTool::getNext() {
    if (r >= end)
	return NULL;
    
    CompressLenType b = *(r++);
    if (b == 255) {
	memcpy(&b, r, sizeof(CompressLenType));
	r += sizeof(CompressLenType);
    }
//    printf("b: %d\n", b);
    memcpy(c+b, r, compressedPuzSize - b);
    r += compressedPuzSize - b;

//    for (int i = 0; i < compressedPuzSize; ++i)
//	printf("%02x ", c[i]);
//    printf("\n");
    
    return c;
}

void ComprehensiveReadCompressTool::start(BufferId inBId) {
    bId = inBId;
    BufferStat bStat;
    test(bufMan.getBuffer(bId, &bStat), "failed to get a buffer in CRCT");
    r = bStat.data;
    end = r + bStat.len;
    moreBuffers = bStat.buffersRemaining ? 1 : 0;
    if (moreBuffers)
	test(bId.mergeLevel != 0, "There shouldn't be multiple buffers in a group on mergeLevel 0");
    lastReturned = 0;
}

uint8_t *ComprehensiveReadCompressTool::getNext() {
    if (r >= end) {
	bufMan.returnBuffer(bId);

	if (!moreBuffers) {
	    lastReturned = 1;
	    return NULL;
	}
	
	bId.buf++;

	BufferStat bStat;
	test(bufMan.getBuffer(bId, &bStat), "failed to get next buffer in CRCT");
	r = bStat.data;
	end = r + bStat.len;
	moreBuffers = bStat.buffersRemaining ? 1 : 0;
    }
    CompressLenType b = *(r++);
    if (b == 255) {
	memcpy(&b, r, sizeof(CompressLenType));
	r += sizeof(CompressLenType);
    }

    memcpy(c+b, r, compressedPuzSize - b);
    r += compressedPuzSize - b;

    return c;
}

void ComprehensiveReadCompressTool::finish() {
    if (!lastReturned)
	bufMan.returnBuffer(bId);
}



BufferHeap::BufferHeap(int count) {
    size_t memReq = (sizeof(ComprehensiveReadCompressTool) +
		     sizeof(uint16_t) + compressedPuzSize) * count;
    char *mem = (char *)malloc(memReq);
    test(NULL != mem, "failed to get memory for a BufferHeap");

    bufCount = count;
    crct = (ComprehensiveReadCompressTool *)mem;
    mem += sizeof(ComprehensiveReadCompressTool) * count;
    heap = (uint16_t *)mem;
    mem += sizeof(uint16_t) * count;
    for (int i = 0; i < count; ++i) {
	crct[i].c = (uint8_t *)mem;
	mem += compressedPuzSize;
    }
}

void BufferHeap::finish() {
    for (int i = 0; i < bufCount; ++i)
	crct[heap[i]].finish();
}

BufferHeap::~BufferHeap() {
    finish();
    free(crct);
}

void BufferHeap::setBId(BufferId b, int slot) {
    crct[slot].start(b);
    crct[slot].getNext();
    heap[slot] = slot;

    // this is the only time we're balancing from the top so just do it inline
    int s = slot;
    while(1) {
	if (s == 0)
	    break;
	int lower = (s-1)/2;
	if (0 <= memcmp(crct[slot].c, crct[heap[lower]].c, compressedPuzSize))
	    break;
	heap[s] = heap[lower];
	heap[lower] = slot;
	s = lower;
    }
}

uint8_t *BufferHeap::getFirst() {
    return crct[heap[0]].c;
}

uint8_t *BufferHeap::getNext() {
    // advance the buffer that was in heap[0] (or replace it if now empty)
    if (NULL == crct[heap[0]].getNext()) {
	crct[heap[0]].finish();
	if (--bufCount == 0)
	    return NULL;
	heap[0] = heap[bufCount];
    }
    
    // now rebalance the heap
    int slot = heap[0];
    int s = 0;
    while(1) {
	int up = s*2 + 1;
	int up2 = up + 1;
	if (up >= bufCount)
	    break;
	if (0 < memcmp(crct[slot].c, crct[heap[up]].c, compressedPuzSize)) {
	    // we clearly need to swap slot with one of the upper nodes.  but
	    // if up2 is less than up then we should pick that instead.
	    if (up2 < bufCount)
		if (0 < memcmp(crct[heap[up]].c, crct[heap[up2]].c, compressedPuzSize))
		    up = up2;
	    heap[s] = heap[up];
	    heap[up] = slot;
	    s = up;
	    continue;
	}
	// check the other side
	up++;
	if (up >= bufCount)
	    break;
	if (0 < memcmp(crct[slot].c, crct[heap[up]].c, compressedPuzSize)) {
	    heap[s] = heap[up];
	    heap[up] = slot;
	    s = up;
	    continue;
	}
	// if we're here the heap is now ordered.
	break;
    }
    return crct[heap[0]].c;
}


void WorkerThread::mergeBuffers(BufferId *inBIds, int count, BufferId outBId) {
    BufferHeap bh(count);

    for (int i = 0; i < count; ++i)
	bh.setBId(inBIds[i], i);

    CompressTool outCt;
    INIT_COMPRESSTOOL(outCt);
    outCt.initOutBuf(posBuffer, smallmem);

    uint64_t outPos = 0;

    outCt.compressDedupAdd(bh.getFirst(), 1);

    while(1) {
	uint8_t *next = bh.getNext();
	if (!next)
	    break;

	int res = outCt.compressDedupAdd(next, 0);
	if (res == 0) { // no space
	    bufMan.insertBuffer(outBId, posBuffer, outCt.r-posBuffer, outPos);
	    outBId.buf++;
	    outCt.initOutBuf(posBuffer, smallmem);
	    test(1 == outCt.compressDedupAdd(next, 1), "failed to add to new buffer in merge");
	    outPos = 1;
	} else if (res == 1) { // success
	    outPos++;
	} // don't really care if it was a duplicate
    }
    bufMan.insertBuffer(outBId, posBuffer, outCt.r-posBuffer, outPos);
    bufMan.setGroupFinished(outBId);
    TaskItem t;
    t.type = TaskItem::FinishedGroup;
    t.bufGroup = TaskFinishedBufGroup(outBId);
    DoneQueue.add(t);
}

void WorkerThread::dedupGen(BufferId inBId, BufferId *genBIds, int genCount, BufferId outBId) {
    // prep input buffer
    ComprehensiveReadCompressTool crct;
    INIT_CRCT(crct);
    crct.start(inBId);
    
    uint8_t *inBuf = crct.getNext();

    // prep gen heap
    BufferHeap bh(genCount);
    for (int i = 0; i < genCount; ++i)
	bh.setBId(genBIds[i], i);
    uint8_t *genBuf = bh.getFirst();
    

    // prep output buffer
    CompressTool outCt;
    INIT_COMPRESSTOOL(outCt);
    outCt.initOutBuf(posBuffer, smallmem);

    uint64_t outPos = 0;
    int first = 1;

    int exhaustedGenHeap = 0;
    while(1) {
	int mc;
	
	if (exhaustedGenHeap) {
	    mc = -1;
	} else {
	    mc = memcmp(inBuf, genBuf, compressedPuzSize);
	}
	if (mc < 0) {  // inBuf doesn't exist in genBuf
	    int res = outCt.compressDedupAdd(inBuf, first);
	    first = 0;
	    if (res == 0) { // no space
		bufMan.insertBuffer(outBId, posBuffer, outCt.r-posBuffer, outPos);
		outBId.buf++;
		outCt.initOutBuf(posBuffer, smallmem);
		test(1 == (res = outCt.compressDedupAdd(inBuf, 1)), "failed to add to new buffer in merge");
		outPos = 1;
	    } else {
		outPos++;
	    }
	    test(res == 1, "somehow had duplicate in input buffer in dedupGen");
	}
	if (mc <= 0) {  // input was duplicate or copied.  thus we can advance the input.
	    inBuf = crct.getNext();
	    if (NULL == inBuf) {  // cleanup and we're done
		crct.finish();
		bh.finish();
		bufMan.insertBuffer(outBId, posBuffer, outCt.r-posBuffer, outPos);
		bufMan.setGroupFinished(outBId);
		TaskItem t;
		t.type = TaskItem::FinishedGroup;
		t.bufGroup = TaskFinishedBufGroup(outBId);
		DoneQueue.add(t);
		return;
	    }
	}
	if (mc >= 0) { // gen was duplicate or before current input.  Thus advance genheap
	    genBuf = bh.getNext();
	    if (NULL == genBuf)
		exhaustedGenHeap = 1;
	}
    }
    // unreachable
}	
	

// look for duplicates between two sets of buffers.  If no duplicates are found return 0
// if a duplicate is found return 1 and put the bufferId and offset (from bId1) into outBId and offset.
int WorkerThread::findDuplicate(BufferId bId1, BufferId bId2, BufferId *outBId, uint64_t *offset) {
    ComprehensiveReadCompressTool crct1;
    INIT_CRCT(crct1);
    crct1.start(bId1);
    uint8_t *buf1 = crct1.getNext();
    *offset = 0;

    ComprehensiveReadCompressTool crct2;
    INIT_CRCT(crct2);
    crct2.start(bId2);
    uint8_t *buf2 = crct2.getNext();

//    printf("*** looking for duplicate ***\n");
    
    while(1) {
//	printf("buf1: ");
//	printCompressed(buf1);
//	printf("\nbuf2: ");
//	printCompressed(buf2);
//	printf("\n");
	int mc = memcmp(buf1, buf2, compressedPuzSize);
	if (mc == 0) {
	    // we found something.  fill out outBId and offset, clean up, and get out of here.
	    // ok, this is shitty but it works.  Since we don't have a count into the current
	    // buffer but we have a count from the first buffer, put down the first buffer and offset
	    // from that.
	    *outBId = bId1;
	    crct1.finish();
	    crct2.finish();

	    // offset already set
	    return 1;
	}
	if (mc < 0) {
	    buf1 = crct1.getNext();
	    if (NULL == buf1) {
		//printf("buf1 empty!\n");
		break;
	    }
	    *offset += 1;
	} else {
	    buf2 = crct2.getNext();
	    if (NULL == buf2) {
		//printf("buf2 empty!\n");
		break;
	    }
	}
    }
    // no duplicates found.  clean up and return 0
    crct1.finish();
    crct2.finish();
    return 0;
}
	

int WorkerThread::checkWin(BufferId inBId, uint64_t offset, uint64_t count) {
    ComprehensiveReadCompressTool crct;
    INIT_CRCT(crct);
    crct.start(inBId);

    for (uint64_t i = 0; i < offset; ++i)
	crct.getNext();
	
    for (uint64_t i = 0; i < count; ++i) {
	uint8_t *in = crct.getNext();
	test(in != NULL, "failed to get an requested buffers to check for win");

	// get a new board to process
	uncompressBoard(in);

	if (checkIfWin()) {
	    printf("checkWin(): found winning board!!!\n");

	    TaskItem t;
	    t.type = TaskItem::FoundWin;
	    t.foundWin = TaskFoundWin(inBId, offset + i);  // this might span into the next buffer but that's ok
	    DoneQueue.add(t);
	    return 1;
	}

    }
    // return the input buffers
    crct.finish();

    return 0;
}
    
int WorkerThread::checkIfWin() {
    for (int i = 0; i < winList.numWinCond; ++i) {
	WinCond *wc = &winList.winCond[i];
	PieceIdType pNum = workBoard[wc->posXY];
	if (pNum > numPiece)  // is it a space or wall?
	    return 0;

	if (piece[pNum].pType != wc->pieceType)
	    return 0;
	
	if (curLoc[pNum] != wc->posXY)
	    return 0;
    }

    // if I'm here I found a winning position!!!
    return 1;
}

void WorkerThread::run() {

    //printf("starting run() in a worker thread\n");

    while(1) {
	TaskItem t = JobQueue.get();
	
	int i;
	int shutdown = 0;

	switch(t.type) {
	case TaskItem::ProcessPositions:
	    //printf("calling processBuffer()\n");
	    processBuffer(t.process.inp, t.process.offset, t.process.count, t.process.out);
	    DoneQueue.add(t);  // these reponses are explicitly counted to know when this part is finished
	    break;

	case TaskItem::MergeGroups:
	    //printf("calling mergeGroups()\n");
	    for (i = 0; i < TASKMERGE_MAX; ++i) {
		//t.merge.inp[i].print();
		//printf("\n");
		if (t.merge.inp[i].isNull())
		    break;
	    }
	    mergeBuffers(t.merge.inp, i, t.merge.out);
	    for (i = 0; i < TASKMERGE_MAX; ++i)
		if (!t.merge.inp[i].isNull())
		    bufMan.deleteBufferGroup(t.merge.inp[i]);
	    DoneQueue.add(t);  // is this really necessary?
	    break;

	case TaskItem::DedupGroup:
	    //printf("calling dedupGen()\n");
	    for (i = 0; i < TASKDEDUP_MAX; ++i)
		if (t.dedup.gen[i].isNull())
		    break;
	    dedupGen(t.dedup.inp, t.dedup.gen, i, t.dedup.out);
	    bufMan.deleteBufferGroup(t.dedup.inp);
	    DoneQueue.add(t);  // this one probably isn't necessary either.
	    break;

	case TaskItem::CheckWins:
	    //printf("calling checkWin(): ");
	    //t.check.inp.print();
	    //printf(" offset: %lu count: %lu\n", t.check.offset, t.check.count);
	    checkWin(t.check.inp, t.check.offset, t.check.count);
	    DoneQueue.add(t);  // these responses are explicitly counted to know when this part is finished.
	    break;

	case TaskItem::DeleteGroup:
	    bufMan.deleteBufferGroup(t.del.bId);
	    DoneQueue.add(t);
	    break;
	    
	case TaskItem::Shutdown:
	    shutdown = 1;
	    //printf("got shutdown request\n");
	    break;

	default:
	    test(0, "invalid item found on the job queue");
	}

	if (shutdown)
	    return;
    }
}
