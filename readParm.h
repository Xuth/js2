#ifndef readParm_h
#define readParm_h


// how many pieces to preallocate
#define PIECE_ALLOC 20

#define MAXSTRING 4096

struct IPieceDef {  // definition of the initial pieces
    // all of the int types in this should be normal 'int's so that they
    // can be read in with pointers.  These will be changed in PieceDef to
    // tighter types.
    int pieceType;
    int moveX;
    int moveY;
    int curLoc;
    int numBloc;
    int *xbloc;
    int *ybloc;
//    int *xyoffset;
//    int moveBlock[4]; /* number of blocks that need moved to move the piece in a given direction */
//    int *moveTo[4];  /* blocks that become 'pieceType' when piece moves a given direction */
//    int *moveFrom[4]; /* blocks that become a space when piece moves a given direction */
    int xstep;    /* ie if set, board can't be stored unless the net x or y */
    int ystep;    /* movement is a multiple of xstep or ystep */
    PieceIdType finalType; // what is this after it's been copied to PieceTypeDef
    char label;
    char used;
    
    IPieceDef() {
	moveX = 1;
	moveY = 1;
    }
};

struct IPieceLoc {
    int pieceType;
    int startX;
    int startY;
    char label;
};

int readParms(int argc, char *argv[]);

void initDirections();
int convert_iPiece(IPieceDef *iPiece, int numiPiece, int usediPiece,
		   IPieceLoc *iLoc, int numiLoc);
void calcMoveToFrom(PieceTypeDef *curType);

int EatString(char **sptr, const char *cstr, int critical);
int EatInt(char **sptr, int *iptr, int critical);
int EatPair(char **sptr, int *i1ptr, int *i2ptr, int critical);
int EatQuotedChar(char **sptr, char *cptr, int critical);
void validateIPiece(IPieceDef *iPiece);
void printPieceTypeDef(PieceTypeDef *ptd);

#endif


