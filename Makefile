CPPFLAGS = -O2
H_FILES = readParm.h js2.h showWin.h bufferTool.h taskManager.h taskQueue.h bitset.h
OBJECTS = readParm.o js2.o showWin.o init_code.o bufferTool.o taskManager.o
all: js2


%.o : %.cpp $(H_FILES) Makefile
	g++ -O2 -g -c -Wall -Werror -o $@ $< $(CPPFLAGS)


js2 : $(OBJECTS) 
	g++ -g -Wall -Werror  -o $@ $^ -lpthread

clean :
	rm *.o js2
