
// #include <boost/array.hpp>
#include <boost/thread.hpp>
// #include <boost/asio.hpp>
#include <boost/date_time.hpp>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


const int MAXDATASIZE = 1024;


typedef struct Buffer_t{
  bool busy;
  int id;
  pthread_mutex_t* mutex;
  pthread_cond_t* cond;
}Buffer;

void play_video_func(){
  boost::posix_time::seconds workTime(1);
  std::cout << "Play thread: playing video " << std::endl;
  // Pretend to do something useful...
  boost::this_thread::sleep(workTime);
  std::cout << "Play thread: finished" << std::endl;
}

void network_thread(){
  // std::cout<<"reached here1"<<std::endl;
  

}

void* display_thread(void *ptr){
  return NULL;
}

void connect_to_host(char *hostname, char *port){
    
}

int main(int argc, char* argv[])
{
    network_thread();
    return 0;
}