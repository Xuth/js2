#ifndef showWin_h
#define showWin_h

#include "js2.h"

void renumberPieces(PieceIdType *board, PosType *curLoc);
void resetPrevLoc(PosType *curLoc);
void findDiff(PosType *curLoc, PieceIdType *movedPiece, PosType *movedUp, PosType *movedRight);
void showWin(BufferId inBId, uint64_t offset);



#endif
