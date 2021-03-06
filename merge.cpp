extern "C" {
  // Get declaration for f(int i, char c, float x)
    #include <math.h>
    #include <stdio.h>
    #include <string.h>
    #include <limits.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <errno.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <sys/wait.h>
    #include <sys/time.h>
    #include <signal.h>
    #include <pthread.h>
    #include <libavutil/opt.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/common.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/samplefmt.h>
    #include <libswscale/swscale.h>
}

#include <iostream>
#include <vector>
#include <ctime>
#include <string>
#include <algorithm>    // std::min
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>


#define INBUF_SIZE 4096
#define WIDTH 640
#define HEIGHT 480
#define FPS 30
#define MAX_NUM_STREAM 4
const int BACKLOG = 10;
const int MAXDATASIZE = 1024;

typedef std::vector<uint8_t> buffer_type;
buffer_type output_video;

GLuint vboId; // Vertex buffer ID
GLuint cboId; // Color buffer ID

AVFrame *frames[MAX_NUM_STREAM][FPS];

int total_frames ;
float colorarray0[WIDTH*HEIGHT*3];
float vertexarray0[WIDTH*HEIGHT*3];
float colorarray1[WIDTH*HEIGHT*3];
float vertexarray1[WIDTH*HEIGHT*3];

float colorarray[WIDTH*HEIGHT*6];
float vertexarray[WIDTH*HEIGHT*6];

typedef struct RGBColor_t{
    uint8_t r;
    uint8_t g;
    uint8_t b;
}RGBColor;

typedef struct Decoder_t{
    int id;
    AVCodec *codec;
    AVCodecContext *c;
    int frame_count;
    FILE *f;
    char filename[100];
    AVFrame *frame;
    uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    AVPacket avpkt;
}Decoder;

typedef struct Encoder_t{
    AVCodec *codec;
    AVCodecContext *c;
    int number_frames;
    AVFrame *frame;
    AVPacket pkt;
    int fps;
}Encoder;


// void transformPointCloud(glm::vec4 &V, const float angle){
//     glm::mat4 transform = glm::mat4(1.0f);
//     transform = glm::rotate(transform, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    
//     V=transform*V;
// }

static void allocate_atx_and_frame_for_decoder(Decoder *d){
    d->c = avcodec_alloc_context3(d->codec);
    if(!d->c){
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    if(d->codec->capabilities&CODEC_CAP_TRUNCATED){
        d->c->flags|= CODEC_FLAG_TRUNCATED; 
    }
    if(avcodec_open2(d->c, d->codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    d->frame = av_frame_alloc();
    if (!d->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
}

static void allocate_atx_and_frame_for_encoder(Encoder *enc, AVCodecID codec_id){
    enc->c = avcodec_alloc_context3(enc->codec);
    if (!enc->c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    enc->c->bit_rate = 400000;
    // enc->c->bit_rate = 300000;

    /* resolution must be a multiple of two */
    enc->c->width = WIDTH;
    enc->c->height = HEIGHT;
    /* frames per second */
    enc->c->time_base = (AVRational){1,30};
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    enc->c->gop_size = 10;
    enc->c->max_b_frames = 1;
    enc->c->pix_fmt = AV_PIX_FMT_YUV420P;
    if (codec_id == AV_CODEC_ID_H264)
        av_opt_set(enc->c->priv_data, "preset", "slow", 0);
    /* open it */
    if (avcodec_open2(enc->c, enc->codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    enc->frame = av_frame_alloc();
    if (!enc->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    enc->frame->format = enc->c->pix_fmt;
    enc->frame->width  = enc->c->width;
    enc->frame->height = enc->c->height;
    /* the image can be allocated by any means and av_image_alloc() is
     * just the most convenient way if av_malloc() is to be used */
    int  ret = av_image_alloc(enc->frame->data, enc->frame->linesize, enc->c->width, enc->c->height, enc->c->pix_fmt, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
        exit(1);
    }
}

static void init_decoder(Decoder *dcrs){  
    for(int i=0;i<MAX_NUM_STREAM; i++){
        dcrs[i].id=i;
        /* find the mpeg1 video decoder */
        dcrs[i].codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
        if (!dcrs[i].codec) {
            fprintf(stderr, "Codec not found\n");
            exit(1);
        }
        av_init_packet(&dcrs[i].avpkt);
        memset(dcrs[i].inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);
        dcrs[i].frame_count = 0;
    }
}


static void init_encoder(Encoder *enc, AVCodecID codec_id, int fps){
    enc->codec = avcodec_find_encoder(codec_id);
    enc->fps = fps;
    if (!enc->codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    // allocate_atx_and_frame_for_encoder(enc, codec_id);
}

static void getRGB(RGBColor *pixel, AVFrame *frame, int x, int y){
    
    // Y component
    uint8_t Y = frame->data[0][frame->linesize[0]*y + x];

    // U, V components 
    x /= 2;
    y /= 2;
    uint8_t U = frame->data[1][frame->linesize[1]*y + x];
    uint8_t V = frame->data[2][frame->linesize[2]*y + x];

    // printf("%d  %d  %d\n", (int)Y, (int)U, (int)V );
    // RGB conversion
    pixel->r = Y + 1.402*(V-128);
    pixel->g = Y - 0.344*(U-128) - 0.714*(V-128);
    pixel->b = Y + 1.772*(U-128);
}

static int getColorAndCoordData(float* destColor, AVFrame *frameColor, float* destDepth, AVFrame *frameDepth){
    float *fdestColor = destColor;
    float *fdestDepth = destDepth;
    RGBColor pixel;
    memset(&pixel, 0, sizeof(RGBColor));
    int x,y;
    int num=0;
    for(y=0; y<frameColor->height; y++){
        for(x=0; x<frameColor->width; x++){
            getRGB(&pixel, frameColor, x ,y);
            // printf("%d  %d  %d\n", (int)pixel.r, (int)pixel.g, (int)pixel.b);
            if((int)pixel.r==13 && (int)pixel.g==237 && (int)pixel.b==13){
                continue;
            }          
            // printf("%d  %d  %d\n", (int)pixel.r, (int)pixel.g, (int)pixel.b);  
            num++;
            *fdestColor++ = ((float)pixel.r)/255.f;
            *fdestColor++ = ((float)pixel.g)/255.f;
            *fdestColor++ = ((float)pixel.b)/255.f;
            *fdestDepth++ = ((float)x)/frameColor->width;
            *fdestDepth++ = ((float)y)/frameColor->height;
            uint8_t Y = frameDepth->data[0][y*frameDepth->linesize[0]+x];
            *fdestDepth++ = Y/255.f;   
        }
    }
    return num;
}

static void getDataForFrame(AVFrame *colorFrame0, AVFrame *depthFrame0, AVFrame *colorFrame1, AVFrame *depthFrame1, int *num_points_0, int *num_points_1){
    *num_points_0 = getColorAndCoordData(colorarray0, colorFrame0, vertexarray0, depthFrame0);
    *num_points_1 = getColorAndCoordData(colorarray1, colorFrame1, vertexarray1, depthFrame1);
}

static GLubyte* render(AVFrame *colorFrame0, AVFrame *depthFrame0, AVFrame *colorFrame1, AVFrame *depthFrame1){
    int num_points_0 = 0;
    int num_points_1 = 0;

    getDataForFrame(colorFrame0, depthFrame0, colorFrame1, depthFrame1, &num_points_0, &num_points_1);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glColor3f (1.0, 1.0, 1.0);

    glBegin(GL_POINTS);
    for (int i = 0; i < num_points_0; ++i) {
        glColor3f(colorarray0[i*3], colorarray0[i*3+1], colorarray0[i*3+2]);
        glVertex3f(vertexarray0[i*3], vertexarray0[i*3+1], vertexarray0[i*3+2]);
    }
    for (int i = 0; i < num_points_1; ++i) {
        glColor3f(colorarray1[i*3], colorarray1[i*3+1], colorarray1[i*3+2]);
        glVertex3f(vertexarray1[i*3], vertexarray1[i*3+1], vertexarray1[i*3+2]);
    }
    glEnd();   
    glutSwapBuffers();

    glFlush();

    GLubyte *data = (GLubyte*)malloc(3 * WIDTH * HEIGHT);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, data);
    return data;   
}

static int getColorAndCoordData_new(float* &fdestColor, AVFrame *frameColor, float* &fdestDepth, AVFrame *frameDepth, bool main_camera){
    RGBColor pixel;
    memset(&pixel, 0, sizeof(RGBColor));
    int x,y;
    int active_points = 0;
    for(y=0; y<frameColor->height; y++){
        for(x=0; x<frameColor->width; x++){
            getRGB(&pixel, frameColor, x ,y);
            if((int)pixel.r==13 && (int)pixel.g==237 && (int)pixel.b==13){
                continue;
            }          
            active_points++;
            *fdestColor++ = ((float)pixel.r)/255.f;
            *fdestColor++ = ((float)pixel.g)/255.f;
            *fdestColor++ = ((float)pixel.b)/255.f;

            float x_ = ((float)x)/frameColor->width;
            float y_ = ((float)y)/frameColor->height;
            uint8_t Y = frameDepth->data[0][y*frameDepth->linesize[0]+x];
            float z_ = Y/255.f;   
            // glm::vec4 V = glm::vec4(x_, y_, z_, 1.0f);
            // transformPointCloud(V, angle);
            if(main_camera){
                *fdestDepth++= x_;
                *fdestDepth++= y_;
                *fdestDepth++= z_;    
            }else{
                //transform coordinate
                *fdestDepth++ = 1-z_;
                *fdestDepth++ = y_;
                *fdestDepth++ = 1-x_;
            }
        }
    }
    return active_points;
}

static int getDataForFrame_new(AVFrame *colorFrame0, AVFrame *depthFrame0, AVFrame *colorFrame1, AVFrame *depthFrame1){
    float *fdestColor = colorarray;
    float *fdestDepth = vertexarray;
    int num_points = 0;
    num_points+=getColorAndCoordData_new(fdestColor, colorFrame0, fdestDepth, depthFrame0, true);
    num_points+=getColorAndCoordData_new(fdestColor, colorFrame1, fdestDepth, depthFrame1, true);
    return num_points;
}

static GLubyte* render_new(Encoder *enc ,AVFrame *colorFrame0, AVFrame *depthFrame0, AVFrame *colorFrame1, AVFrame *depthFrame1){
    int num_points=getDataForFrame_new(colorFrame0, depthFrame0, colorFrame1, depthFrame1);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBegin(GL_POINTS);
    for (int i = 0; i < num_points; ++i) {
        glColor3f(colorarray[i*3], colorarray[i*3+1], colorarray[i*3+2]);
        glVertex3f(vertexarray[i*3], vertexarray[i*3+1], vertexarray[i*3+2]);
    }
    glEnd();   
    glutSwapBuffers();

    glFlush();

    GLubyte *data = (GLubyte*)malloc(3 * WIDTH * HEIGHT);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, data);
    return data;
}

static void render_scene(){
    for(int i=0; i<total_frames; i++){
        render(frames[0][i], frames[1][i], frames[2][i], frames[3][i]);
    }
}

static int decode_frame(Decoder *decoder){
    int len, got_frame;
    len = avcodec_decode_video2(decoder->c, decoder->frame, &got_frame, &decoder->avpkt);
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame %d\n", decoder->frame_count);
        return len;
    }
    if (got_frame) {
        //copy the frame into our placeholder structure
        // printf("%d\n", AV_PIX_FMT_YUV420P);
        frames[decoder->id][decoder->frame_count]=av_frame_clone(decoder->frame);
        decoder->frame_count++;
    }
    if (decoder->avpkt.data) {
        decoder->avpkt.size -= len;
        decoder->avpkt.data += len;
    }
    return 0;
}

static void decode_video_frame(Decoder *dcrs)
{
    //maybe we should stop doing this parallel processing:
    for(int i=0; i<MAX_NUM_STREAM; i++){
        while(1){
            dcrs[i].avpkt.size = fread(dcrs[i].inbuf, 1, INBUF_SIZE, dcrs[i].f);
            if(dcrs[i].avpkt.size<=0){
                break;
            }
            dcrs[i].avpkt.data = dcrs[i].inbuf;
            while (dcrs[i].avpkt.size > 0){
                // printf("decoder-%d has %d left to decode\n", i, dcrs[i].avpkt.size );
                if (decode_frame(&dcrs[i]) < 0){
                    fprintf(stderr, "something wrong happens with the decoding\n");
                    exit(1);                    
                }
            }
        }

        dcrs[i].avpkt.data = NULL;
        dcrs[i].avpkt.size = 0;
        dcrs[i].frame_count=0;
        decode_frame(&dcrs[i]);
        avcodec_close(dcrs[i].c);
        av_free(dcrs[i].c);
        av_frame_free(&dcrs[i].frame);
    }

}

static void add_data_to_memory(uint8_t* data, int size){
    output_video.insert(output_video.end(), data, data+size);
}

static void encode_video(Encoder *enc){
    uint8_t endcode[] = { 0, 0, 1, 0xb7};
    int ret, x,y, i,got_output;
    for (i = 0; i < enc->fps; i++) {
        av_init_packet(&enc->pkt);
        enc->pkt.data = NULL;    // packet data will be allocated by the encoder
        enc->pkt.size = 0;
        fflush(stdout);

        //prepare image:
        // GLubyte* data=render(frames[0][i], frames[1][i], frames[2][i], frames[3][i]);
        GLubyte* data=render_new(enc, frames[0][i], frames[1][i], frames[2][i], frames[3][i]);

        for(y=0; y<enc->c->height; y++){
            for(x=0; x<enc->c->width; x++){
                GLubyte *from = data+(y*enc->c->width+x)*3;
                uint8_t R = from[0];
                uint8_t G = from[1];
                uint8_t B = from[2];
                int Y = ( ( 66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
                enc->frame->data[0][y * enc->frame->linesize[0] + x] = Y;
            }
        }

        for (y = 0; y < enc->c->height/2; y++) {
            for (x = 0; x < enc->c->width/2; x++) {
                GLubyte *from = data+(y*2*enc->c->width+x*2)*3;
                uint8_t R = from[0];
                uint8_t G = from[1];
                uint8_t B = from[2];
                int Y = ( ( 66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
                int U = ( ( -38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
                int V = ( ( 112 * R - 94 * G - 18 * B + 128) >> 8) + 128;
                enc->frame->data[1][y * enc->frame->linesize[1] + x] = U;
                enc->frame->data[2][y * enc->frame->linesize[2] + x] = V;
            }
        }

        enc->frame->pts = i;
        /* encode the image */
        ret = avcodec_encode_video2(enc->c, &enc->pkt, enc->frame, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }
        if (got_output) {
            // printf("Write frame %3d (size=%5d)\n", i, enc->pkt.size);
            add_data_to_memory(enc->pkt.data, enc->pkt.size);
            av_free_packet(&enc->pkt);
        }
    }
    /* get the delayed frames */
    for (got_output = 1; got_output; i++) {
        fflush(stdout);
        ret = avcodec_encode_video2(enc->c, &enc->pkt, NULL, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }
        if (got_output) {
            // printf("Write frame %3d (size=%5d)\n", i, enc->pkt.size);
            add_data_to_memory(enc->pkt.data, enc->pkt.size);
            av_free_packet(&enc->pkt);
        }
    }
    /* add sequence end code to have a real mpeg file */
    add_data_to_memory(endcode, sizeof(endcode));
    
    avcodec_close(enc->c);
    av_free(enc->c);
    av_freep(&enc->frame->data[0]);
    av_frame_free(&enc->frame);
}

void setUpOpenGL(){
    glClearColor(0,0,0,0);
    glClearDepth(1.0f);
    // Set up array buffers
    glGenBuffers(1, &vboId);
    glBindBuffer(GL_ARRAY_BUFFER, vboId);
    glGenBuffers(1, &cboId);
    glBindBuffer(GL_ARRAY_BUFFER, cboId);

/*  select clearing (background) color       */
    glClearColor (0.0, 0.0, 0.0, 0.0);

/*  initialize viewing values  */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int listenForConnection(){
    int sockfd, clientfd;  // listen on sock_fd, new connection on clientfd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, "8000", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 0;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 0;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");
    sin_size = sizeof their_addr;
    clientfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    return clientfd;
}

void send_file(uint8_t *data, int data_size, int clientfd){
    char buf[MAXDATASIZE];
    int pos = 0;
    while(pos<data_size){
        int len_to_copy = std::min(MAXDATASIZE, data_size - MAXDATASIZE);
        // printf("%d\n", len_to_copy);
        memcpy(buf, data+pos, len_to_copy);
        if (send(clientfd, buf, len_to_copy, 0) == -1){
            fprintf(stderr, "error in sending response content!\n");
                exit(1);
        }   
        pos+=len_to_copy;
    }
}

void recv_confm(int clientfd){
    uint8_t buf[10];
    int numbytes;
    if((numbytes = recv(clientfd, buf, 9, 0))!=0){
        // printf("%d\n", numbytes);
        buf[numbytes]='\n';
        // printf("received confirmation: %s\n", buf);
    }else{
        fprintf(stderr, "error receiving confirmation\n");
    }
}

static void run_server(Decoder *dcrs, Encoder *enc){
    int clientfd = listenForConnection();

    //start processing video clips from 0 to 59
    for(int i=0; i<30; i++){
        for(int j=0; j<MAX_NUM_STREAM; j++){
            allocate_atx_and_frame_for_decoder(&dcrs[j]);
            allocate_atx_and_frame_for_encoder(enc, AV_CODEC_ID_MPEG1VIDEO);
        }
        // printf("%d\n", i);
        sprintf(dcrs[0].filename, "clips/color0_%d.mpg", i);
        sprintf(dcrs[1].filename, "clips/depth0_%d.mpg", i);
        sprintf(dcrs[2].filename, "clips/color1_%d.mpg", i);
        sprintf(dcrs[3].filename, "clips/depth1_%d.mpg", i);       
        dcrs[0].f = fopen(dcrs[0].filename, "rb");
        dcrs[1].f = fopen(dcrs[1].filename, "rb");
        dcrs[2].f = fopen(dcrs[2].filename, "rb");
        dcrs[3].f = fopen(dcrs[3].filename, "rb");
        
        struct timeval * pre = new struct timeval;
        struct timeval * now = new struct timeval;

        gettimeofday(pre, NULL);
        decode_video_frame(dcrs);
        encode_video(enc);
        gettimeofday(now, NULL);

        long mili = (now->tv_sec - pre->tv_sec)*1000+(now->tv_usec-pre->tv_usec)/1000;

        printf("%ld\n", mili);



        // printf("%d\n", i);
        // printf("%lu\n", output_video.size());

        send_file(&output_video[0], output_video.size(), clientfd);
        // printf("sent video %d\n", i);
        // recv_confm(clientfd);

        // FILE *pFile;
        // pFile = fopen("output_20.mpg", "wb");
        // fwrite(&output_video[0], sizeof(uint8_t), output_video.size(), pFile);
        // fclose(pFile);

        // break;
        output_video.clear();


    
    }
    // fclose(output);
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize (640, 480); 
    glutInitWindowPosition (100, 100);
    glutCreateWindow (argv[0]);
    /* register all the codecs */
    avcodec_register_all();
    setUpOpenGL();

    for(int i=0; i<MAX_NUM_STREAM; i++){
        for(int j=0; j<FPS; j++){
            frames[i][j]=NULL;
        }
    }

    Decoder *dcrs = (Decoder*)malloc(MAX_NUM_STREAM*sizeof(Decoder));
    init_decoder(dcrs);
    Encoder *enc = (Encoder*)malloc(sizeof(Encoder));
    init_encoder(enc, AV_CODEC_ID_MPEG1VIDEO, 25);

    run_server(dcrs, enc);

    // decode_video_frame(dcrs);
    // encode_video(enc);


    // printf("%lu\n", output_video.size());

    // FILE *output;
    // output = fopen("output_to_disk.mpg", "wb");
    // fwrite(&output_video[0], 1, output_video.size(), output);
    // fclose(output);
    
    // glutDisplayFunc(render_scene); 
    // glutIdleFunc(render_scene);
    // glutMainLoop();

    return 0;
}



