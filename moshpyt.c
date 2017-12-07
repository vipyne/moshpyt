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
// #include <libavfilter/vf_codecview.h> // TODO: find this file

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

static void draw_line(uint8_t *buf, int sx, int sy, int ex, int ey,
                      int w, int h, int stride, int color)
{
    int x, y, fr, f;

    if (clip_line(&sx, &sy, &ex, &ey, w - 1))
        return;
    if (clip_line(&sy, &sx, &ey, &ex, h - 1))
        return;

    sx = av_clip(sx, 0, w - 1);
    sy = av_clip(sy, 0, h - 1);
    ex = av_clip(ex, 0, w - 1);
    ey = av_clip(ey, 0, h - 1);

    buf[sy * stride + sx] += color;

    if (FFABS(ex - sx) > FFABS(ey - sy)) {
        if (sx > ex) {
            FFSWAP(int, sx, ex);
            FFSWAP(int, sy, ey);
        }
        buf += sx + sy * stride;
        ex  -= sx;
        f    = ((ey - sy) << 16) / ex;
        for (x = 0; x <= ex; x++) {
            y  = (x * f) >> 16;
            fr = (x * f) & 0xFFFF;
                   buf[ y      * stride + x] += (color * (0x10000 - fr)) >> 16;
            if(fr) buf[(y + 1) * stride + x] += (color *            fr ) >> 16;
        }
    } else {
        if (sy > ey) {
            FFSWAP(int, sx, ex);
            FFSWAP(int, sy, ey);
        }
        buf += sx + sy * stride;
        ey  -= sy;
        if (ey)
            f = ((ex - sx) << 16) / ey;
        else
            f = 0;
        for(y= 0; y <= ey; y++){
            x  = (y*f) >> 16;
            fr = (y*f) & 0xFFFF;
                   buf[y * stride + x    ] += (color * (0x10000 - fr)) >> 16;
            if(fr) buf[y * stride + x + 1] += (color *            fr ) >> 16;
        }
    }
}
static void draw_arrow(uint8_t *buf, int sx, int sy, int ex,
                       int ey, int w, int h, int stride, int color, int tail, int direction)
{
  int dx,dy;

  if (direction) {
    FFSWAP(int, sx, ex);
    FFSWAP(int, sy, ey);
  }

  sx = av_clip(sx, -100, w + 100);
  sy = av_clip(sy, -100, h + 100);
  ex = av_clip(ex, -100, w + 100);
  ey = av_clip(ey, -100, h + 100);

  dx = ex - sx;
  dy = ey - sy;

  if (dx * dx + dy * dy > 3 * 3) {
    int rx =  dx + dy;
    int ry = -dx + dy;
    int length = sqrt((rx * rx + ry * ry) << 8);

    // FIXME subpixel accuracy
    rx = ROUNDED_DIV(rx * 3 << 4, length);
    ry = ROUNDED_DIV(ry * 3 << 4, length);

    if (tail) {
      rx = -rx;
      ry = -ry;
    }

    draw_line(buf, sx, sy, sx + rx, sy + ry, w, h, stride, color);
    draw_line(buf, sx, sy, sx - ry, sy + rx, w, h, stride, color);
  }
  draw_line(buf, sx, sy, ex, ey, w, h, stride, color);
}

static int decode_packet(int *got_frame,
  int cached,
  AVPacket *vec_pkt,
  AVPacket *img_pkt,
  AVFormatContext *ofmt_ctx,
  int *img_got_frame,
  AVPacket *write_to_pkt) {
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
          // const int direction = mv->source > 0;

          // if (s->mv_type) {
            // const int is_fp = direction == 0 && (s->mv_type & MV_TYPE_FOR);
            // const int is_bp = direction == 1 && (s->mv_type & MV_TYPE_BACK);

            // if ((!s->frame_type && (is_fp || is_bp)) ||
            //     is_iframe && is_fp || is_iframe && is_bp ||
            //     is_pframe && is_fp ||
            //     is_bframe && is_fp || is_bframe && is_bp)

              // draw_arrow(
                  img_frame->data[0] = frame->data[0];
                  img_mv->dst_x = mv->dst_x;
                  img_mv->dst_y = mv->dst_y;
                  img_mv->src_x = mv->src_x;
                  img_mv->src_y = mv->src_y;
                  img_frame->width = frame->width;
                  img_frame->height = frame->height;
                  img_frame->linesize[0] = frame->linesize[0];
              // );

                  img_frame->data[0] += 2;


        // } else if (s->mv)
                  // DRAW ARROW FUNCTION
        //     if ((direction == 0 && (s->mv & MV_P_FOR)  && frame->pict_type == AV_PICTURE_TYPE_P) ||
        //         (direction == 0 && (s->mv & MV_B_FOR)  && frame->pict_type == AV_PICTURE_TYPE_B) ||
        //         (direction == 1 && (s->mv & MV_B_BACK) && frame->pict_type == AV_PICTURE_TYPE_B))
        //         draw_arrow(frame->data[0], mv->dst_x, mv->dst_y, mv->src_x, mv->src_y,
        //                     frame->width, frame->height, frame->linesize[0],
        //                     100, 0, direction);
        }

      } else {
          printf(".\n");
      }
    }
  }
  write_to_pkt = img_pkt;
  av_interleaved_write_frame(ofmt_ctx, write_to_pkt); /////////////////////
  printf("I\n");
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

  if (avformat_open_input(&fmt_ctx, vector_src_filename, NULL, NULL) < 0) {
    fprintf(stderr, "Could not open source file %s\n", vector_src_filename);
    exit(1);
  }

  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    exit(1);
  }

  if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
    video_stream = fmt_ctx->streams[video_stream_idx];
    video_dec_ctx = video_stream->codec;
  }

  av_dump_format(fmt_ctx, 0, vector_src_filename, 0);

/////////////////////////   /////////////////////
  if (avformat_open_input(&img_fmt_ctx, image_src_filename, NULL, NULL) < 0) {
    fprintf(stderr, "Could not open source file %s\n", image_src_filename);
    exit(1);
  }

  if (avformat_find_stream_info(img_fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    exit(1);
  }

  if (open_codec_context(&image_video_stream_idx, img_fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
    image_video_stream = img_fmt_ctx->streams[image_video_stream_idx];
    image_video_dec_ctx = image_video_stream->codec;
  }

  // av_dump_format last arg 0 for input, 1 for output
  av_dump_format(img_fmt_ctx, 0, image_src_filename, 0);
/////////////////////////   /////////////////////




  if (!video_stream) {
    fprintf(stderr, "Could not find video stream in the input, aborting\n");
    ret = 1;
    goto end;
  }

  // frame = av_frame_alloc(); ////////// ??


  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output_filename);
  if (!ofmt_ctx) {
    fprintf(stderr, "Could not create output context\n");
    ret = AVERROR_UNKNOWN;
    goto end;
  }

  ofmt = ofmt_ctx->oformat;

  //////////////////
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

  av_dump_format(ofmt_ctx, 0, output_filename, 1);
  //////////////////

  if (!(ofmt->flags & AVFMT_NOFILE)) {
    ret = avio_open(&ofmt_ctx->pb, output_filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Could not open output file '%s'", output_filename);
      goto end;
    }
  }

  int header_written = avformat_write_header(ofmt_ctx, NULL); ///////////

  if (header_written < 0) {
    fprintf(stderr, "who knows why header wasn't written: \n");
    fprintf(stderr, "Error occurred when opening output file\n");
  }

  frame = av_frame_alloc(); //////////
  img_frame = av_frame_alloc(); //////////
  if (!frame || !img_frame) {
    fprintf(stderr, "Could not allocate frame\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  /* initialize packet, set data to NULL, let the demuxer fill it */
  av_init_packet(&write_to_pkt);
  write_to_pkt.data = NULL;
  write_to_pkt.size = 0;

  int img_got_frame;

  /* read frames from the file */
  while (av_read_frame(fmt_ctx, &vec_pkt) >= 0) {
    av_read_frame(img_fmt_ctx, &img_pkt);
    AVStream *in_stream, *out_stream;

    do {
      got_frame = 0;
      img_got_frame = 0;

      in_stream  = fmt_ctx->streams[vec_pkt.stream_index];
      out_stream = ofmt_ctx->streams[vec_pkt.stream_index];
      ret = decode_packet(&got_frame, 0, &vec_pkt, &img_pkt, ofmt_ctx, &img_got_frame, &write_to_pkt);

      write_to_pkt = vec_pkt; ///////////////////////////////////////////////////////////////
      av_interleaved_write_frame(ofmt_ctx, &write_to_pkt); /////////////////////

      if (ret < 0)
          break;
      vec_pkt.data += ret;
      vec_pkt.size -= ret;
    } while ( (vec_pkt.size > 0) && (0 != ret));
  }
  /* flush cached frames */
  write_to_pkt.data = NULL;
  write_to_pkt.size = 0;
  av_write_trailer(ofmt_ctx); /////////////////

  end:
    avcodec_close(video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    return ret < 0;
}
