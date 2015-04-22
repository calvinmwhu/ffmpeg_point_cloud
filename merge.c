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
#include <libswscale/swscale.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>

#define INBUF_SIZE 4096
#define WIDTH 640
#define HEIGHT 480
#define FPS 30
#define MAX_NUM_STREAM 4

AVFrame *frames[MAX_NUM_STREAM][FPS];

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

static void init_decoder(Decoder *dcrs){
    dcrs[0].filename="color0.mpg";
    dcrs[1].filename="depth0.mpg";
    dcrs[2].filename="color1.mpg";
    dcrs[3].filename="depth1.mpg";
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
        
        dcrs[i].c = avcodec_alloc_context3(dcrs[i].codec);
        if(!dcrs[i].c){
            fprintf(stderr, "Could not allocate video codec context\n");
            exit(1);
        }
        if(dcrs[i].codec->capabilities&CODEC_CAP_TRUNCATED){
            dcrs[i].c->flags|= CODEC_FLAG_TRUNCATED; 
        }
        if(avcodec_open2(dcrs[i].c, dcrs[i].codec, NULL) < 0) {
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

static int getColorAndCoordData(float* destColor, AVFrame *frameColor, float* destDepth, AVFrame *frameDepth){
    float *fdestColor = destColor;
    float *fdestDepth = destDepth;
    int x,y;
    int num=0;
    for(y=0; y<frameColor->height; y++){
        for(x=0; x<frameColor->width; x++){
            int offset = y*frameColor->width+x;
            uint8_t r_color = frameColor->data[0][offset];
            uint8_t g_color = frameColor->data[0][offset+1];
            uint8_t b_color = frameColor->data[0][offset+2];
            uint8_t r_coord = frameDepth->data[0][offset];
            uint8_t g_coord = frameDepth->data[0][offset+1];
            uint8_t b_coord = frameDepth->data[0][offset+2];
            if(r_color==0 && g_color==255 && b_color==0){
                continue;   
            }
            num++;
            *fdestColor++ = ((float)r_color)/255.f;
            *fdestColor++ = ((float)g_color)/255.f;
            *fdestColor++ = ((float)b_color)/255.f;
            *fdestDepth++ = ((float)x)/frameColor->width;
            *fdestDepth++ = ((float)y)/frameColor->height;
            *fdestDepth++ = r_coord/255.f;   
        }
    }
    // printf("%d\n", num);
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

    GLubyte *data = malloc(3 * WIDTH * HEIGHT);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, data);

    return data;
}

static void render_scene(){
    for(int i=0; i<total_frames; i++){
        render(frames[0][i], frames[1][i], frames[2][i], frames[3][i]);
    }
}

static int decode_frame_RGB(Decoder *decoder){
    int frame_data_size = (sizeof(uint8_t))*decoder->frame->width*decoder->frame->height*3;
    AVFrame *rgbFrame = av_frame_alloc();
    rgbFrame->width = decoder->frame->width;
    rgbFrame->height = decoder->frame->height;
    rgbFrame->format = AV_PIX_FMT_RGB24 ;
    rgbFrame->pict_type = decoder->frame->pict_type;
    rgbFrame->data[0] = malloc(frame_data_size);
    memset(rgbFrame->data[0], 0, frame_data_size);
    rgbFrame->linesize[0] = 3*rgbFrame->width;
    struct SwsContext* rgbCtx = sws_getContext(
            decoder->frame->width,
            decoder->frame->height,
            decoder->frame->format,
            rgbFrame->width,
            rgbFrame->height,
            rgbFrame->format,
            SWS_BICUBIC,
            NULL,NULL,NULL
        );
    int out_height = sws_scale(
        rgbCtx,
        decoder->frame->data,
        decoder->frame->linesize,
        0,
        decoder->frame->height,
        rgbFrame->data,
        rgbFrame->linesize
        );

    frames[decoder->id][decoder->frame_count]=av_frame_clone(rgbFrame);
    decoder->frame_count++;
    av_frame_free(&rgbFrame);
    return out_height;
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
        //can we directly convert it to a RGB frame ?
        decode_frame_RGB(decoder);
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
        decode_frame(&dcrs[i]);
        fclose(dcrs[i].f);
        avcodec_close(dcrs[i].c);
        av_free(dcrs[i].c);
        av_frame_free(&dcrs[i].frame);
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
        GLubyte* data=render(frames[0][i], frames[1][i], frames[2][i], frames[3][i]);
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



void setUpOpenGL(){
    glClearColor(0,0,0,0);
    glClearDepth(1.0f);
    /* select clearing (background) color*/
    glClearColor (0.0, 0.0, 0.0, 0.0);

    /* initialize viewing values */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

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

    // frames = (AVFrame**)malloc(4*sizeof(AVFrame*));
    // for(int i=0; i<4; i++){
    //     frames[i]=(AVFrame*)malloc(30*sizeof(AVFrame));
    // }
    for(int i=0; i<MAX_NUM_STREAM; i++){
        for(int j=0; j<FPS; j++){
            frames[i][j]=NULL;
        }
    }

    total_frames=INT_MAX;

    Decoder *dcrs = (Decoder*)malloc(MAX_NUM_STREAM*sizeof(Decoder));
    init_decoder(dcrs);
    decode_video_frame(dcrs);


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

    // printf("%d\n", total_frames);

    Encoder *enc = (Encoder*)malloc(sizeof(Encoder));
    init_encoder(enc, AV_CODEC_ID_MPEG1VIDEO, total_frames);
    encode_video(enc);

    // glutDisplayFunc(render_scene); 
    // glutIdleFunc(render_scene);
    // glutMainLoop();

    return 0;
}



