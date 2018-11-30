#include "showWin.h"
#include <string.h>

int prevLoc[MAX_PIECE];


/*
 * Renumber the pieces so that if a piece is still in the
 * same position the last time this function was called it
 * will still have the same label.
 */
void renumberPieces(PieceIdType *board, PosType *curLoc) {
   static int firstTime = 1; 
   int a, b, c;

   if (firstTime) {
      for (a = 0; a < numPiece; ++a)
         prevLoc[a] = curLoc[a];

      firstTime = 0;
      return;
   }

   for (a = 0; a < numPiece; ++a) {
      for (b = 0; b < numPiece; ++b) {
         if (a == b)
            continue;

         if ((piece[a].pType == piece[b].pType) &&
             (curLoc[b] == prevLoc[a])) {
            /* if we're here then piece 'b' was in slot 'a' last move */
            /* first swap curLoc */
            curLoc[b] = curLoc[a];
            curLoc[a] = prevLoc[a];

            /* then swap the pieces on *board */
            for (c = 0; c < pieceTypes[piece[a].pType].numBloc; ++c) {
               board[curLoc[a] + pieceTypes[piece[a].pType].xyoffset[c]] = a;
               board[curLoc[b] + pieceTypes[piece[b].pType].xyoffset[c]] = b;
	    }
            /* we've already swapped this piece, we had */
            /* better not be able to find it later so...*/
               break; 
	 }
      }
   }
}


void resetPrevLoc(PosType *curLoc) {
   int a;

   for (a = 0; a < numPiece; ++a)
      prevLoc[a] = curLoc[a];
}


void findDif(PosType *curLoc, int *movedPiece, int *movedUp, int *movedRight) {
   int a;

   for (a = 0; a < numPiece; ++a) {
      if (curLoc[a] != prevLoc[a]) {
         *movedPiece = a;
         *movedUp = prevLoc[a] / xsize - curLoc[a] / xsize;
         *movedRight = curLoc[a] % xsize - prevLoc[a] % xsize;
         return;
      }
   }
}



void showWin(BufferId inBId, uint64_t offset) {

    // first off, we're going to need a chunk of memory to hold the compressed trail
    // through the winning sequence (since we generate it in reverse but want to show it forwards)
    uint8_t *winSeq = (uint8_t *)malloc((inBId.step+1) * compressedPuzSize);
    test(winSeq != NULL, "Can't allocate memory to show winning sequence!");
    uint8_t *curSeq = winSeq;
    
    WorkerThread wt = WorkerThread();
    uint32_t finalStep = inBId.step;

    BufferId bId = inBId;
    uint64_t off = offset;

    // we need to do the first part of this (copying down the next position on our list)
    // finalStep + 1 times.  We need to do the second part (finding the next position)
    // only finalStep times.  Thus there's a break in the middle of the for loop.
    for (uint32_t s = 0; s <= finalStep; ++s) {
	uint32_t step = finalStep - s;
	// make a copy of the position we want
	ComprehensiveReadCompressTool crct;
	INIT_CRCT(crct);
	crct.start(bId);

	uint64_t offCopy = off;
	while(offCopy) {
	    crct.getNext();
	    offCopy--;
	}
	uint8_t *pos = crct.getNext();
	memcpy(curSeq, pos, compressedPuzSize);
	curSeq += compressedPuzSize;

	if (s == finalStep)
	    break;
	
	// find all of the potential previous positions
	// (and yes I know that just using processBuffer() duplicates the work I just did)
	BufferId outBId = BufferId(1, s, 0, 0, 0);
	wt.processBuffer(bId, off, 1, outBId);
	BGroupStat bgStat;
	bufMan.getGroupStat(BufferId(1, s, 0, 0), &bgStat);
	
	test(wt.findDuplicate(outBId, bufMan.finalBufId(0, step-1), &bId, &off),
	     "Can't find previous position when tracing win!");
    }



    for (uint32_t s = 0; s <= finalStep; ++s) {
	curSeq -= compressedPuzSize;
	wt.uncompressBoard(curSeq);
	wt.printBoard();
    }
}




#if 0

void ShowWin()
   {
   unsigned char cString[256];
   unsigned char *cSPtr;
   Generation *gen;
   int genNum;
   unsigned char *winSeq;
   unsigned char *curSeq;
   int moveNum;
   int movedUp;
   int movedRight;
   int movedPiece;

   printf("***FOUND WIN ***\n");

   if (winOnly)
      {
      PrintBoard(workBoard);
      printf("Finished in %d seconds\n", (int)(time(NULL) - startTime));
      exit(0);
      }
   /*
    * goofy hack so that we aren't looking for a win when using
    * calls such as ProcessBoard()
    */
   concurrent = 1;
   if (NULL == (winSeq = (unsigned char *) malloc((curMove + 1) * bt_entrySize)))
      {
      printf("unable to allocate memory to show win sequence\n");
      exit(0);
      }

   curSeq = winSeq;
   CompressBoard(cString, workBoard);
   memcpy(curSeq, cString, bt_entrySize);
   UncompressBoard(cString, workBoard);

   if (bigStorage)
      genNum = curMoveNum - 1;
   else
      gen = curGen;
   
   while(1)
      {
      if (bigStorage)
         {
         if (genNum <= 0)
            break;
         }
      else
         {
         if (!gen)
            break;
         }

      curSeq += bt_entrySize;
      BT_Reset(newPosList);
      ProcessBoard();
      BT_CvtToLL(newPosList);
      if (bigStorage)
         {
         if (NULL == (cSPtr = FindDuplicateInLinkedList(genNum, -1, newPosList)))
            {
            printf("Can't find board in gen (bigstorage)\n");
            exit(0);
            }
         }
      else
         {
         if (NULL == (cSPtr = FindBoardInGen(gen)))
            {
            printf("Can't find board in gen\n");
            exit(0);
            }
         }
      memcpy(curSeq, cSPtr, bt_entrySize);
      UncompressBoard(curSeq, workBoard);
      if (bigStorage)
         {
         --genNum;
         }
      else
         {
         gen = gen->prev;
         }
      }
   
   moveNum = 0;
   for ( ; curSeq >= winSeq; curSeq -= bt_entrySize)
      {
      UncompressBoard(curSeq, workBoard);
      RenumberPieces(workBoard);
      FindDif(&movedPiece, &movedUp, &movedRight);
      ResetPrevLoc();
      if (moveNum++)
         {
         printf("move %d: piece %c", moveNum - 1, PieceNum2Char(movedPiece));
         if (movedUp > 0)
            printf(" up %d", movedUp);
         if (movedUp < 0)
            printf(" down %d", -movedUp);
         if (movedRight > 0)
            printf(" right %d", movedRight);
         if (movedRight < 0)
            printf(" left %d", -movedRight);
         printf("\n");
         if (!noGraph)
            PrintBoard(workBoard);
         }
      else
         {
         printf("start position\n");
         PrintBoard(workBoard);
         }
      }

   printf("Finished in %d seconds\n", (int)(time(NULL) - startTime));
   exit(0);
   }


void ConcurrentShowWin(unsigned char *cPtr)
   {
   unsigned char *cSPtr;
   Generation *gen;
   Generation *fGen;
   Generation *rGen;
   int genNum;
   int fGenNum;
   int rGenNum;
   unsigned char *winSeq1;
   unsigned char *curSeq1;
   unsigned char *winSeq2;
   unsigned char *curSeq2;
   int moveNum;
   int movedUp;
   int movedRight;
   int movedPiece;

   printf("***FOUND WIN ***\n");
   if (winOnly)
      {
      PrintBoard(workBoard);
      printf("Finished in %d seconds\n", (int)(time(NULL) - startTime));
      exit(0);
      }

   if (NULL == (winSeq1 = (unsigned char *) malloc(curMove * bt_entrySize)))
      {
      printf("unable to allocate memory to show win sequence\n");
      exit(0);
      }
   if (NULL == (winSeq2 = (unsigned char *) malloc(curMove * bt_entrySize)))
      {
      printf("unable to allocate memory to show win sequence\n");
      exit(0);
      }


   if (bigStorage)
      {
      if (processForward)
         {
         fGenNum = curMoveNum - 2;
         rGenNum = curMoveNum - 3;
         }
      else
         {
         fGenNum = curMoveNum - 3;
         rGenNum = curMoveNum - 2;
         }
      genNum = fGenNum;
      }
   else
      {
      if (processForward)
         {
         fGen = curGen;
         rGen = rPrevGen;
         }
      else
         {
         fGen = fPrevGen;
         rGen = curGen;
         }
      gen = fGen;
      }

   curSeq1 = winSeq1;
   memcpy(curSeq1, cPtr, bt_entrySize);
   UncompressBoard(curSeq1, workBoard);

   while(1)
      {
      if (bigStorage)
         {
         if (genNum <= 0)
            break;
         }
      else
         {
         if (!gen)
            break;
         }

      curSeq1 += bt_entrySize;
      BT_Reset(newPosList);
      ProcessBoard(workBoard);
      BT_CvtToLL(newPosList);
      if (bigStorage)
         {
         if (NULL == (cSPtr = FindDuplicateInLinkedList(genNum, -1, newPosList)))
            {
            printf("Can't find board in gen (bigstorage)\n");
            exit(0);
            }
         }
      else
         {
         if (NULL == (cSPtr = FindBoardInGen(gen)))
            {
            printf("Can't find board in gen\n");
            exit(0);
            }
         }
      memcpy(curSeq1, cSPtr, bt_entrySize);
      UncompressBoard(curSeq1, workBoard);
      if (bigStorage)
         genNum -= 2;
      else
         gen = gen->prev;
      }
   
   curSeq2 = winSeq2;
   memcpy(curSeq2, winSeq1, bt_entrySize);
   UncompressBoard(curSeq2, workBoard);


   if (bigStorage)
      genNum = rGenNum;
   else
      gen = rGen;
   while (1)
      {
      if (bigStorage)
         {
         if (genNum <= 0)
            break;
         }
      else
         {
         if (!gen)
            break;
         }

      curSeq2 += bt_entrySize;
      BT_Reset(newPosList);
      ProcessBoard(workBoard);
      BT_CvtToLL(newPosList);
      if (bigStorage)
         {
         if (NULL == (cSPtr = FindDuplicateInLinkedList(genNum, -1, newPosList)))
            {
            printf("Can't find board in gen (bigstorage)\n");
            exit(0);
            }
         }
      else
         {
         if (NULL == (cSPtr = FindBoardInGen(gen)))
            {
            printf("Can't find board in gen\n");
            exit(0);
            }
         }
      memcpy(curSeq2, cSPtr, bt_entrySize);
      UncompressBoard(curSeq2, workBoard);
      if (bigStorage)
         genNum -= 2;
      else
         gen = gen->prev;
      }

   
   moveNum = 0;
   for ( ; curSeq1 >= winSeq1; curSeq1 -= bt_entrySize)
      {
      UncompressBoard(curSeq1, workBoard);
      RenumberPieces(workBoard);
      FindDif(&movedPiece, &movedUp, &movedRight);
      ResetPrevLoc();
      if (moveNum++)
         {
         printf("move %d: piece %c", moveNum - 1, PieceNum2Char(movedPiece));
         if (movedUp > 0)
            printf(" up %d", movedUp);
         if (movedUp < 0)
            printf(" down %d", -movedUp);
         if (movedRight > 0)
            printf(" right %d", movedRight);
         if (movedRight < 0)
            printf(" left %d", -movedRight);
         printf("\n");
         if (!noGraph)
            PrintBoard(workBoard);
         }
      else
         {
         printf("start position\n");
         PrintBoard(workBoard);
         }
      }

   for (curSeq1 = winSeq2 + bt_entrySize; curSeq1 <= curSeq2; curSeq1 += bt_entrySize)
      {
      UncompressBoard(curSeq1, workBoard);
      RenumberPieces(workBoard);
      FindDif(&movedPiece, &movedUp, &movedRight);
      ResetPrevLoc();
      if (moveNum++)
         {
         printf("move %d: piece %c", moveNum - 1, PieceNum2Char(movedPiece));
         if (movedUp > 0)
            printf(" up %d", movedUp);
         if (movedUp < 0)
            printf(" down %d", -movedUp);
         if (movedRight > 0)
            printf(" right %d", movedRight);
         if (movedRight < 0)
            printf(" left %d", -movedRight);
         printf("\n");
         if (!noGraph)
            PrintBoard(workBoard);
         }
      else
         {
         printf("start position\n");
         PrintBoard(workBoard);
         }
      }

   printf("Finished in %d seconds\n", (int)(time(NULL) - startTime));
   exit(0);
   }


#endif
