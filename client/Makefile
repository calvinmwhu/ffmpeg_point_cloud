CC = g++ -std=c++11

CFLAG = -I/usr/local/Cellar/boost/1.57.0/include
LIBFLAG = -L/usr/local/Cellar/boost/1.57.0/lib  

all: client

client: client.o
	$(CC) -lpthread client.o -o client  

client.o: client.cpp
	$(CC) -c client.cpp

clean:
	rm -rf *.o client video_clips/*