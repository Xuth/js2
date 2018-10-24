#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "js2.h"
#include "readParm.h"



int readParms(int argc, char *argv[]) {
    char defFileName[] = "puz.txt";
    char *fileName;
    FILE *infile;
    int numiPiece = 0;
    IPieceDef *iPiece;
    IPieceDef *curPiece;
    int pieceAlloc;
    int usediPiece = 0;
    int numiLoc = 0;
    IPieceLoc *iLoc;
    IPieceLoc *curLoc;
    int locAlloc;
    char instring[MAXSTRING];
    char *sptr;
    int pType;
    int pStep;
    int a, b;
    int numBloc = 0;
    int noConcurrent = 0;
    int numPieceWinCond = 0; /* used for determining whether concurrent solve possible */
    int line, offset1, offset2;
    int blockCount = 0;
    int parenCount;

    wallList.numWall = 0;
    winList.numWinCond = 0;


    if (argc > 1)
	fileName = argv[1];
    else
	fileName = defFileName;

    if (NULL == (infile = fopen(fileName, "r"))) {
	printf("Can't open %s\n", fileName);
	exit(1);
    }
    
    pieceAlloc = PIECE_ALLOC;
    if (NULL == (iPiece = (IPieceDef *)malloc(pieceAlloc * sizeof(IPieceDef)))) {
	printf("ReadParm - can't malloc initial iPiece\n");
	exit(1);
    }

    locAlloc = PIECE_ALLOC;
    if (NULL == (iLoc = (IPieceLoc *)malloc(locAlloc * sizeof(IPieceLoc)))) {
	printf("ReadParm - can't malloc initial iLoc\n");
	exit(1);
    }

    while (NULL != fgets(instring, MAXSTRING, infile)) {
	for (sptr = instring; *sptr && *sptr != ':'; ++sptr)
	    *sptr = toupper(*sptr);
	
	sptr = instring;
	
	if (EatString(&sptr, "XSIZE:", 0)) {
	    EatInt(&sptr, &xsize, 1);
	    xsize += 2;
	    continue;
	}
	
	else if (EatString(&sptr, "YSIZE:", 0)) {
	    EatInt(&sptr, &ysize, 1);
	    ysize += 2;
	    continue;
	}
	
	else if (EatString(&sptr, "PIECE:", 0)) {
	    if (numiPiece == pieceAlloc) {
		pieceAlloc += PIECE_ALLOC;
		if (NULL == (iPiece = (IPieceDef *)realloc(iPiece, pieceAlloc * sizeof(IPieceDef)))) {
		    printf("ReadParm - Can't realloc iPiece\n");
		    exit(1);
		}
	    }
	    curPiece = &iPiece[numiPiece++];
	    *curPiece = IPieceDef();  // run a constructor against the raw memory
	    EatInt(&sptr, &curPiece->pieceType, 1);
 
	    /* count the '(' so we can figure out how big an array to allocate */
	    parenCount = 0;
	    for (a = 0; sptr[a]; ++a)
		if (sptr[a] == '(')
		    ++parenCount;
	    if (NULL == (curPiece->xbloc = (int *) calloc(parenCount * 2, sizeof (int)))) {
		printf("ReadParm - Can't calloc for iPiece");
		exit(1);
	    }
	    curPiece->ybloc = &curPiece->xbloc[parenCount];
	    
	    while (EatPair(&sptr, &curPiece->xbloc[curPiece->numBloc],
			   &curPiece->ybloc[curPiece->numBloc], 0)) {
		++curPiece->numBloc;
		++blockCount;
            }

	    validateIPiece(curPiece);
	    continue;
	}
	
	else if (EatString(&sptr, "PUT:", 0)) {
	    EatInt(&sptr, &pType, 1);
	    for (a = 0; a < numiPiece; ++a)
		if (iPiece[a].pieceType == pType)
		    break;

	    if (a >= numiPiece) {
		printf("unknown piece type to be put\n");
		exit(1);
            }
	    curPiece = &iPiece[a];
	    if (!curPiece->used) {
		curPiece->used = 1;
		usediPiece++;
	    }

	    numBloc += curPiece->numBloc;
	    
	    if (numiLoc == locAlloc) {
		locAlloc += PIECE_ALLOC;
		if (NULL == (iLoc = (IPieceLoc *) realloc(iLoc, locAlloc * sizeof(IPieceLoc)))) {
		    printf("ReadParm - Can't realloc iLoc in put\n");
		    exit(1);
		}
	    }
	    curLoc = &iLoc[numiLoc++];
	    curLoc->pieceType = pType;
		
	    EatPair(&sptr, &curLoc->startX, &curLoc->startY, 1);
	    if (!EatQuotedChar(&sptr, &curLoc->label, 0))
		curLoc->label = 0;
	    continue;
	}

	else if (EatString(&sptr, "LOCKY:", 0)) {
	    EatInt(&sptr, &pType, 1);
	    for (a = 0; a < numiPiece; ++a)
		if (iPiece[a].pieceType == pType)
		    iPiece[a].moveY = 0;
	    continue;
	}

	else if (EatString(&sptr, "LOCKX:", 0)) {
	    EatInt(&sptr, &pType, 1);
	    for (a = 0; a < numiPiece; ++a)
		if (iPiece[a].pieceType == pType)
		    iPiece[a].moveX = 0;
	    continue;
	}

	else if (EatString(&sptr, "YSTEP:", 0)) {
	    EatInt(&sptr, &pType, 1);
	    EatInt(&sptr, &pStep, 1);
	    for (a = 0; a < numiPiece; ++a)
		if (iPiece[a].pieceType == pType)
		    iPiece[a].ystep = pStep;
	    continue;
	}

	else if (EatString(&sptr, "XSTEP:", 0)) {
	    EatInt(&sptr, &pType, 1);
	    EatInt(&sptr, &pStep, 1);
	    for (a = 0; a < numiPiece; ++a)
		if (iPiece[a].pieceType == pType)
		    iPiece[a].xstep = pStep;
	    continue;
	}

	else if (EatString(&sptr, "WALL:", 0)) {
	    EatPair(&sptr, &wallList.wallX[wallList.numWall],
		    &wallList.wallY[wallList.numWall], 1);
	    ++wallList.numWall;
	    continue;
	}
	
	else if (EatString(&sptr, "HWALL:", 0)) {
	    EatInt(&sptr, &line, 1);
	    EatPair(&sptr, &offset1, &offset2, 1);
	    if (offset1 > offset2) {
		a = offset1;
		offset1 = offset2;
		offset2 = a;
            }
	    for (a = offset1; a <= offset2; ++a) {
		wallList.wallX[wallList.numWall] = a;
		wallList.wallY[wallList.numWall++] = line;
            }
	}
	
	else if (EatString(&sptr, "VWALL:", 0)) {
	    EatInt(&sptr, &line, 1);
	    EatPair(&sptr, &offset1, &offset2, 1);
	    if (offset1 > offset2) {
		a = offset1;
		offset1 = offset2;
		offset2 = a;
            }
	    for (a = offset1; a <= offset2; ++a) {
		wallList.wallX[wallList.numWall] = line;
		wallList.wallY[wallList.numWall++] = a;
            }
	}

	else if (EatString(&sptr, "WIN:", 0)) {
	    EatInt(&sptr, &winList.winCond[winList.numWinCond].iPieceType, 1);
	    EatPair(&sptr, &winList.winCond[winList.numWinCond].posX,
		    &winList.winCond[winList.numWinCond].posY, 1);

	    if (winList.winCond[winList.numWinCond].iPieceType)
		++numPieceWinCond;
	    else
		winList.winCond[winList.numWinCond].pieceType = 0;
	    ++winList.numWinCond;
	    continue;
	}
	
	else if (EatString(&sptr, "NOCONCURRENT:", 0)) {
	    noConcurrent = 1;
	}
	else if (EatString(&sptr, "WINONLY:", 0)) {
	    winOnly = 1;
	}
	else if (EatString(&sptr, "DIRECTED:", 0)) {
	    EatInt(&sptr, &directed, 1);
	}
	else if (EatString(&sptr, "CUTOFF:", 0)) {
	    EatInt(&sptr, &cutoffStep, 1);
	}
	else if (EatString(&sptr, "CHECKFULLTREE:", 0)) {
	    checkFullTree = 1;
	}
	else if (EatString(&sptr, "USELETTERS:", 0)) {
	    useLetters = 1;
	}
	else if (EatString(&sptr, "NOGRAPH:", 0)) {
	    noGraph = 1;
	}
	else if (EatString(&sptr, "BIGMEM:", 0)) {
	    EatInt(&sptr, &bigmem, 1);
	}
	else if (EatString(&sptr, "SMALLMEM:", 0)) {
	    EatInt(&sptr, &smallmem, 1);
	}
	else if (EatString(&sptr, "NUMTHREADS:", 0)) {
	    EatInt(&sptr, &numThreads, 1);
	}
#if 0
	else if (EatString(&sptr, "USELABELS:", 0)) {
	    useLabels = 1;
	}
	else if (EatString(&sptr, "NUMWORKFILES:", 0)) {
	    EatInt(&sptr, &numWorkFiles, 1);
	}
	else if (EatString(&sptr, "BIGSTORAGE:", 0)) {
	    bigStorage = 1;
	}
#endif
    }
   
    fclose(infile);

    if (!xsize) {
	printf("xsize undefined\n");
	exit(1);
    }
    if (!ysize) {
	printf("ysize undefined\n");
	exit(1);
    }

    initDirections();
    convert_iPiece(iPiece, numiPiece, usediPiece, iLoc, numiLoc);








    startLoc = xsize + 1;
    endLoc = (ysize - 1) * xsize - 1;
    totSize = xsize * ysize;
    numSpace = (xsize - 2) * (ysize - 2) - numBloc - wallList.numWall;
    bitsPerType = 1;
    b = 2;
    for (a = 0; a < 32; ++a) {
	if (b > numType)
	    break;
	b = b * 2;
	++bitsPerType;
    }

    compressedPuzSize = (bitsPerType * (numPiece + numSpace)) / 8;
    if ((bitsPerType * (numPiece + numSpace)) % 8)
	++compressedPuzSize;

    maxEntriesPerBuf = smallmem / compressedPuzSize;
   
    for (a = 0; a < winList.numWinCond; ++a) {
	winList.winCond[a].posXY = (winList.winCond[a].posY + 1) 
	    * xsize + winList.winCond[a].posX + 1;
    }


    if (numPieceWinCond == numPiece && !noConcurrent)
	concurrent = 1;
    else
	concurrent = 0;


    if (concurrent && directed) {
	printf("I don't currently support concurrent directed searches\n");
	printf("add 'NOCONCURRENT:' to the config file\n");
	exit(0);
    }

    printf("numPiece - %d\n", numPiece);
    printf("numSpace - %d\n", numSpace);
    printf("numType - %d\n", numType);
    printf("bitsPerType - %d\n", bitsPerType);
    printf("compressedPuzSize - %d\n", compressedPuzSize);
    printf("xsize - %d\n", xsize);
    printf("ysize - %d\n", ysize);
#ifdef DEBUG_INFO
    printf("startLoc - %d\n", startLoc);
    printf("endLoc - %d\n", endLoc);
    printf("totSize - %d\n", totSize);
    printf("concurrent - %d\n", concurrent);
    printf("maxNewPos - %d\n", maxNewPos);
    printf("maxEntriesPerSl - %d\n", maxEntriesPerSl);
#endif













    
    return 1;
}

void initDirections() {
    DIR[MOVE_RIGHT] = 1;
    DIR[MOVE_LEFT] = -1;
    DIR[MOVE_DOWN] = xsize;
    DIR[MOVE_UP] = -xsize;
}


void validateIPiece(IPieceDef *iPiece) {
    // TODO: validateIPiece
    // check that 0,0 exists
    // check that 0,0 is the left edge of the top row of the other pieces
}

int convert_iPiece(IPieceDef *iPiece, int numiPiece, int usediPiece,
		   IPieceLoc *iLoc, int numiLoc) {
    int defBlockCount = 0;
    for (int i = 0; i < numiPiece; ++i) {
	IPieceDef *cur = &iPiece[i];
	if (!cur->used)
	    continue;
	defBlockCount += cur->numBloc;
    }

    // piece type definitions, piece definitions, and the block definititions
    // within them should all be as close together as possible.
    size_t pieceTypeMem = usediPiece * sizeof(PieceTypeDef);
    size_t mem = pieceTypeMem;
    size_t pieceDefMem = numiLoc * sizeof(PieceDef);
    mem += pieceDefMem;
    mem += defBlockCount * POSTYPES_PER_BLOCK * sizeof(PosType);
    char *pieceMem = (char *)malloc(mem);
    if (NULL == pieceMem) {
	printf("ReadParm - Can't malloc piece definitions\n");
	exit(1);
    }
    pieceTypes = (PieceTypeDef *) pieceMem;
    piece = (PieceDef *) &pieceMem[pieceTypeMem];
    posTypePool = (PosType *) &pieceMem[pieceTypeMem + pieceDefMem];


    if (NULL == (startLocs = (PosType *)malloc(numiLoc * sizeof(PosType)))) {
	printf("ReadParm - Can't malloc start locations\n");
	exit(1);
    }

    numType = usediPiece;
    numPiece = numiLoc;
    
    PieceTypeDef *curType =  pieceTypes;
    for (int i = 0; i < numiPiece; ++i) {
	IPieceDef *curIP = &iPiece[i];
	if (!curIP->used)
	    continue;
	curType->numBloc = curIP->numBloc;
	curType->xyoffset = posTypePool;
	posTypePool += curType->numBloc;
	for (int b = 0; b < curType->numBloc; ++b)
	    curType->xyoffset[b] = curIP->xbloc[b] + curIP->ybloc[b] * xsize;
	// can't set moveBlock, moveTo, or moveFrom yet
	curType->xstep = curIP->xstep;
	curType->ystep = curIP->ystep;
	curType->moveX = curIP->moveX;
	curType->moveY = curIP->moveY;

	calcMoveToFrom(curType);
	
	// set finalType so that we can do conversions for PieceDef
	curIP->finalType = curType - pieceTypes;

	// convert type for win conditions
	for (int j = 0; j < winList.numWinCond; ++j)
	    if (winList.winCond[j].iPieceType == curIP->pieceType)
		winList.winCond[j].pieceType = curIP->finalType;

	//printPieceTypeDef(curType);

	++curType;
    }

    PieceDef *curPiece = piece;
    PosType *curLoc = startLocs;
    for (int i = 0; i < numiLoc; ++i) {
	IPieceLoc *curIPL = &iLoc[i];
	for (int j = 0; j < numiPiece; ++j) {
	    if (iPiece[j].pieceType == curIPL->pieceType) {
		curPiece->pType = iPiece[j].finalType;
		break;
	    }
	}
	curPiece->label = curIPL->label;
	*curLoc = curIPL->startX + curIPL->startY * xsize + xsize + 1;
	++curPiece;
	++curLoc;
    }
    return 1;
}	
    
void calcMoveToFrom(PieceTypeDef *curType) {
    for (int dir = 0; dir < 4; ++dir) {
	// first count how many blocks we need to move for the purposes
	// of allocation

	curType->moveBlock[dir] = 0;
	for (int i = 0; i < curType->numBloc; ++i) {
	    int j;
	    for (j = 0; j < curType->numBloc; ++j)
		if (curType->xyoffset[i] + DIR[dir] == curType->xyoffset[j])
		    break;
	    if (j < curType->numBloc)  // if this block moves to place where another
		continue;              // block from this piece already exists
	                               // do nothing
	    // if we didn't find a match this one needs to be recorded
	    ++curType->moveBlock[dir];
	}

	// now actually record them
	curType->moveTo[dir] = posTypePool;
	posTypePool += curType->moveBlock[dir];
	curType->moveFrom[dir] = posTypePool;
	posTypePool += curType->moveBlock[dir];

	int countTo = 0;
	int countFrom = 0;
	for (int i = 0; i < curType->numBloc; ++i) {
	    int j;
	    for (j = 0; j < curType->numBloc; ++j)
		if (curType->xyoffset[i] + DIR[dir] == curType->xyoffset[j])
		    break;
	    if (j < curType->numBloc)
		continue;
	    curType->moveTo[dir][countTo++] = curType->xyoffset[i] + DIR[dir];
	}
	for (int i = 0; i < curType->numBloc; ++i) {
	    int j;
	    for (j = 0; j < curType->numBloc; ++j)
		if (curType->xyoffset[i] - DIR[dir] == curType->xyoffset[j])
		    break;
	    if (j < curType->numBloc)
		continue;
	    curType->moveFrom[dir][countFrom++] = curType->xyoffset[i];
	}

	assert(countTo == curType->moveBlock[dir]);
	assert(countFrom == curType->moveBlock[dir]);
    }
}
	

void printPieceTypeDef(PieceTypeDef *ptd) {
    printf("numBloc: %d\n", ptd->numBloc);
    printf("xyoffset:");
    for (int i = 0; i < ptd->numBloc; ++i)
	printf(" %d", ptd->xyoffset[i]);
    printf("\n");
    for (int d = 0; d < 4; ++d) {
	printf("moveTo[%d]:", d);
	for (int i = 0; i < ptd->moveBlock[d]; ++i)
	    printf(" %d", ptd->moveTo[d][i]);
	printf("\nmoveFrom[%d]:", d);
	for (int i = 0; i < ptd->moveBlock[d]; ++i)
	    printf(" %d", ptd->moveFrom[d][i]);
	printf("\n");
    }
    printf("xstep: %d\n", ptd->xstep);
    printf("ystep: %d\n", ptd->ystep);
    printf("moveX: %d\n", ptd->moveX);
    printf("moveY: %d\n", ptd->moveY);
}
	
	
    


int EatString(char **sptr, const char *cstr, int critical) {
    char *cptr = *sptr;
    int len = strlen(cstr);
    
    
    for (cptr = *sptr; *cptr && (*cptr <= ' '); ++cptr);
    
    if (!strncmp(cstr, cptr, len)) {
	*sptr = cptr + len;
	return 1;
    }
    if (critical) {
	printf("Expected %s\n", cstr);
	exit(1);
    }
    return 0;
}

int EatInt(char **sptr, int *iptr, int critical) {
    char *cptr;
    
    for (cptr = *sptr; *cptr && (*cptr <= ' '); ++cptr);
    
    *iptr = atoi(cptr);
    if (*cptr == '-')
	++cptr;
    
    if (!isdigit(*cptr)) {
	if (critical) {
	    printf("Expected a numeric at %s\n", cptr);
	    exit(1);
	}
	else
	    return 0;
    }

    for (; isdigit(*cptr); ++cptr);
    *sptr = cptr;
    return 1;
}

int EatQuotedChar(char **sptr, char *cptr, int critical) {
    char *tptr = *sptr;
    if (EatString(&tptr, "'", critical)) {
	*cptr = *(tptr++);
	if (EatString(&tptr, "'", critical)) {
	    *sptr = tptr;
	    return 1;
	}
    }
    return 0;
}

int EatPair(char **sptr, int *i1ptr, int *i2ptr, int critical) {
    char *tptr = *sptr;
    
    if (EatString(&tptr, "(", critical))
	if (EatInt(&tptr, i1ptr, critical))
	    if (EatString(&tptr, ",", critical))
		if (EatInt(&tptr, i2ptr, critical))
		    if (EatString(&tptr, ")", critical)) {
			*sptr = tptr;
			return 1;
		    }
    return 0;
}
