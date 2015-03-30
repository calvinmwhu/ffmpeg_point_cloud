#include <math.h>
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

typedef struct RGBColor_t{
    uint8_t r;
    uint8_t g;
    uint8_t b;
}RGBColor;

typedef struct Decoder_t{
    int id;
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
    // int i, ret, x, y, got_output;
    FILE *f;
    const char* filename;
    AVFrame *frame;
    AVPacket pkt;
    uint8_t endcode[] ;
}Encoder;

static void init_decoder(Decoder *dcrs, int number, AVCodec *codec){
    dcrs[0].filename="color0.mpg";
    dcrs[1].filename="depth0.mpg";
    dcrs[2].filename="color1.mpg";
    dcrs[3].filename="depth1.mpg";
    for(int i=0;i<number; i++){
        dcrs[i].id=i;
        av_init_packet(&dcrs[i].avpkt);
        memset(dcrs[i].inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);
        
        dcrs[i].c = avcodec_alloc_context3(codec);
        if (!dcrs[i].c) {
            fprintf(stderr, "Could not allocate video codec context\n");
            exit(1);
        }
        if(codec->capabilities&CODEC_CAP_TRUNCATED){
            dcrs[i].c->flags|= CODEC_FLAG_TRUNCATED; 
        }
        if (avcodec_open2(dcrs[i].c, codec, NULL) < 0) {
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

static void init_encoder(Encoder *enc, int codec_id){
    enc->codec = avcodec_find_encoder(codec_id);
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
    enc->c->width = 640;
    enc->c->height = 480;
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
            *fdest++ = pixel.r;
            *fdest++ = pixel.g;
            *fdest++ = pixel.b;
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
            *fdest++ = (float)x;
            *fdest++ = (float)y;
            uint8_t Y = frame->data[0][frame->linesize[0]*y+x];
            *fdest++ = Y/255.f;
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
    // Camera setup
    glViewport(0, 0, WIDTH, HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, WIDTH /(GLdouble) HEIGHT, 0.1, 1000);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0,0,0,0,0,1,0,1,0);
}

void getDataForFrame(AVFrame *colorFrame, AVFrame *depthFrame){
    const int dataSize = WIDTH*HEIGHT*3*sizeof(float);
    GLubyte* ptr;
    glBindBuffer(GL_ARRAY_BUFFER, vboId);
    glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);
    ptr = (GLubyte*) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    //get coordinate data
    if(ptr){
        getFrameCoordinateData(ptr, depthFrame);
    }else{
        fprintf(stderr, "Failed to get a point to coordinate data\n");
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, cboId);
    glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);
    ptr = (GLubyte*) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    //get color data:
    if(ptr){
        getFrameColorData(ptr, colorFrame);
    }else{
        fprintf(stderr, "Failed to get a point to rgb data\n");
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);
}

static GLubyte* render(AVFrame *colorFrame, AVFrame *depthFrame){    
    getDataForFrame(colorFrame, depthFrame);
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

static int decode_frame(Decoder *decoder, int last){
    int len, got_frame;
    len = avcodec_decode_video2(decoder->c, decoder->frame, &got_frame, &decoder->avpkt);
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame %d\n", decoder->frame_count);
        return len;
    }
    if (got_frame) {
        // printf("Saving %sframe %3d\n", last ? "last " : "", decoder->frame_count);
        // printf("decoder-%d gets %d's frame \n", decoder->id, decoder->frame_count);
        fflush(stdout);
        /* the picture is allocated by the decoder, no need to free it */
        // snprintf(buf, sizeof(buf), outfilename, *frame_count);
        // write_data(frame, buf);
        decoder->frame_count++;
    }
    if (decoder->avpkt.data) {
        decoder->avpkt.size -= len;
        decoder->avpkt.data += len;
    }
    return 0;
}

static void decode_video(Decoder *dcrs, int number)
{
    AVCodec *codec;
    /* find the mpeg1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    init_decoder(dcrs, number, codec);

    for (;;) {
        int i;
        for(i=0; i<number; i++){
            dcrs[i].avpkt.size = fread(dcrs[i].inbuf, 1, INBUF_SIZE, dcrs[i].f);
            // if(dcrs[i].avpkt.size==0){
            //     break;
            // }
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
        // printf("%d\n", dcrs[0].frame_count);
        //after decoding the frame, render the data:
        GLubyte* data0 = render(dcrs[0].frame, dcrs[1].frame);
        GLubyte* data1 = render(dcrs[2].frame, dcrs[3].frame);

        // FILE *write_file;
        // write_file=fopen("myfile.pgm", "w");
        // fwrite (data0, sizeof(GLubyte), 3*WIDTH*HEIGHT, write_file);
        // fclose(write_file);


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

static void encode_video(const char *filename, int codec_id, int frame_count){
   
    // /* get the delayed frames */
    // for (got_output = 1; got_output; i++) {
    //     fflush(stdout);
    //     ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
    //     if (ret < 0) {
    //         fprintf(stderr, "Error encoding frame\n");
    //         exit(1);
    //     }
    //     if (got_output) {
    //         printf("Write frame %3d (size=%5d)\n", i, pkt.size);
    //         fwrite(pkt.data, 1, pkt.size, f);
    //         av_free_packet(&pkt);
    //     }
    // }
    // /* add sequence end code to have a real mpeg file */
    // fwrite(endcode, 1, sizeof(endcode), f);
    // fclose(f);
    // avcodec_close(c);
    // av_free(c);
    // av_freep(&frame->data[0]);
    // av_frame_free(&frame);
    // printf("\n");
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize (500, 500); 
    glutInitWindowPosition (100, 100);
    glutCreateWindow (argv[0]);
    /* register all the codecs */
    avcodec_register_all();
    setUpOpenGL();

    Decoder *dcrs = (Decoder*)malloc(4*sizeof(Decoder));
    Encoder *enc = (Encoder*)malloc(sizeof(Encoder));
    decode_video(dcrs, 4);

    return 0;
}



