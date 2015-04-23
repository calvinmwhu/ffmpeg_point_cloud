CC = gcc
LIB = -lavcodec -lavformat -lavfilter -lavutil -lswresample -lswscale -lGL -lGLU -lglut -lGLEW
FLAG = -c -Wno-deprecated
all: merge

merge: merge.o
	$(CC) merge.o -o merge $(LIB)

merge.o: merge.c
	$(CC) $(FLAG) merge.c

clean:
	rm -rf *.o merge output.mpg