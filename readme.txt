js2 v. 1.5
copyright Jim Leonard (Xuth) 2018-2019
jim@xuth.net

This is a rewrite of my sliding block puzzle solver (jim'slide) that I started writing over two decades ago.  My original code was extremely fast but difficult to work on or update.  Some aspects I was really proud of and others significantly less so.  My priorities for this version are to have similar functionality and be easier to work with while being built for modern architectures (multithreaded, assume more RAM, more linear memory accesses) and being at least as fast as the original codebase in the single thread case.

Machine requirements:
Nearly any reasonably modern machine will work.  If disk space needs to be used to hold puzzle positions then js2 will want to create a directory js2Cache in the working directory.  This can be deleted after the solver has run.

If you have any questions or comments, please feel free to email me at
       jim@xuth.net
I really would like feedback, if nothing else just to say you downloaded it.  I will respond to messages as time permits (I'll try to respond to everything eventually but I don't want to make any promises).

To run it:
first create a config file to define the puzzle (more on that later and with this should be a few examples).
For this example let's call the config file puz.txt (which is the default config file name if none is supplied).

enter at command line terminal:
jimslide puz.txt

or to save the solution to a file (say puz.sol) enter:
jimslide puz.txt > puz.sol


The config file:
first off any line that starts with a '!' or a '#' is considered a comment.  If the line doesn't start with a valid command or a comment token will generate an error.

There are two data types used - integers and paired integers.  Paired integers are expressed (without the quotes) as "(x, y)" ie (5, 6).

To help explain the config file we are going to use the following common puzzle (L'Ane Rouge?) as an example (where the object is to move the large square to the bottom center where the four small squares are) and then afterwards we'll give a complete reference list and description of the command set.

+--+-----+--+
|  |     |  |
|  |     |  |
|  |     |  |
+--+-----+--+
|  |     |  |
|  +--+--+  |
|  |  |  |  |
+--+--+--+--+
   |  |  |
   +--+--+

first we need to specify the dimensions of the board
set horizontal size of puzzle to 4 unit squares
	xsize: 4
set vertical size of puzzle to 5 unit squares
	ysize: 5
since the puzzle is rectangular there's no need to define any internal walls.

Then we need to define the piece set:
	piece: 1 (0,0) (1,0) (0,1) (1,1) ! (2x2 square)
ie define piece type '1' to be made of of four blocks at relative positions (0,0), (1,0), (0,1) and (1,1).
	piece: 2 (0,0) (1,0)             ! (horizontal rectangle)
	piece: 3 (0,0) (0,1)             ! (vertical rectangle)
	piece: 4 (0,0)                   ! (1 block square)
All coordinates are 0 based.  Each piece must start with position (0,0) and that position must be the left most square of the uppermost row of squares.

We then place one or more of each type of piece on the board using the 'put:' command.
	put: 3 (0,0)
ie put a piece of type '3' at position (0,0).
	put: 1 (1,0)
	put: 3 (3,0)
	put: 3 (0,2)
	put: 2 (1,2)
	put: 3 (3,2)
	put: 4 (1,3)
	put: 4 (2,3)
	put: 4 (1,4)
	put: 4 (2,4)

Now we define the winning condition(s)
	win: 1 (1,3)
ie to win there must be a piece of type '1' based at position (1,3).  All win conditions must be met for the puzzle to be considered solved.

All that's left to define the puzzle is to specify the computing constraints that the solver will work in.  First let's set the number of compute threads.  My notebook has 4 cores, so I generally run with 3 compute threads so I still have a peppy computer to work with while js2 is working.
    	numthreads: 3
By default there is one additional thread that writes buffers to disk and until you get to significantly more compute threads, 1 is plenty.
Next we need to specify that js2 will be allowed to use.  There are two memory parameters and they're poorly named having just been repurposed from the original jimslide code.  'bigmem:' is the amount of memory that is allowed to be held by the various buffers of position data.  This is the bulk of the memory used by js2 but the various metadata to keep track of the buffers is not kept in this accounting so you shouldn't set it to all of the memory available.  On a very large puzzle, it is helpful to give js2 as much memory as you can spare.  On my 8GB laptop I might give it 4-6GB depending on what else I'm doing.  However this is a very small puzzle and we can give it much less.  
        bigmem: 1000000000               ! even this is gross overkill for this puzzle
next is the buffer size.  This is the maximum size a buffer of positions can be.  Certain operations within js2 can use up to 9 buffers so the rule of thumb I use is that smallmem should be no more than bigmem / number of threads / 10.
     	smallmem: 1000000
	
Save the config file and run the program.





Full Command Set Reference

A list of the keywords
  xsize: n
  ysize: n
  wall: (x,y)
  hwall: y (x1, x2)
  vwall: x (y1, y2)
  piece: n (x1, y1) [(x2, y2)...(x~, y~)]
  lockx: n
  locky: n
  xstep: n1 n2
  ystep: n1 n2
  put: n (x, y) ['c']
  win: n (x, y)

  useletters:
  hexprint:
  winonly:
  bigmem: n
  smallmem: n
  numthreads: n
  diskthreads: n
  cachepercent: n

The following exist in jimslide that are features that are not yet implemented in js2
  nograph:
  noconcurrent:
  cutoff: n
  directed: n
  checkfulltree:

Each command is defined in the following format:
syntax
	definition
example

and the command set is broken up into three sections:
	defining puzzle dimensions
	defining puzzle pieces
	and solver flags and parameters

Defining Puzzle Dimensions
xsize: n
	set horizontal size of puzzle to n units
xsize: 4

ysize: n
	set vertical size of puzzle to n units
ysize: 5

wall: (x,y)
	adds a 1 unit square wall inside the puzzle boundaries
wall: (3,4)


hwall: y (x1, x2)
	adds a horizontal wall on row y from column x1 to x2

vwall: x (y1, y2)
	adds a vertical wall on column x from row y1 to y2


Defining Puzzle Pieces

piece: n (x1, y1) [(x2, y2)...]
	define piece type n to be comprised of unit squares offset at
	(x1,y1), (x2,y2), ... The piece labels can be any integer, but
	don't use 0 as it is reserved for empty spaces.
piece: 1 (0,0) (1,0) (0,1) (1,1)

lockx: n
locky: n
	constrain a piece (n) horizontally (lockx:) or vertically (locky:)
	This is useful for things like Nob's "Rush Hour" (or also when a
	piece just isn't able to move in a direction this will save some
	processing time)

xstep: n1 n2
ystep: n1 n2
	If set for a piece of type n1, a position is only stored if the 
	net movement of that piece in the x (or y for ystep:) direction 
	of that piece is a multiple of n2 spaces.


put: n (x, y)
 - or -
put: n (x, y) 'c'
	place a piece of type n at location x,y.  Multiple instances of
	the same type of piece may be placed making them interchangable
	for win conditions.  You may optionally put a single character
	in single quotes so that when that piece is displayed it is
	displayed as 'c' instead of an arbitrary character.
put: 3 (3,0) 'T'

win: n (x, y)
	to win, a piece of type n must be based off of (x,y) or if
	n = 0 (x,y) must be an open space.  If multiple win conditions are
	specified they must all be satisfied.  There currently are
	no provisions for 'or'ing conditions.
win: 1 (1,4)

nograph:
	< not yet implemented >
	when the winning move sequence is displayed, don't print the
	intermediate board positions, just the text description of
	what happened.

useletters:
	use letters instead of numbers for the default piece labels.

hexprint:
	use two digit hex values instead of numbers for the piece labels.
	This is good for puzzles with many pieces.


Solver Flags and Parameters

bigmem: n
	This is the amount of ram that can be used to hold buffers of
	positions.  At minimum this needs to be large enough to hold
	all of the buffers that the worker threads are using at any
	given time (some operations require a single worker thread to
	use up to 9 separate buffers).  For very large puzzles this
	should be as much RAM as you can spare.  Note that there is
	bookkeeping data that is not accounted for so bigmem shouldn't
	be set to all available RAM.
bigmem: 4000000000

smallmem: n
	This is the maximum size of a memory buffer.
smallmem: 10000000

numthreads: n
	number of worker threads doing calculations.
numthreads: 3

diskthreads: n
	number of threads that are tasked with writing buffers to disk
	as bigmem fills up.  The default of 1 is plenty until you get
	to more than a few threads or you are working on a slow/network
	disk.  The diskthreads only start moving buffers to disk once
	the available percentage of bigmem drops below a certain
	threshold and then the diskthreads work to maintain that
	percentage of available memory.
diskthreads: 1

cachepercent: n
	this is the percentage of bigmem that the disk threads try to
	keep available.  The default is 50 but a lower value might
	be warranted on a large memory machine.



noconcurrent:
	< not yet implemented >
	this will keep the program from attempting to solve the puzzle
	from both ends despite what it thinks it can do.  There are
	times when this is faster.

winonly:
	this will cause the program to only search for the solution
	and not save the entire move tree.  Useful to find the complete
	solution so that you can run a concurrent search later.  Used
	when you don't have the disk space.

cutoff: n
	< not yet implemented >
	abort the solve after n generations (moves).

directed: n
	< not yet implemented >
	performs a partial width search using only the best n
	positions in a generation as defined by a fitness function.
	The current fitness function is the sum of the size of each
	piece multiplied by the square of the horizontal and vertical 
	distances from it's win condition.  This function is rather
	arbitrary and could probably be improved upon.  If you have
	a C compiler this can be modified in the functions Distance()
	and CDistance().

checkfulltree:
	< not yet implemented >
	perform duplicate checks on the entire move tree instead of
	just the current and the previous two generations.  There is
	no reason to use this except while performing a directed
	search.


example puzzles:
************************
! The above example, (L'Ane Rouge)
xsize: 4
ysize: 5
piece: 1 (0,0) (1,0) (0,1) (1,1)
piece: 2 (0,0) (1,0)
piece: 3 (0,0) (0,1)
piece: 4 (0,0)
put: 3 (0,0)
put: 1 (1,0)
put: 3 (3,0)
put: 3 (0,2)
put: 2 (1,2)
put: 3 (3,2)
put: 4 (1,3)
put: 4 (2,3)
put: 4 (1,4)
put: 4 (2,4)
win: 1 (1,3)
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
************************
! rush hour puzzle #1
! up thru the put's same for all rh's
xsize: 6
ysize: 6
piece: 1 (0,0) (1,0) ! the red car
piece: 2 (0,0) (1,0) ! horizontal cars
piece: 3 (0,0) (0,1) ! vertical cars
piece: 4 (0,0) (1,0) (2,0) ! horizontal trucks
piece: 5 (0,0) (0,1) (0,2) ! vertical trucks
locky: 1
locky: 2
lockx: 3
locky: 4
lockx: 5
win: 1 (4,2)
! now we're at the puzzle specific code
put: 1 (1,2)
put: 2 (0,0)
put: 2 (4,4)
put: 3 (0,4)
put: 4 (2,5)
put: 5 (5,0)
put: 5 (0,1)
put: 5 (3,1)
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
****************************
! Nob's Tzer:
xsize: 6
ysize: 5
piece: 1 (0,0) (-1, 1) (0, 1) (1, 1);
piece: 2 (0,0) (-1, 1) (0, 1) (1, 1);
piece: 3 (0,0) (1, 0);
piece: 4 (0,0);
put: 1 (1,0)
put: 2 (4,0)
put: 3 (0,3)
put: 3 (2,3)
put: 3 (4,3)
put: 3 (2,4)
put: 4 (0,4)
put: 4 (1,4)
put: 4 (4,4)
put: 4 (5,4)
wall: (0,0)
wall: (2,0)
wall: (3,0)
wall: (5,0)
win: 1 (4,0)
win: 2 (1,0)
win: 3 (0,3)
win: 3 (2,3)
win: 3 (4,3)
win: 3 (2,4)
win: 4 (0,4)
win: 4 (1,4)
win: 4 (4,4)
win: 4 (5,4)
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
**************************
! Nob's Ultimate Tzer
xsize: 6
ysize: 5
piece: 1 (0,0) (-1, 1) (0, 1) (1, 1);
piece: 2 (0,0) (-1, 1) (0, 1) (1, 1);
piece: 3 (0,0) (1, 0);
piece: 4 (0,0);
put: 1 (1,0)
put: 2 (4,0)
put: 3 (2,2)
put: 3 (0,3)
put: 3 (2,3)
put: 3 (4,3)
put: 4 (2,4)
put: 4 (3,4)
put: 4 (0,4)
put: 4 (1,4)
put: 4 (4,4)
put: 4 (5,4)
wall: (0,0)
wall: (2,0)
wall: (3,0)
wall: (5,0)
win: 1 (4,0)
win: 2 (1,0)
win: 3 (2,2)
win: 3 (0,3)
win: 3 (2,3)
win: 3 (4,3)
win: 4 (2,4)
win: 4 (3,4)
win: 4 (0,4)
win: 4 (1,4)
win: 4 (4,4)
win: 4 (5,4)
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
**************************
! Nob's Pink & Blue
! While the eye piece is larger in the real puzzle
! it does follow the same constraints using the
! locky: parameter, and causes the same constraints
! because of the larger middle slide area on the real
! puzzle.
!
xsize: 4
ysize: 8
wall: (0,2)
wall: (2,2)
wall: (3,2)
wall: (0,5)
wall: (1,5)
wall: (3,5)
piece: 1 (0,0) (0,1)
piece: 2 (0,0) (0,1)
piece: 3 (0,0) (0,1)
piece: 4 (0,0) (0,1)
piece: 5 (0,0) (0,1)
piece: 6 (0,0) (0,1)
piece: 7 (0,0) (0,1)
piece: 8 (0,0) (0,1)
piece: 9 (0,0) (0,1)
locky: 9
put: 1 (0,0)
put: 2 (1,0)
put: 3 (2,0)
put: 4 (3,0)
put: 5 (0,6)
put: 6 (1,6)
put: 7 (2,6)
put: 8 (3,6)
put: 9 (3,3)
win: 1 (0,6)
win: 2 (1,6)
win: 3 (2,6)
win: 4 (3,6)
win: 5 (0,0)
win: 6 (1,0)
win: 7 (2,0)
win: 8 (3,0)
win: 9 (3,3)
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
**************************
! Dirty Dozen #12
xsize: 6
ysize: 5
piece: 1 (0,0) (1,0) (0,1) (1,1)
piece: 2 (0,0) (1,0) (0,1)
piece: 3 (0,0) (-1,1) (0,1)
piece: 4 (0,0) (1,0)
piece: 5 (0,0) (0,1)
piece: 6 (0,0)
put: 1 (2,2)
put: 2 (1,0)
put: 3 (3,0)
put: 2 (4,0)
put: 5 (5,1)
put: 6 (0,2)
put: 6 (1,2)
put: 5 (4,2)
put: 6 (0,3)
put: 6 (1,3)
put: 3 (5,3)
put: 6 (0,4)
put: 6 (1,4)
put: 4 (2,4)
win: 1 (4,3)
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
**************************
! Kuroko and Dairu
bigmem: 32000000
smallmem: 8000000
!
xsize: 13
ysize: 3
wall: (2,1)
wall: (3,1)
piece: 1 (0,0) (1,0)
piece: 2 (0,0) (1,0) (2,0)
piece: 3 (0,0) (1,0) (2,0)
piece: 4 (0,0) (1,0) (2,0)
piece: 5 (0,0) (1,0)
piece: 6 (0,0) (1,0) (2,0)
piece: 7 (0,0) (1,0)
piece: 8 (0,0) (1,0) (2,0) (3,0)
piece: 9 (0,0) (0,1) (1,0) (1,1)
piece: 10 (0,0) (1,0) (-2,1) (-1,1) (0,1) (1,1)
lockx: 9
put: 1 (0,0)
put: 2 (2,0)
put: 3 (5,0)
put: 4 (8,0)
put: 5 (2,2)
put: 6 (4,2)
put: 7 (7,2)
put: 8 (9,2)
put: 9 (0,1)
put: 10 (11,0)
win: 1 (2,2)
win: 2 (4,2)
win: 3 (7,2)
win: 4 (10,2)
win: 5 (0,0)
win: 6 (2,0)
win: 7 (5,0)
win: 8 (7,0)
win: 9 (0,1)
win: 10 (11,0)
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
***************************
!Mosaic C (pentomino puzzle)
!by Michael McKee
xsize: 9
ysize: 9
wall: (0,0)
wall: (8,8)
piece: 1 (0,0) (-1,1) (1,1) (0,2) (0,1) ! X
piece: 2 (0,0) (4,0) (1,0) (2,0) (3,0)  ! I
piece: 3 (0,0) (1,0) (-1,1) (0,1) (-1,2)! W
piece: 4 (0,0) (2,0) (-1,1) (0,1) (1,0) ! N
piece: 5 (0,0) (0,3) (1,1) (0,1) (0,2)  ! Y
piece: 6 (0,0) (1,0) (-1,2) (0,2) (0,1) ! Z
piece: 7 (0,0) (2,0) (1,2) (1,0) (1,1)  ! T
piece: 8 (0,0) (-2,2) (0,2) (0,1) (-1,2)! V
piece: 9 (0,0) (-1,2) (1,1) (0,2) (0,1) ! F
piece: 10 (0,0) (1,2) (1,0) (0,2) (1,1) ! U
piece: 11 (0,0) (-3,1) (0,1) (-2,1) (-1,1)! L
piece: 12 (0,0) (2,0) (0,1) (1,1) (1,0) ! P
put: 1 (1,0) 'X'
put: 2 (2,0) 'I'
put: 3 (3,1) 'W'
put: 4 (5,1) 'N'
put: 5 (0,2) 'Y'
put: 6 (6,2) 'Z'
put: 7 (3,3) 'T'
put: 8 (7,3) 'V'
put: 9 (1,4) 'F'
put: 10 (2,4) 'U'
put: 11 (4,6) 'L'
put: 12 (5,6) 'P'
win: 1 (7,6)
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
***************************
! Jerry Slocum puzzle (the really slow way)
! by Minoru Abe
!
xsize: 12
ysize: 3
piece: 1 (0, 0)(1, 0)
piece: 2 (0, 0)(1, 0)
piece: 3 (0, 0)(1, 0)
piece: 4 (0, 0)(1, 0)
piece: 5 (0, 0)(1, 0)
piece: 6 (0, 0)(1, 0)
piece: 7 (0, 0)(1, 0)(0, 1)(1, 1)
piece: 8 (0, 0)(1, 0)
piece: 9 (0, 0)(1, 0)
piece: 10 (0, 0)(1, 0)
piece: 11 (0, 0)(1, 0)
piece: 12 (0, 0)(1, 0)
put: 1 (0, 0) 'S'
put: 2 (2, 0) 'L'
put: 3 (4, 0) 'O'
put: 4 (6, 0) 'C'
put: 5 (8, 0) 'U'
put: 6 (10, 0) 'M'
put: 7 (10, 1) 'X'
put: 8 (0, 2) 'J'
put: 9 (2, 2) 'E'
put: 10 (4, 2) 'R'
put: 11 (6, 2) 'r'
put: 12 (8, 2) 'Y'
wall: (0, 1)
hwall: 1 (3, 5)
hwall: 1 (8, 9)
win: 1 (0, 2)
win: 2 (2, 2)
win: 3 (4, 2)
win: 4 (6, 2)
win: 5 (8, 2)
win: 6 (10, 2)
win: 7 (10, 0)
win: 8 (0, 0)
win: 9 (2, 0)
win: 10 (4, 0)
win: 11 (6, 0)
win: 12 (8, 0)
lockx: 7
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
***************************
! Jerry Slocum puzzle (faster)
! by Minoru Abe
!
! Warning, This puzzle can take several hours (or days) to
! solve.  It is included here for it's use of xstep and ystep.
! 
! The Jerry Slocum puzzle above gets tied down very quickly saving
! positions for half steps that you would never use.  Because there
! is a valid place to stop a piece on the half step, we cheat and move
! that space over and use xstep and ystep to constrain the what the
! solver considers a valid position.
!
xsize: 12
ysize: 5
piece: 1 (0, 0)(1, 0)
piece: 2 (0, 0)(1, 0)
piece: 3 (0, 0)(1, 0)
piece: 4 (0, 0)(1, 0)
piece: 5 (0, 0)(1, 0)
piece: 6 (0, 0)(1, 0)
piece: 7 (0, 0)(1, 0)(0, 1)(1, 1)(0, 2)(1, 2)(0, 3)(1, 3)
piece: 8 (0, 0)(1, 0)
piece: 9 (0, 0)(1, 0)
piece: 10 (0, 0)(1, 0)
piece: 11 (0, 0)(1, 0)
piece: 12 (0, 0)(1, 0)
put: 1 (0, 0) 'S'
put: 2 (2, 0) 'L'
put: 3 (4, 0) 'O'
put: 4 (6, 0) 'C'
put: 5 (8, 0) 'U'
put: 6 (10, 0) 'M'
put: 7 (10, 1) 'X'
put: 8 (0, 4) 'J'
put: 9 (2, 4) 'E'
put: 10 (4, 4) 'R'
put: 11 (6, 4) 'r'
put: 12 (8, 4) 'Y'
wall: (0, 1)
hwall: 1 (3, 5)
hwall: 2 (3, 5)
hwall: 3 (3, 5)
vwall: 8 (1, 3)
vwall: 9 (1, 3)
wall: (0, 3)
win: 1 (0, 4)
win: 2 (2, 4)
win: 3 (4, 4)
win: 4 (6, 4)
win: 5 (8, 4)
win: 6 (10, 4)
win: 7 (10, 0)
win: 8 (0, 0)
win: 9 (2, 0)
win: 10 (4, 0)
win: 11 (6, 0)
win: 12 (8, 0)
xstep: 1 2
xstep: 2 2
xstep: 3 2
xstep: 4 2
xstep: 5 2
xstep: 6 2
xstep: 8 2
xstep: 9 2
xstep: 10 2
xstep: 11 2
xstep: 12 2
ystep: 1 2
ystep: 2 2
ystep: 3 2
ystep: 4 2
ystep: 5 2
ystep: 6 2
ystep: 8 2
ystep: 9 2
ystep: 10 2
ystep: 11 2
ystep: 12 2
lockx: 7
numthreads: 3
bigmem: 1000000000
smallmem: 10000000
