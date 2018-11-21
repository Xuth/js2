#ifndef js2_h
#define js2_h

#include "bufferTool.h"

#include <stdint.h>
#include <time.h>
#include <vector>
#include <thread>

// things to get rid of
#define MAX_PIECE 250
#define LARGEST_BOARD_SIZE 2048
#define WALL_CHAR 255
#define SPACE_CHAR 254

struct WallList_s {
   int numWall;
   int wallX[LARGEST_BOARD_SIZE];
   int wallY[LARGEST_BOARD_SIZE];
   };
typedef struct WallList_s WallList;
extern WallList wallList;

struct WinCond_s {
   int pieceType;
   int iPieceType;
   int posX;
   int posY;
   int posXY;
   };
typedef struct WinCond_s WinCond;

struct WinList_s {
   int numWinCond;
   WinCond winCond[MAX_PIECE];
   };
typedef struct WinList_s WinList;
extern WinList winList;





// type that is large enough to hold a piece type
// there is currently a fair bit of code that depends on this being a single char
// right now.  I don't think it would be that difficult to fix but that's where we are.
typedef uint8_t PieceTypeType;

// type that is large enough to hold a piece id (not just the type)
typedef uint8_t PieceIdType;

// the type of int that is large enough to hold position info.  this can be negative at times
typedef int16_t PosType;

// the type of int that is large enough to hold the compressed board length
typedef uint16_t CompressLenType;

#define MOVE_RIGHT  0
#define MOVE_LEFT   1
#define MOVE_DOWN   2
#define MOVE_UP     3
extern PosType DIR[4];

struct PieceTypeDef {
    PosType numBloc;
    PosType *xyoffset;
    PosType moveBlock[4]; /* number of blocks that need moved to move the piece in a given direction */
    PosType *moveTo[4];  /* blocks that become 'pieceType' when piece moves a given direction */
    PosType *moveFrom[4]; /* blocks that become a space when piece moves a given direction */
    PosType xstep;    /* ie if set, board can't be stored unless the net x or y */
    PosType ystep;    /* movement is a multiple of xstep or ystep */
    char moveX;  // does this piece move in the X direction
    char moveY;  // does this piece move in the Y direction
};
// used for malloc'ing xyoffset, moveTo, and moveFrom
#define POSTYPES_PER_BLOCK 9

struct PieceDef {
    PieceTypeType pType;
    char label;
};




/* the following are puzzle defining globals */
// most are filled in when reading parameters.
extern PieceTypeDef *pieceTypes;
extern PosType *posTypePool;
extern PieceDef *piece;
extern PosType *startLocs;   // starting locations for each piece


extern WinList winList;
extern WallList wallList;

extern int numPiece;
extern int numSpace;
extern int numType;
extern int bitsPerType;
extern int compressedPuzSize;
extern int xsize;
extern int ysize;
extern int startLoc;
extern int endLoc;
extern int totSize;
extern int concurrent;
extern int winOnly;
extern int hexPrint;
extern int useLetters;
extern int noGraph;
extern int useLabels;
extern int directed;
extern int cutoffStep;
extern int checkFullTree;
extern long int bigmem;
extern long int smallmem;
extern int numThreads;
extern unsigned int maxEntriesPerBuf;

#if 0
// no longer relevant
extern int bigStorage; // = 0;
extern int numWorkFiles;
#endif

extern PieceIdType *baseBoard;

extern time_t startTime;
extern PieceTypeType uncompressMask;

extern BufferManager bufMan;

extern std::vector<std::thread> threads;

// all of the CompressTool routines are very specific to the WorkerThread routines.
struct CompressTool {
    uint8_t *r;
    uint8_t *end;
    uint8_t *c;

    // reading
    void initBuf(uint8_t *buf, size_t len);
    uint8_t *getNext();

    // writing
    void initOutBuf(uint8_t *buf, size_t len);
    int compressDedupAdd(uint8_t *pos, int first);

    // compressing a buffer in place
    size_t compressDedupSortedBuf(uint8_t *buf, uint8_t *endBuf, unsigned int *positions);

};
// this is really hacky but I want the compress buffer on the stack
#define INIT_COMPRESSTOOL(ct) ct.c = (uint8_t *)alloca(compressedPuzSize)

struct ComprehensiveReadCompressTool {
    BufferId bId;
    uint8_t *r;
    uint8_t *end;
    uint8_t *c;
    char moreBuffers;
    char lastReturned;
    
    void start(BufferId inBId);
    uint8_t *getNext();
    void finish();
};
#define INIT_CRCT(crct) crct.c = (uint8_t *)alloca(compressedPuzSize)

struct BufferHeap {
    ComprehensiveReadCompressTool *crct;
    uint16_t *heap;
    uint16_t bufCount;  // this can go down as buffers are drained

    // I've been doing lots of things to keep stuff on the stack or in class
    // variables but this is potentially large enough that I can just put it
    // on its own in the heap.
    BufferHeap(int count);
    ~BufferHeap();
    void setBId(BufferId b, int slot);
    uint8_t *getFirst();
    uint8_t *getNext();
};


// with the simple compression scheme we use for the puzzles, the maximum increase in size of a sorted
// buffer is 256.  Save this amount at the start of the buffer for compression slack.
#define writePtrOffset 256

class WorkerThread {
    BufferId bId = BufferId(0,0,0,0,0);
    PieceIdType *workBoard;  // this is our game board that we look at
    PosType *curLoc;  // this is the current location of each piece
    uint8_t *placed;  // this is a bitset with 1 bit per piece

    uint8_t *posBuffer; // this is the buffer for collecting positions
    uint8_t *writePtr;  // this is the current working position within posBuffer

    // processBoard() "globals"
    PosType *locList;  // when investigating possible movements of a piece keep track
                       // of where it's been so we know when to stop processing it.
    PosType *nextLLItem;  // pointer into locList

    PieceTypeDef *pPtr;  // piece type information of piece we're currently looking at
    PieceIdType pieceNum; // the piece we're currently looking at
    PosType pStartLoc; // starting location of the piece we're looking at
    
public: 


    WorkerThread();
    ~WorkerThread();

    void run();

    void copyToCurLoc(PosType *locs);
    void initBoardFromLocs();
    void printBoard();
    void callRenumber();
    void uncompressBoard(uint8_t *compressed);
    void compressBoard();
    void insertBuffer();
    void setBId(BufferId id) { bId = id; }
    void processBuffer(BufferId inBId, uint64_t offset, uint64_t count, BufferId outBId);
    void printBuffer(BufferId inBId, uint64_t offset, uint64_t count);
    void printBuffer(BufferId inBId);

    // this is currently implemented as a list of the places a piece has been.
    // for any puzzle with lots of spaces this is certainly faster as a bitset.
    // for anything with only a few spaces but lots of board positions, clearing
    // the bitset becomes more expensive.
    // TODO: maybe find a compromise here?
    void ResetLocList() { nextLLItem = locList; }
    int AddLocList(PosType loc) {
	PosType *curLLItem = locList;
	for (; curLLItem < nextLLItem; ++curLLItem)
	    if (loc == *curLLItem)
		return 0;
	*(nextLLItem++) = loc;
	return 1;
    }
    void processBoard();
    void TryPiece_1();
    void TryMove_1(int offset);
    void TryPiece();
    void TryMove(int direction);
    void enqueueBoard();

    void mergeBuffers(BufferId *inBIds, int count, BufferId outBId);
    int checkWin(BufferId inBId, uint64_t offset, uint64_t count);
    int checkIfWin();
    void dedupGen(BufferId inBId, BufferId *genBIds, int genCount, BufferId outBId);
};

// effectively assert but prints a potentially useful string if the assert fails
void test(int t, const char *s);


void doInits();
char pieceNum2Char(int pn);

#endif
