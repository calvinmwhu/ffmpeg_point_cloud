CC = g++ -std=c++11
LIB = -lavcodec -lavformat -lavfilter -lavutil -lswresample -lswscale -framework GLUT -framework OpenGL -framework Cocoa
FLAG = -c -Wno-deprecated
CFLAG = -I/usr/local/Cellar/boost/1.57.0/include
LIBFLAG = -L/usr/local/Cellar/boost/1.57.0/lib  

all: merge

merge: merge.o
	$(CC) $(LIB) merge.o -o merge 

merge.o: merge.cpp
	$(CC) $(FLAG) merge.cpp

clean:
	rm -rf *.o merge outputs/*


	