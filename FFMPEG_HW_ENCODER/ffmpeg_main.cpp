// Ref : https://github.com/jiangjie8/FFmpegSimple/blob/32a27294c4d0bb30af7eba470c62993fa97fd1cf/source/hw_encode/hw_encode.cpp

/*
    Out Format : YUV420P(I420)
        ffmpeg -i input320x180.mp4 -vf format=yuv420p -f rawvideo out_yuv420.yuv

    Out Format : NV12
        ffmpeg -i input320x180.mp4 -vf format=nv12 -f rawvideo out_nv12.yuv
*/

//#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#include "ffmpeg_main.h"

static int width, height;
static AVBufferRef* hw_device_ctx = NULL;

static int set_hwframe_ctx(AVCodecContext* ctx, AVBufferRef* hw_device_ctx)
{
    AVBufferRef* hw_frames_ref;
    AVHWFramesContext* frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        fprintf(stderr, "Failed to create VAAPI frame context.\n");
        return -1;
    }
    frames_ctx = (AVHWFramesContext*)(hw_frames_ref->data);
    frames_ctx->format = AV_PIX_FMT_QSV;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width = width;
    frames_ctx->height = height;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        fprintf(stderr, "Failed to initialize VAAPI frame context."
            "Error code: %s\n", av_err2str(err));
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx)
        err = AVERROR(ENOMEM);

    av_buffer_unref(&hw_frames_ref);
    return err;
}

static int encode_write(AVCodecContext* avctx, AVFrame* frame, FILE* fout)
{
    int ret = 0;
    AVPacket enc_pkt;

    av_init_packet(&enc_pkt);
    enc_pkt.data = NULL;
    enc_pkt.size = 0;

    if ((ret = avcodec_send_frame(avctx, frame)) < 0) {
        fprintf(stderr, "Error code: %s\n", av_err2str(ret));
        goto end;
    }
    while (1) {
        ret = avcodec_receive_packet(avctx, &enc_pkt);
        if (ret)
            break;

        enc_pkt.stream_index = 0;
        printf("=== %d\n", enc_pkt.size);
        ret = fwrite(enc_pkt.data, enc_pkt.size, 1, fout);
        av_packet_unref(&enc_pkt);
    }

end:
    ret = ((ret == AVERROR(EAGAIN)) ? 0 : -1);
    return ret;
}

static void read_file_bgra(FILE* srcfile, uint8_t* data[4], int linesize[4], int width, int height, int buffsize)
{
    int err;

    if ((err = fread((uint8_t*)data[0], 1, buffsize, srcfile)) <= 0) // read Packed BGRA data
    {
        printf("Failed to read BGRA data from file : %d\n", err);
    }

    printf("read file data : %d\n", err);
}

char input_url[] = R"(out.bgra)";
char output_url[] = R"(test.h264)";

int main(int argc, char* argv[])
{
    int size, err;
    FILE* fin = NULL, * fout = NULL;

    uint8_t* src_data[4], * dst_data[4];
    int src_linesize[4], dst_linesize[4];

    int src_bufsize;

    AVFrame* sw_frame = NULL, * hw_frame = NULL;
    AVCodecContext* avctx = NULL;
    AVCodec* codec = NULL;
    SwsContext* _swsCtx = nullptr;

    const char* enc_name = "h264_qsv";

    int ret;
    int frames = 1000;

    width = 1280;
    height = 720;
    size = width * height;

    if (!(fin = fopen(input_url, "r+b"))) {
        fprintf(stderr, "Fail to open input file : %s\n", strerror(errno));
        return -1;
    }
    if (!(fout = fopen(output_url, "w+b"))) {
        fprintf(stderr, "Fail to open output file : %s\n", strerror(errno));
        err = -1;
        goto close;
    }

    err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_QSV,
        NULL, NULL, 0);
    if (err < 0) {
        fprintf(stderr, "Failed to create a VAAPI device. Error code: %s\n", av_err2str(err));
        goto close;
    }

    if (!(codec = avcodec_find_encoder_by_name(enc_name))) {
        fprintf(stderr, "Could not find encoder.\n");
        err = -1;
        goto close;
    }

    if (!(avctx = avcodec_alloc_context3(codec))) {
        err = AVERROR(ENOMEM);
        goto close;
    }

    avctx->width = width;
    avctx->height = height;
    avctx->time_base = { 1, 25 };
    avctx->framerate = { 25, 1 };
    avctx->sample_aspect_ratio = { 1, 1 };
    avctx->pix_fmt = AV_PIX_FMT_QSV;

    /* set hw_frames_ctx for encoder's AVCodecContext */
    if ((err = set_hwframe_ctx(avctx, hw_device_ctx)) < 0) {
        fprintf(stderr, "Failed to set hwframe context.\n");
        goto close;
    }

    if ((err = avcodec_open2(avctx, codec, NULL)) < 0) {
        fprintf(stderr, "Cannot open video encoder codec. Error code: %s\n", av_err2str(err));
        goto close;
    }

    /* create scaling context */
    _swsCtx = sws_getContext(width, height, AV_PIX_FMT_BGRA,
        width, height, AV_PIX_FMT_NV12,
        SWS_BICUBIC, NULL, NULL, NULL);
    if (!_swsCtx) {
        fprintf(stderr,
            "Impossible to create scale context for the conversion "
            "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
            av_get_pix_fmt_name(AV_PIX_FMT_BGRA), width, height,
            av_get_pix_fmt_name(AV_PIX_FMT_NV12), width, height);
        //ret = AVERROR(EINVAL);
        goto close;
    }

    /* allocate source and destination image buffers */
    if ((ret = av_image_alloc(src_data, src_linesize,
        width, height, AV_PIX_FMT_BGRA, 1)) < 0) {
        fprintf(stderr, "Could not allocate source image\n");
        goto close;
    }
    src_bufsize = ret;
    printf("Source image alloc size : %d\n", src_bufsize);

    /* buffer is going to be written to rawvideo file, no alignment */
    if ((ret = av_image_alloc(dst_data, dst_linesize,
        width, height, AV_PIX_FMT_NV12, 16)) < 0) {
        fprintf(stderr, "Could not allocate destination image\n");
        goto close;
    }

    while (--frames > 0)
    {
        /* generate synthetic video */
        read_file_bgra(fin, src_data, src_linesize, width, height, src_bufsize);

        /* convert to destination format */
        sws_scale(_swsCtx, (const uint8_t* const*)src_data,
            src_linesize, 0, height, dst_data, dst_linesize);

        if (!(sw_frame = av_frame_alloc())) {
            err = AVERROR(ENOMEM);
            goto close;
        }
        /* read data into software frame, and transfer them into hw frame */
        sw_frame->width = width;
        sw_frame->height = height;
        sw_frame->format = AV_PIX_FMT_NV12;
        if ((err = av_frame_get_buffer(sw_frame, 32)) < 0)
            goto close;

        memcpy(sw_frame->data[0], dst_data[0], size);
        memcpy(sw_frame->data[1], dst_data[1], size/2);

        //if ((err = fread((uint8_t*)(sw_frame->data[0]), size, 1, fin)) <= 0) // read Y(NV12)
        //    break;
        //if ((err = fread((uint8_t*)(sw_frame->data[1]), size / 2 , 1, fin)) <= 0) // read UV(NU12)
        //    break;

        if (!(hw_frame = av_frame_alloc())) {
            err = AVERROR(ENOMEM);
            goto close;
        }

        if ((err = av_hwframe_get_buffer(avctx->hw_frames_ctx, hw_frame, 0)) < 0) {
            fprintf(stderr, "Error code: %s.\n", av_err2str(err));
            goto close;
        }
        if (!hw_frame->hw_frames_ctx) {
            err = AVERROR(ENOMEM);
            goto close;
        }
        if ((err = av_hwframe_transfer_data(hw_frame, sw_frame, 0)) < 0) {
            fprintf(stderr, "Error while transferring frame data to surface."
                "Error code: %s.\n", av_err2str(err));
            goto close;
        }

        if ((err = (encode_write(avctx, hw_frame, fout))) < 0) {
            fprintf(stderr, "Failed to encode.\n");
            goto close;
        }
        av_frame_free(&hw_frame);
        av_frame_free(&sw_frame);
    }

    /* flush encoder */
    err = encode_write(avctx, NULL, fout);
    if (err == AVERROR_EOF)
        err = 0;

close:
    if (fin)
        fclose(fin);
    if (fout)
        fclose(fout);


    sws_freeContext(_swsCtx);
    av_frame_free(&sw_frame);
    av_frame_free(&hw_frame);
    avcodec_free_context(&avctx);
    av_buffer_unref(&hw_device_ctx);

    return err;
}