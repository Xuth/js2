#include "js2.h"
#include <stdlib.h>
void initBaseBoard() {
    baseBoard = (PieceIdType *)malloc(totSize * sizeof(PieceIdType));
    test(baseBoard != NULL, "can't malloc baseBoard");

    for (int i = 0; i < totSize; ++i)
	baseBoard[i] = SPACE_CHAR;
    
    for (int y = 0; y < ysize; ++y) {
	baseBoard[y * xsize] = WALL_CHAR;
	baseBoard[(y + 1) * xsize - 1] = WALL_CHAR;
    }
    for (int x = 0; x < xsize; ++x) {
	baseBoard[x] = WALL_CHAR;
	baseBoard[x + xsize * (ysize - 1)] = WALL_CHAR;
    }
    for (int a = 0; a < wallList.numWall; ++a) {
	baseBoard[wallList.wallX[a] + 1 + xsize * (wallList.wallY[a] + 1)]
	    = WALL_CHAR;
    }
}

void initUncompressMask() {
    uncompressMask = 0;
    for (int a = 0; a < bitsPerType; ++a)
	uncompressMask = uncompressMask * 2 + 1;
}

void initStartBoard() {
    WorkerThread wt = WorkerThread();
    wt.copyToCurLoc(startLocs);
    wt.initBoardFromLocs();
    wt.printBoard();
    /*
     *  It's at this point (and this point only) that I have my board
     *  as the user designed it without the pieces being mixed around
     *  so...  while I'm here I should call RenumberPieces() so that
     *  I can keep a snapshot of this for when I've found my solution.
     */
    wt.callRenumber();
    wt.compressBoard();
//    bufMan.startBufferGroup(BufferId(0, 0, 0, 0));
    wt.setBId(BufferId(0,0,0,0,0));
    wt.insertBuffer();
//    bufMan.setGroupFinished(BufferId(0,0,0,0));


    CompressTool ct;
    INIT_COMPRESSTOOL(ct);
    
    // prep the input buffers
    BufferStat bStat;
    test(bufMan.getBuffer(BufferId(0,0,0,0,0), &bStat), "failed to get a buffer for init processing");
    
    uint8_t *data;
    ct.initBuf(bStat.data, bStat.len);
    data = ct.getNext();
    
    wt.uncompressBoard(data);
    wt.printBoard();
    bufMan.returnBuffer(BufferId(0,0,0,0,0));
}

void testProcess() {
//    bufMan.startBufferGroup(BufferId(0, 1, 0, 0));
    WorkerThread wt = WorkerThread();
    wt.processBuffer(BufferId(0,0,0,0,0), 0, 1, BufferId(0,1,0,0));
    wt.printBuffer(BufferId(0,1,0,0,0), 0, 4);
}
    


void doInits() {
    bufMan.setMem(bigmem);
    initBaseBoard();
    initUncompressMask();
    initStartBoard();
//    testProcess();
}
