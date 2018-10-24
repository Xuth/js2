#include "showWin.h"


int prevLoc[MAX_PIECE];


/*
 * Renumber the pieces so that if a piece is still in the
 * same position the last time this function was called it
 * will still have the same label.
 */
void renumberPieces(PieceIdType *board, PosType *curLoc)
   {
   static int firstTime = 1; 
   int a, b, c;

   if (firstTime)
      {
      for (a = 0; a < numPiece; ++a)
         prevLoc[a] = curLoc[a];

      firstTime = 0;
      return;
      }

   for (a = 0; a < numPiece; ++a)
      {
      for (b = 0; b < numPiece; ++b)
         {
         if (a == b)
            continue;

         if ((piece[a].pType == piece[b].pType) &&
             (curLoc[b] == prevLoc[a]))
            {
            /* if we're here then piece 'b' was in slot 'a' last move */
            /* first swap curLoc */
            curLoc[b] = curLoc[a];
            curLoc[a] = prevLoc[a];

            /* then swap the pieces on *board */
            for (c = 0; c < pieceTypes[piece[a].pType].numBloc; ++c)
               {
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


void resetPrevLoc(PosType *curLoc)
   {
   int a;

   for (a = 0; a < numPiece; ++a)
      prevLoc[a] = curLoc[a];
   }


void findDif(PosType *curLoc, int *movedPiece, int *movedUp, int *movedRight)
   {
   int a;

   for (a = 0; a < numPiece; ++a)
      {
      if (curLoc[a] != prevLoc[a])
         {
         *movedPiece = a;
         *movedUp = prevLoc[a] / xsize - curLoc[a] / xsize;
         *movedRight = curLoc[a] % xsize - prevLoc[a] % xsize;
         return;
         }
      }
   }
