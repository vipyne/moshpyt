/*
 * Copyright (c) 2012 Stefano Sabatini
 * Copyright (c) 2014 Clément Bœsch
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

#include <libavutil/timestamp.h>
#include <libavutil/motion_vector.h>
#include <libavformat/avformat.h>

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static AVCodecContext *image_video_dec_ctx = NULL;
static AVStream *video_stream = NULL;
static AVStream *image_video_stream = NULL;
static const char *vector_src_filename = NULL;
static const char *image_src_filename = NULL;
static const char *output_filename = NULL;

static int video_stream_idx = -1;
static int image_video_stream_idx = -1;
static AVFrame *frame = NULL;
static AVFrame *out_frame = NULL;
static AVFrame *img_frame = NULL;
static int video_frame_count = 0;

static int decode_packet(int *got_frame,
  int cached,
  AVPacket *vec_pkt,
  AVPacket *img_pkt,
  AVFormatContext *ofmt_ctx,
  int *img_got_frame,
  AVPacket *write_to_pkt)
{
  int decoded = vec_pkt->size;

  *got_frame = 0;
  *img_got_frame = 0;

  if (vec_pkt->stream_index == video_stream_idx) {
    int ret2 = avcodec_decode_video2(image_video_dec_ctx, img_frame, img_got_frame, img_pkt);
    int ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, vec_pkt);

    if (ret < 0 || ret2 < 0) {
      fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
      return ret;
    }

    if (*got_frame) {
      int i;
      AVFrameSideData *sd;
      AVFrameSideData *img_sd;

      video_frame_count++;
      sd = av_frame_get_side_data(img_frame, AV_FRAME_DATA_MOTION_VECTORS);
      if (sd) {

        AVMotionVector *mvs = (AVMotionVector *)sd->data;
        AVMotionVector *img_mvs = (AVMotionVector *)img_frame->side_data[0]->data;

        for (i = 0; i < sd->size / sizeof(*mvs); i++) {
          AVMotionVector *mv = &mvs[i];
          AVMotionVector *img_mv = &img_mvs[i];

          img_frame->data[0] = frame->data[0];
          img_mv->dst_x = mv->dst_x;
          img_mv->dst_y = mv->dst_y;
          img_mv->src_x = mv->src_x;
          img_mv->src_y = mv->src_y;
          img_frame->width = frame->width;
          img_frame->height = frame->height;
          img_frame->linesize[0] = frame->linesize[0];

          img_frame->data[0] += 2;
        }

      } else {
          printf(".\n");
      }
    }
  }
  write_to_pkt = img_pkt;
  av_interleaved_write_frame(ofmt_ctx, write_to_pkt);
  printf("decoded__ %d\n", decoded);
  return decoded;
}

static int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type) {
  int ret;
  AVStream *st;
  AVCodecContext *dec_ctx = NULL;
  AVCodec *dec = NULL;
  AVDictionary *opts = NULL;

  ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
  if (ret < 0) {
    fprintf(stderr, "Could not find %s stream in input file '%s'\n",
            av_get_media_type_string(type), vector_src_filename);
    return ret;
  } else {
    *stream_idx = ret;
    st = fmt_ctx->streams[*stream_idx];

    /* find decoder for the stream */
    dec_ctx = st->codec;
    dec = avcodec_find_decoder(dec_ctx->codec_id);
    if (!dec) {
      fprintf(stderr, "Failed to find %s codec\n",
              av_get_media_type_string(type));
      return AVERROR(EINVAL);
    }

    /* Init the video decoder */
    av_dict_set(&opts, "flags2", "+export_mvs", 0);
    if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
      fprintf(stderr, "Failed to open %s codec\n",
              av_get_media_type_string(type));
      return ret;
    }
  }

  return 0;
}

int main(int argc, char **argv) {
  int ret = 0, got_frame;

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <vector_video> <image_video> <output_video>\n", argv[0]);
    exit(1);
  }
  vector_src_filename = argv[1];
  image_src_filename = argv[2];
  output_filename = argv[3];
  AVOutputFormat *ofmt = NULL;
  AVFormatContext *ofmt_ctx = NULL;
  AVFormatContext *img_fmt_ctx = NULL;
  AVPacket vec_pkt;
  AVPacket img_pkt;
  AVPacket write_to_pkt;

  av_register_all();

  // vector video
  {
    if (avformat_open_input(&fmt_ctx, vector_src_filename, NULL, NULL) < 0) {
      fprintf(stderr, "Could not open source file %s\n", vector_src_filename);
      exit(1);
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
      fprintf(stderr, "Could not find stream information\n");
      exit(1);
    }

    // open codec context for video stream
    if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
      video_stream = fmt_ctx->streams[video_stream_idx];
      video_dec_ctx = video_stream->codec;
    }

    if (!video_stream) {
      fprintf(stderr, "Could not find video stream in the input, aborting\n");
      ret = 1;
      goto end;
    }

    // av_dump_format last arg 0 for input, 1 for output
    // av_dump_format vector_src_filename
    av_dump_format(fmt_ctx, 0, vector_src_filename, 0);
  }

  // image video
  {
    if (avformat_open_input(&img_fmt_ctx, image_src_filename, NULL, NULL) < 0) {
      fprintf(stderr, "Could not open source file %s\n", image_src_filename);
      exit(1);
    }

    if (avformat_find_stream_info(img_fmt_ctx, NULL) < 0) {
      fprintf(stderr, "Could not find stream information\n");
      exit(1);
    }

    // open codec context for image video stream
    if (open_codec_context(&image_video_stream_idx, img_fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
      image_video_stream = img_fmt_ctx->streams[image_video_stream_idx];
      image_video_dec_ctx = image_video_stream->codec;
    }

    if (!image_video_stream) {
      fprintf(stderr, "Could not find image video stream in the input, aborting\n");
      ret = 1;
      goto end;
    }

    // av_dump_format last arg 0 for input, 1 for output
    // av_dump_format image_src_filename
    av_dump_format(img_fmt_ctx, 0, image_src_filename, 0);
  }

  // output video
  {
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output_filename);

    if (!ofmt_ctx) {
      fprintf(stderr, "Could not create output context\n");
      ret = AVERROR_UNKNOWN;
      goto end;
    }

    ofmt = ofmt_ctx->oformat;

    AVStream *in_stream = fmt_ctx->streams[0];
    AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
    if (!out_stream) {
      fprintf(stderr, "Failed allocating output stream\n");
      ret = AVERROR_UNKNOWN;
      goto end;
    }

    ret = avcodec_copy_context(out_stream->codec, in_stream->codec);

    if (ret < 0) {
      fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
      goto end;
    }

    out_stream->codec->codec_tag = 0;

    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // av_dump_format last arg 0 for input, 1 for output
    // av_dump_format output_filename
    av_dump_format(ofmt_ctx, 0, output_filename, 1);

    if (!(ofmt->flags & AVFMT_NOFILE)) {
      ret = avio_open(&ofmt_ctx->pb, output_filename, AVIO_FLAG_WRITE);
      if (ret < 0) {
        fprintf(stderr, "Could not open output file '%s'", output_filename);
        goto end;
      }
    }
  }

  // write header
  int header_written = avformat_write_header(ofmt_ctx, NULL);

  if (header_written < 0) {
    fprintf(stderr, "who knows why header wasn't written: \n");
    fprintf(stderr, "Error occurred when opening output file\n");
  }

  frame = av_frame_alloc();
  img_frame = av_frame_alloc();
  if (!frame || !img_frame) {
    fprintf(stderr, "Could not allocate frame\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  // initialize packet, set data to NULL, let the demuxer fill it
  av_init_packet(&write_to_pkt);
  write_to_pkt.data = NULL;
  write_to_pkt.size = 0;

  int img_got_frame;

  // read frames from the import "vector" video file
  while (av_read_frame(fmt_ctx, &vec_pkt) >= 0) {

    av_read_frame(img_fmt_ctx, &img_pkt);
    AVStream *in_stream, *out_stream;

    do {
      got_frame = 0;
      img_got_frame = 0;

      in_stream  = fmt_ctx->streams[vec_pkt.stream_index];
      out_stream = ofmt_ctx->streams[vec_pkt.stream_index];
      ret = decode_packet(&got_frame, 0, &vec_pkt, &img_pkt, ofmt_ctx, &img_got_frame, &write_to_pkt);

      write_to_pkt = vec_pkt;
      av_interleaved_write_frame(ofmt_ctx, &write_to_pkt);

      if (ret < 0)
          break;
      vec_pkt.data += ret;
      vec_pkt.size -= ret;
    } while ( (vec_pkt.size > 0) && (0 != ret));

  }

  // flush cached frames
  write_to_pkt.data = NULL;
  write_to_pkt.size = 0;
  av_write_trailer(ofmt_ctx);

  end:
    avcodec_close(video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    return ret < 0;
}
