CPPFLAGS = -O2 -g -Wall -Werror
H_FILES = readParm.h js2.h showWin.h bufferId.h bufferTool.h taskManager.h taskQueue.h bitset.h \
	bufferToolDisk.h bufIdAddOrderSet.h
OBJECTS = readParm.o js2.o showWin.o init_code.o bufferTool.o taskManager.o bufferId.o \
	bufferTool.o bufferToolDisk.o
all: js2


%.o : %.cpp $(H_FILES) Makefile
	g++ $(CPPFLAGS) -c -o $@ $< 


js2 : $(OBJECTS) 
	g++ -g -Wall -Werror  -o $@ $^ -lpthread

clean :
	rm *.o js2
