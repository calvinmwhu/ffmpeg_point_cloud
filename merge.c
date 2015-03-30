#include <math.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
#define WIDTH 640
#define HEIGHT 480

GLuint vboId; // Vertex buffer ID
GLuint cboId; // Color buffer ID
GLuint *framebuffer;
GLuint *framebuffer;
AVFrame **frames;
int total_frames ;
float colorarray0[WIDTH*HEIGHT*3];
float vertexarray0[WIDTH*HEIGHT*3];
float colorarray1[WIDTH*HEIGHT*3];
float vertexarray1[WIDTH*HEIGHT*3];


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
    const char* filename;
    AVFrame *frame;
    uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    AVPacket avpkt;
}Decoder;

typedef struct Encoder_t{
    AVCodec *codec;
    AVCodecContext *c;
    int number_frames;
    FILE *f;
    const char* filename;
    AVFrame *frame;
    AVPacket pkt;
}Encoder;

static void init_decoder(Decoder *dcrs, int number ){
    dcrs[0].filename="color0.mpg";
    dcrs[1].filename="depth0.mpg";
    dcrs[2].filename="color1.mpg";
    dcrs[3].filename="depth1.mpg";
    for(int i=0;i<number; i++){
        dcrs[i].id=i;
        /* find the mpeg1 video decoder */
        dcrs[i].codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
        if (!dcrs[i].codec) {
            fprintf(stderr, "Codec not found\n");
            exit(1);
        }
        av_init_packet(&dcrs[i].avpkt);
        memset(dcrs[i].inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);
        
        dcrs[i].c = avcodec_alloc_context3(dcrs[i].codec);
        if (!dcrs[i].c) {
            fprintf(stderr, "Could not allocate video codec context\n");
            exit(1);
        }
        if(dcrs[i].codec->capabilities&CODEC_CAP_TRUNCATED){
            dcrs[i].c->flags|= CODEC_FLAG_TRUNCATED; 
        }
        if (avcodec_open2(dcrs[i].c, dcrs[i].codec, NULL) < 0) {
            fprintf(stderr, "Could not open codec\n");
            exit(1);
        }
        dcrs[i].f = fopen(dcrs[i].filename, "rb");
        if (!dcrs[i].f) {
            fprintf(stderr, "Could not open %s\n", dcrs[i].filename);
            exit(1);
        }
        dcrs[i].frame = av_frame_alloc();
        if (!dcrs[i].frame) {
            fprintf(stderr, "Could not allocate video frame\n");
            exit(1);
        }
        dcrs[i].frame_count = 0;
    }
}

// typedef struct Encoder_t{
//     AVCodec *codec;
//     AVCodecContext *c;
//     // int i, ret, x, y, got_output;
//     FILE *f;
//     const char* filename;
//     AVFrame *frame;
//     AVPacket pkt;
//     uint8_t endcode[] ;
// }Encoder;


static void init_encoder(Encoder *enc, int codec_id, int number_frames){
    enc->number_frames=number_frames;
    enc->codec = avcodec_find_encoder(codec_id);
    enc->filename = "output.mpg";
    printf("encoding video file %s \n", enc->filename);
    if (!enc->codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    enc->c = avcodec_alloc_context3(enc->codec);
    if (!enc->c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    enc->c->bit_rate = 400000;
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
    enc->f = fopen(enc->filename, "wb");
    if (!enc->f) {
        fprintf(stderr, "Could not open %s\n", enc->filename);
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

static void getRGB(RGBColor *pixel, AVFrame *frame, int x, int y){
    
    // Y component
    const unsigned char Y = frame->data[0][frame->linesize[0]*y + x];

    // U, V components 
    x /= 2;
    y /= 2;
    const unsigned char U = frame->data[1][frame->linesize[1]*y + x];
    const unsigned char V = frame->data[2][frame->linesize[2]*y + x];
    // RGB conversion
    pixel->r = Y + 1.402*(V-128);
    pixel->g = Y - 0.344*(U-128) - 0.714*(V-128);
    pixel->b = Y + 1.772*(U-128);
}

static void getFrameColorData(GLubyte* dest, AVFrame *frame){
    float* fdest = (float*) dest;
    RGBColor pixel;
    memset(&pixel, 0, sizeof(RGBColor));
    int x, y;
    for(y=0; y<frame->height; y++){
        for(x=0; x<frame->width; x++){
            getRGB(&pixel, frame, x ,y);
            *fdest++ = pixel.r/255.f;
            *fdest++ = pixel.g/255.f;
            *fdest++ = pixel.b/255.f;
            // printf("%f, %f, %f\n", *(fdest-3), *(fdest-2), *(fdest-1) );
        }
    }
}

static void getFrameCoordinateData(GLubyte* dest, AVFrame *frame){
    float* fdest = (float*) dest;
    RGBColor pixel;
    memset(&pixel, 0, sizeof(RGBColor));
    int x,y;
    for(y=0; y<frame->height; y++){
        for(x=0; x<frame->width; x++){
            RGBColor pixel;
            *fdest++ = ((float)x)/WIDTH;
            *fdest++ = ((float)y)/HEIGHT;
            uint8_t Y = frame->data[0][frame->linesize[0]*y+x];
            *fdest++ = Y/255.f;
            // printf("%f, %f, %f\n", *(fdest-3), *(fdest-2), *(fdest-1) );
        }
    }
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



    // // Camera setup
    // glViewport(0, 0, WIDTH, HEIGHT);
    // glMatrixMode(GL_PROJECTION);
    // glLoadIdentity();
    // gluPerspective(45, WIDTH /(GLdouble) HEIGHT, 0.1, 1000);
    // glMatrixMode(GL_MODELVIEW);
    // glLoadIdentity();
    // gluLookAt(0,0,0,0,0,1,0,1,0);
}

void getDataForFrame_old(AVFrame *colorFrame0, AVFrame *depthFrame0, AVFrame *colorFrame1, AVFrame *depthFrame1){
    getFrameCoordinateData((GLubyte*)vertexarray0, depthFrame0);
    getFrameCoordinateData((GLubyte*)vertexarray1, depthFrame1);
    getFrameColorData((GLubyte*)colorarray0, colorFrame0);
    getFrameColorData((GLubyte*)colorarray1, colorFrame1);
}

void getDataForFrame(AVFrame *colorFrame0, AVFrame *depthFrame0, AVFrame *colorFrame1, AVFrame *depthFrame1){
    const int dataSize = WIDTH*HEIGHT*6*sizeof(float);
    GLubyte* ptr;
    glBindBuffer(GL_ARRAY_BUFFER, vboId);
    glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);
    ptr = (GLubyte*) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    //get coordinate data
    if(ptr){
        getFrameCoordinateData(ptr, depthFrame0);
        getFrameCoordinateData(ptr, depthFrame1);   
    }else{
        fprintf(stderr, "Failed to get a point to coordinate data\n");
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, cboId);
    glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);
    ptr = (GLubyte*) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    //get color data:
    if(ptr){
        getFrameColorData(ptr, colorFrame0);
        getFrameColorData(ptr, colorFrame1);
    }else{
        fprintf(stderr, "Failed to get a point to rgb data\n");
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);
}


static GLubyte* render(AVFrame *colorFrame0, AVFrame *depthFrame0, AVFrame *colorFrame1, AVFrame *depthFrame1){    
    getDataForFrame(colorFrame0, depthFrame0, colorFrame1, depthFrame1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glBindBuffer(GL_ARRAY_BUFFER, vboId);
    glVertexPointer(3, GL_FLOAT, 0, NULL);

    glBindBuffer(GL_ARRAY_BUFFER, cboId);
    glColorPointer(3, GL_FLOAT, 0, NULL);

    glPointSize(1.f);
    glDrawArrays(GL_POINTS, 0, WIDTH*HEIGHT);

    GLubyte *data = malloc(3 * WIDTH * HEIGHT);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, data);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    return data;
}

static GLubyte* render_old(AVFrame *colorFrame0, AVFrame *depthFrame0, AVFrame *colorFrame1, AVFrame *depthFrame1){
    getDataForFrame_old(colorFrame0, depthFrame0, colorFrame1, depthFrame1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glColor3f (1.0, 1.0, 1.0);

    glBegin(GL_POINTS);
    for (int i = 0; i < WIDTH*HEIGHT; ++i) {
        glColor3f(colorarray0[i*3], colorarray0[i*3+1], colorarray0[i*3+2]);
        glVertex3f(vertexarray0[i*3], vertexarray0[i*3+1], vertexarray0[i*3+2]);
        glColor3f(colorarray1[i*3], colorarray1[i*3+1], colorarray1[i*3+2]);
        glVertex3f(vertexarray1[i*3], vertexarray1[i*3+1], vertexarray1[i*3+2]);
    }
    glEnd();   
    glutSwapBuffers();

    glFlush();

    GLubyte *data = malloc(3 * WIDTH * HEIGHT);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, data);

    return data;
}


static void render_scene(){
    for(int i=0; i<total_frames; i++){
        render(&frames[0][i], &frames[1][i], &frames[2][i], &frames[3][i]);
    }
}

static void render_scene_old(){
    for(int i=0; i<total_frames; i++){
        render_old(&frames[0][i], &frames[1][i], &frames[2][i], &frames[3][i]);
    }

}



static int decode_frame(Decoder *decoder, int last){
    int len, got_frame;
    len = avcodec_decode_video2(decoder->c, decoder->frame, &got_frame, &decoder->avpkt);
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame %d\n", decoder->frame_count);
        return len;
    }
    if (got_frame) {
        // printf("Saving %sframe %3d\n", last ? "last " : "", decoder->frame_count);
        printf("decoder-%d gets %d's frame \n", decoder->id, decoder->frame_count);
        fflush(stdout);
        /* the picture is allocated by the decoder, no need to free it */
        // snprintf(buf, sizeof(buf), outfilename, *frame_count);
        // write_data(frame, buf);
        memcpy(&frames[decoder->id][decoder->frame_count], decoder->frame, sizeof(AVFrame));
        decoder->frame_count++;
    }
    if (decoder->avpkt.data) {
        decoder->avpkt.size -= len;
        decoder->avpkt.data += len;
    }
    return 0;
}

static void decode_video_frame(Decoder *dcrs, int number)
{
    for (;;) {
        int i;
        for(i=0; i<number; i++){
            dcrs[i].avpkt.size = fread(dcrs[i].inbuf, 1, INBUF_SIZE, dcrs[i].f);
        }
        if(dcrs[0].avpkt.size==0 && dcrs[1].avpkt.size==0 && dcrs[2].avpkt.size==0 && dcrs[3].avpkt.size==0){
            break;
        }
        /* NOTE1: some codecs are stream based (mpegvideo, mpegaudio)
           and this is the only method to use them because you cannot
           know the compressed data size before analysing it.
           BUT some other codecs (msmpeg4, mpeg4) are inherently frame
           based, so you must call them with all the data for one
           frame exactly. You must also initialize 'width' and
           'height' before initializing them. */
        /* NOTE2: some codecs allow the raw parameters (frame size,
           sample rate) to be changed at any frame. We handle this, so
           you should also take care of it */
        /* here, we use a stream based decoder (mpeg1video), so we
           feed decoder and see if it could decode a frame */
        for(i=0; i<number; i++){
            dcrs[i].avpkt.data = dcrs[i].inbuf;
            while (dcrs[i].avpkt.size > 0){
                // printf("%d has decoded %d \n", i, dcrs[i].avpkt.size );
                if (decode_frame(&dcrs[i], 0) < 0){
                    fprintf(stderr, "something wrong happens with the decoding\n");
                    exit(1);                    
                }
            }           
        }
        // GLubyte* data = render(dcrs[0].frame, dcrs[1].frame, dcrs[2].frame, dcrs[3].frame);
    }
    /* some codecs, such as MPEG, transmit the I and P frame with a
       latency of one frame. You must do the following to have a
       chance to get the last frame of the video */
    int i;
    for(i=0; i<number; i++){
        dcrs[i].avpkt.data = NULL;
        dcrs[i].avpkt.size = 0;
        decode_frame(&dcrs[i], 1);
        fclose(dcrs[i].f);
        avcodec_close(dcrs[i].c);
        av_free(dcrs[i].c);
        av_frame_free(&dcrs[i].frame);
        printf("\n");
    }
}

static void encode_video(Encoder *enc){
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    int ret, x,y, i,got_output;
    for (i = 0; i < 25; i++) {
        av_init_packet(&enc->pkt);
        enc->pkt.data = NULL;    // packet data will be allocated by the encoder
        enc->pkt.size = 0;
        fflush(stdout);

        //prepare image:
        GLubyte* data=render_old(&frames[0][i], &frames[1][i], &frames[2][i], &frames[3][i]);
        //Y U, V:
        for(y=0; y<enc->c->height; y++){
            for(x=0; x<enc->c->width; x++){
                GLubyte *from = data+y*enc->c->height+x;
                uint8_t R = from[0];
                uint8_t G = from[1];
                uint8_t B = from[2];
                uint8_t Y = 0.299*R + 0.587*G + 0.114*B;
                enc->frame->data[0][y * enc->frame->linesize[0] + x] = Y;
                enc->frame->data[1][y/2 * enc->frame->linesize[1] + x/2] = 0.492*(B-Y);
                enc->frame->data[2][y/2 * enc->frame->linesize[2] + x/2] = 0.877*(R-Y);
            }
        }
        //Y:
        // for(y=0; y<enc->c->height; y++){
        //     for(x=0; x<enc->c->width; x++){
        //         GLubyte *from = data+y*enc->c->height+x;
        //         uint8_t R = from[0];
        //         uint8_t G = from[1];
        //         uint8_t B = from[2];
        //         enc->frame->data[0][y * enc->frame->linesize[0] + x] = 0.299*R + 0.587*G + 0.114*B;
        //     }
        // }
        // /* Cb and Cr */
        // for (y = 0; y < enc->c->height/2; y++) {
        //     for (x = 0; x < enc->c->width/2; x++) {
        //         GLubyte *from = data+y*2*enc->c->height+x*2;
        //         uint8_t R = from[0];
        //         uint8_t G = from[1];
        //         uint8_t B = from[2];
        //         uint8_t Y = enc->frame->data[0][y*2* enc->frame->linesize[0] + x*2] ;
        //         enc->frame->data[1][y * enc->frame->linesize[1] + x] = 0.492*(B-Y);
        //         enc->frame->data[2][y * enc->frame->linesize[2] + x] = 0.877*(R-Y);
        //     }
        // }

        //  for (y = 0; y < enc->c->height; y++) {
        //     for (x = 0; x < enc->c->width; x++) {
        //         enc->frame->data[0][y * enc->frame->linesize[0] + x] = x + y + i * 3;
        //     }
        // }
        // /* Cb and Cr */
        // for (y = 0; y < enc->c->height/2; y++) {
        //     for (x = 0; x < enc->c->width/2; x++) {
        //         enc->frame->data[1][y * enc->frame->linesize[1] + x] = 128 + y + i * 2;
        //         enc->frame->data[2][y * enc->frame->linesize[2] + x] = 64 + x + i * 5;
        //     }
        // }

        enc->frame->pts = i;
        /* encode the image */
        ret = avcodec_encode_video2(enc->c, &enc->pkt, enc->frame, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }
        if (got_output) {
            printf("Write frame %3d (size=%5d)\n", i, enc->pkt.size);
            fwrite(enc->pkt.data, 1, enc->pkt.size, enc->f);
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
            printf("Write frame %3d (size=%5d)\n", i, enc->pkt.size);
            fwrite(enc->pkt.data, 1, enc->pkt.size, enc->f);
            av_free_packet(&enc->pkt);
        }
    }
    /* add sequence end code to have a real mpeg file */
    fwrite(endcode, 1, sizeof(endcode), enc->f);
    fclose(enc->f);
    avcodec_close(enc->c);
    av_free(enc->c);
    av_freep(&enc->frame->data[0]);
    av_frame_free(&enc->frame);
    printf("\n");
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

    frames = (AVFrame**)malloc(4*sizeof(AVFrame*));
    for(int i=0; i<4; i++){
        frames[i]=(AVFrame*)malloc(30*sizeof(AVFrame));
    }
    total_frames=INT_MAX;

    Decoder *dcrs = (Decoder*)malloc(4*sizeof(Decoder));
    init_decoder(dcrs, 4);
    decode_video_frame(dcrs, 4);

    if(total_frames>dcrs[0].frame_count){
        total_frames=dcrs[0].frame_count;
    }
    if(total_frames>dcrs[1].frame_count){
        total_frames=dcrs[1].frame_count;
    }
    if(total_frames>dcrs[2].frame_count){
        total_frames=dcrs[2].frame_count;
    }
    if(total_frames>dcrs[3].frame_count){
        total_frames=dcrs[3].frame_count;
    }


    Encoder *enc = (Encoder*)malloc(sizeof(Encoder));
    init_encoder(enc, AV_CODEC_ID_MPEG1VIDEO, total_frames);
    encode_video(enc);

    // glutDisplayFunc(render_scene_old); 
    // glutIdleFunc(render_scene_old);

    // glutMainLoop();

    return 0;
}



