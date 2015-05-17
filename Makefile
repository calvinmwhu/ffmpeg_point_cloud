CC = g++ -std=c++11
LIB = -lavcodec -lavformat -lavfilter -lavutil -lswresample -lswscale -framework GLUT -framework OpenGL -framework Cocoa
FLAG = -c -Wno-deprecated
all: merge

merge: merge.o
	$(CC) merge.o -o merge $(LIB)

merge.o: merge.cpp
	$(CC) $(FLAG) merge.cpp

clean:
	rm -rf *.o merge outputs/*