
#include <chrono>
#include <thread>
#include <vector>
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


const int MAXDATASIZE = 1000;
typedef std::vector<uint8_t> buffer_type;


typedef struct Buffer_t{
  int id;
  buffer_type data;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
}Buffer;

Buffer buffer[2];

void flip(int &current){
  if(current==0){
    current=1;
  }else{
    current=0;
  }
}

void recv_file(int sockfd, int buf_num){
  uint8_t buf[MAXDATASIZE];
  int numbytes;
  while((numbytes = recv(sockfd, buf, MAXDATASIZE, 0))!=0){
    if(numbytes==-1){
      perror("recv");
        exit(1);
    }  
    buffer[buf_num].data.insert(buffer[buf_num].data.end(), buf, buf+numbytes);   
    // if(numbytes<MAXDATASIZE){
    //   break;
    // }
    if(buffer[buf_num].data.size()>=600000){
      break;
    }       
  }
  printf("receive %lu bytes\n", buffer[buf_num].data.size());
}

void send_confm(int sockfd, int next_seq){
  char buf[10];
  memset(buf, 0, 10);
  sprintf(buf, "%d", next_seq);
  // printf("%lu\n", strlen(buf));
  send(sockfd, buf, strlen(buf), 0);
}

void* network_thread(void *ptr){
  int sockfd = *(int*)(ptr);
  int next = 0;

  for(int i=0; i<30; i++){
    pthread_mutex_lock(&buffer[next].mutex);
    while(buffer[next].data.size()!=0){
      pthread_cond_wait(&buffer[next].cond, &buffer[next].mutex);
    }
    recv_file(sockfd, next);
    printf("receiving video %d on buffer %d\n", i, next);

    pthread_cond_signal(&buffer[next].cond);
    pthread_mutex_unlock(&buffer[next].mutex);
    // send_confm(sockfd, i+1);
    flip(next);
  }
  return NULL;
}

void* display_thread(void *ptr){
  int next = 0;
  //there are 30 video clip to display (the client already knows this info)
  for(int i=0; i<30; i++){
    pthread_mutex_lock(&buffer[next].mutex);
    while(buffer[next].data.size()==0){
      pthread_cond_wait(&buffer[next].cond, &buffer[next].mutex);
    }
    // printf("in display_thread consuming file\n");
    printf("playing video %d on buffer %d\n", i, next);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    buffer[next].data.clear();
    pthread_cond_signal(&buffer[next].cond);
    pthread_mutex_unlock(&buffer[next].mutex);
    flip(next);
  }
  return NULL;
}

void* display_thread_no_buffer(void *ptr){
  int sockfd = *(int*)(ptr);
  int next = 0;
  char output_name[100];

  for(int i=0; i<30; i++){
    recv_file(sockfd, 0);
    printf("receive video %d\n", i);
    
    buffer[0].data.clear();
    send_confm(sockfd, i+1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  return NULL;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int connect_to_host(char *hostname, char *port){
  int sockfd, numbytes;  
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return -1;
  }

  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }
    printf("connecting to %s at port %s\n", hostname, port);
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return -1;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
  printf("client: connecting to %s\n", s);

  freeaddrinfo(servinfo); // all done with this structure

  return sockfd;
}

int main(int argc, char* argv[])
{
  char host[100];
  char port[100];
  if(argc!=3){
    sprintf(host, "%s", "192.168.0.101");
    sprintf(port, "%s", "8000"); 
  }else{
    sprintf(host, "%s", argv[1]);
    sprintf(port, "%s", argv[2]); 
  }

  //init buffer:
  for(int i=0; i<2; i++){
    buffer[i].id = i;
    pthread_mutex_init(&buffer[i].mutex, NULL);
    pthread_cond_init(&buffer[i].cond, NULL);
  }

  int sockfd = connect_to_host(host, port);
  pthread_t network_t;
  pthread_t display_t;

  pthread_create(&network_t, NULL, network_thread, (void*)(&sockfd));
  pthread_create(&display_t, NULL, display_thread, NULL);

  // pthread_create(&display_t, NULL, display_thread_no_buffer, (void*)(&sockfd));

  pthread_join(network_t, NULL);
  pthread_join(display_t, NULL);

  for(int i=0; i<2; i++){
    pthread_mutex_destroy(&buffer[i].mutex);
    pthread_cond_destroy(&buffer[i].cond);
  }

  return 0;
}