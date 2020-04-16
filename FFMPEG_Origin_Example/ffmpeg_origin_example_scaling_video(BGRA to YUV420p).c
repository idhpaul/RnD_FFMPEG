/*
 * Copyright (c) 2012 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 /**
  * @file
  * libswscale API use example.
  * @example scaling_video.c
  */
#pragma warning(disable : 4996)

#include <stdio.h>

#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <libswscale/swscale.h>

#pragma comment(lib,"swscale.lib")
#pragma comment( lib, "avutil.lib" )

char input_url[] = "test_bgra.bgra";

static void fill_yuv_image(uint8_t* data[4], int linesize[4],
    int width, int height, int frame_index)
{
    int x, y;
    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            data[0][y * linesize[0] + x] = x + y + frame_index * 3;
    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            data[1][y * linesize[1] + x] = 128 + y + frame_index * 2;
            data[2][y * linesize[2] + x] = 64 + x + frame_index * 5;
        }
    }
}

static void read_file_bgra(FILE* srcfile, uint8_t* data[4], int linesize[4], int width, int height, int frame_index)
{
    int err;

    size_t readsize = linesize[0];

    if ((err = fread(data[0], (width*height), 1, srcfile)) <= 0) // read Packed BGRA data
    {
        printf("Failed to read BGRA data from file : %d\n", err);
    }

    printf("read data : %d\n", err);
}

static void write_file_yuv420(FILE* dstfile, uint8_t* data[4], int linesize[4], int width, int height, int frame_index)
{
    int err;

    if ((err = fwrite((uint8_t*)(data[0][linesize[0]]), linesize[0], 1, dstfile)) <= 0) // write Y(YUV420p)
    {
        printf("Failed to Write Y data from file : %d\n", err);
    }

    if ((err = fwrite((uint8_t*)(data[1][linesize[1]]), linesize[1], 1, dstfile)) <= 0) // write U(YUV420p)
    {
        printf("Failed to Write Y data from file : %d\n", err);
    }

    if ((err = fwrite((uint8_t*)(data[2][linesize[2]]), linesize[2], 1, dstfile)) <= 0) // write V(YUV420p)
    {
        printf("Failed to Write Y data from file : %d\n", err);
    }
}

int main(int argc, char** argv)
{
    uint8_t* src_data[4], * dst_data[4];
    int src_linesize[4], dst_linesize[4];
    int src_w = 320, src_h = 180, dst_w, dst_h;
    enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_BGRA, dst_pix_fmt = AV_PIX_FMT_YUV420P/*AV_PIX_FMT_RGB24*/;
    const char* dst_size = NULL;
    const char* dst_filename = NULL;
    FILE* fin = NULL;
    FILE* dst_file = NULL;
    int dst_bufsize;
    struct SwsContext* sws_ctx;
    int i, ret;
    if (argc != 3) {
        fprintf(stderr, "Usage: %s output_file output_size\n"
            "API example program to show how to scale an image with libswscale.\n"
            "This program generates a series of pictures, rescales them to the given "
            "output_size and saves them to an output file named output_file\n."
            "\n", argv[0]);
        exit(1);
    }

    if (!(fin = fopen(input_url, "rb"))) {
        fprintf(stderr, "Fail to open input file : %s\n", strerror(errno));
        return -1;
    }


    dst_filename = argv[1];
    dst_size = argv[2];
    if (av_parse_video_size(&dst_w, &dst_h, dst_size) < 0) {
        fprintf(stderr,
            "Invalid size '%s', must be in the form WxH or a valid size abbreviation\n",
            dst_size);
        exit(1);
    }
    dst_file = fopen(dst_filename, "wb");
    if (!dst_file) {
        fprintf(stderr, "Could not open destination file %s\n", dst_filename);
        exit(1);
    }
    /* create scaling context */
    sws_ctx = sws_getContext(src_w, src_h, src_pix_fmt,
        dst_w, dst_h, dst_pix_fmt,
        SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr,
            "Impossible to create scale context for the conversion "
            "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
            av_get_pix_fmt_name(src_pix_fmt), src_w, src_h,
            av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h);
        ret = AVERROR(EINVAL);
        goto end;
    }
    /* allocate source and destination image buffers */
    if ((ret = av_image_alloc(src_data, src_linesize,
        src_w, src_h, src_pix_fmt, 1)) < 0) {
        fprintf(stderr, "Could not allocate source image\n");
        goto end;
    }
    /* buffer is going to be written to rawvideo file, no alignment */
    if ((ret = av_image_alloc(dst_data, dst_linesize,
        dst_w, dst_h, dst_pix_fmt, 16)) < 0) {
        fprintf(stderr, "Could not allocate destination image\n");
        goto end;
    }
    dst_bufsize = ret;

    int err = 0;

    for (i = 0; i < 100; i++) {
        /* generate synthetic video */
        //fill_yuv_image(src_data, src_linesize, src_w, src_h, i);
        //read_file_bgra(fin, src_data, src_linesize, src_w, src_h, i);

        uint8_t* temp = (uint8_t*)malloc(src_linesize[0]);

        if ((err = fread((uint8_t*)temp, src_linesize[0], 1, fin)) <= 0) // read Packed BGRA data
        {
            printf("Failed to read BGRA data from file : %d\n", err);
        }

        src_data[0] = temp;
        
        /* convert to destination format */
        sws_scale(sws_ctx, (const uint8_t* const*)src_data,
            src_linesize, 0, src_h, dst_data, dst_linesize);

        write_file_yuv420(dst_file, dst_data, dst_linesize, dst_w, dst_h, i);

        free(temp);

        /* write scaled image to file */
        //fwrite(dst_data[0], 1, dst_bufsize, dst_file);

        printf("Processing : %d\n", i + 1);
    }
    fprintf(stderr, "Scaling succeeded. Play the output file with the command:\n"
        "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
        av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h, dst_filename);
end:
    fclose(dst_file);
    av_freep(&src_data[0]);
    av_freep(&dst_data[0]);
    sws_freeContext(sws_ctx);
    return ret < 0;
}
