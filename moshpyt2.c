// 11-17-2018
// vipyne
// http://dranger.com/ffmpeg/tutorial01.html

#include <stdio.h>
#include <errno.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/motion_vector.h>

#define INBUF_SIZE 4096

FILE *f;

const char *vector_src_filename = NULL;
const char *image_src_filename = NULL;
const char *out_filename = NULL;

int vp_error(char *message)
{
  printf("%s\n ", message);
  return -1;
}

static void vp_encode(AVCodecContext *codec_ctx,
                      AVFrame *frame,
                      AVPacket *pkt,
                      FILE *outfile)
{
  int ret;

  ret = avcodec_send_frame(codec_ctx, frame);
  if (0 > ret)
  {
    printf("error sending frame for encoding: %s\n", av_err2str(ret));
    exit(1);
  }

  while ( 0 <= ret)
  {
    ret = avcodec_receive_packet(codec_ctx, pkt);
    if (AVERROR(EAGAIN) == ret)
    {
      printf("e_EAGAIN\n");
      return;
    }
    else if (AVERROR_EOF == ret)
    {
      printf("e_AVERROR_EOF\n");
      return;
    }
    else if (0 > ret)
    {
      printf("error during encoding\n");
      exit(1);
    }

    printf(">>> > > encoded frame %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
    fwrite(pkt->data, 1, pkt->size, outfile);
    av_packet_unref(pkt);
  }
}

static void vp_decode(AVCodecContext *codec_ctx,
                      AVFrame *frame,
                      AVPacket *pkt,
                      const char *filename,
                      FILE *out_file)
{
  char buf[1024];
  int ret;

  ret = avcodec_send_packet(codec_ctx, pkt);
    fprintf(stderr, "HI__ret_ %d\n", ret);
  if (0 > ret)
  {
    printf("error sending packet for decoding: %s\n", av_err2str(ret));
    exit(1);
  }

  while (0 <= ret)
  {

    ret = avcodec_receive_frame(codec_ctx, frame);
    if (AVERROR(EAGAIN) == ret)
    {
      printf("d_EAGAIN\n");
      return;
    }
    else if (AVERROR_EOF == ret)
    {
      printf("d_AVERROR_EOF\n");
      return;
    }
    else if (0 > ret)
    {
      printf("error during decoding\n");
      exit(1);
    }
  }

  printf("saving frame %3d\n", codec_ctx->frame_number);
  fflush(stdout);
  snprintf(buf, sizeof(buf), filename, codec_ctx->frame_number);

  vp_encode(codec_ctx, frame, pkt, out_file);
}

int main(int argc, char *argv[])
{
  const char *vector_filename, *codec_name, *out_filename;
  FILE *out_file;

  vector_filename = argv[1];
  // image_filename = argv[2];
  out_filename = argv[2];

  // register all codecs
  av_register_all();
  ////////////////////////////////////   Motion vector src
  FILE *vector_file;
  AVCodecParserContext *vector_parser = NULL;
  AVCodecContext *vector_ctx = NULL;
  AVCodec *vector_codec = NULL;
  AVFrame *vector_frame;
  AVPacket *vector_pkt;

  uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
  uint8_t *data;
  size_t data_size;

  int ret;

  ////////////////////////////////////   Vector src
  vector_pkt = av_packet_alloc();
  if (!vector_pkt) {
    fprintf(stderr, "could not alloc packet\n");
    exit(1);
  }

  memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

  vector_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!vector_codec) {
    fprintf(stderr, "codec not found \n");
    exit(1);
  }

  vector_parser = av_parser_init(vector_codec->id);
  if (!vector_parser) {
    fprintf(stderr, "parser not found \n");
    exit(1);
  }

  vector_ctx = avcodec_alloc_context3(vector_codec);
  if (!vector_codec) {
    fprintf(stderr, "could not alloc codec context\n");
    exit(1);
  }

  if (0 > avcodec_open2(vector_ctx, vector_codec, NULL)) {
    fprintf(stderr, "could not open codec\n");
    exit(1);
  }

  vector_file = fopen(vector_filename, "rb");
  if (!vector_file) {
    fprintf(stderr, "could not open '%s'\n", vector_filename);
    exit(1);
  }

  vector_frame = av_frame_alloc();
  if (!vector_frame) {
    fprintf(stderr, "could not alloc frame\n");
    exit(1);
  }



  ////////////////////////////////////   OUTPUT
  AVPacket *out_pkt;
  AVFrame *out_frame;
  AVCodec *out_codec;
  AVCodecContext *out_ctx = NULL;

  uint8_t endcode[] = { 0, 0, 1, 0xb7 };

  int i, x, y;
  ////////////////////////////////////   out
  out_pkt = av_packet_alloc();
  if (!out_pkt) {
    fprintf(stderr, "could not alloc packet\n");
    exit(1);
  }

  out_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
  // out_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!out_codec) {
    printf("no out codec found\n");
    exit(1);
  }

  out_ctx = avcodec_alloc_context3(out_codec);
  if (!vector_codec) {
    fprintf(stderr, "could not alloc out codec context\n");
    exit(1);
  }

  out_ctx->bit_rate = 400000;
  out_ctx->width = 1920;
  out_ctx->height = 1080;
  out_ctx->time_base = (AVRational){1, 30};
  out_ctx->framerate = (AVRational){30, 1};
  out_ctx->gop_size = 10;
  out_ctx->max_b_frames = 1;
  out_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  if (AV_CODEC_ID_H264 == out_codec->id) {
    av_opt_set(out_ctx->priv_data, "preset", "slow", 0);
  }

  ret = avcodec_open2(out_ctx, out_codec, NULL);
  if (0 > ret) {
    printf("could not open codec\n", av_err2str(ret));
    exit(1);
  }

  out_file = fopen(out_filename, "wb");
  if (!out_file) {
    printf("Coult not open %s.\n", out_filename);
    exit(1);
  }

  out_frame = av_frame_alloc();
  if (!out_frame) {
    fprintf(stderr, "could not alloc frame\n");
  }

  out_frame->format = out_ctx->pix_fmt;
  out_frame->width = out_ctx->width;
  out_frame->height = out_ctx->height;

  ret = av_frame_get_buffer(out_frame, 32);
  if (0 > ret)
  {
    printf("could not alloc frame data. (av_frame_get_buffer)\n");
  }

  /////////////// image frames
  for (i = 0; i < (30*2); i++)
  {
    fflush(stdout);

    ret = av_frame_make_writable(out_frame);
    if (0 > ret)
    {
      printf("frame could not be made writable.\n");
      exit(1);
    }

    //  Y
    for (y = 0; y < out_ctx->height; y++)
    {
      for (x = 0; x < out_ctx->width; x++)
      {
        out_frame->data[0][y * out_frame->linesize[0] + x] = x + y + i * 3;
      }
    }

    //  Cb & Cr
    for (y = 0; y < out_ctx->height / 2; y++)
    {
      for (x = 0; x < out_ctx->width / 2; x++)
      {
        out_frame->data[1][y * out_frame->linesize[1] + x] = 128 + y + i * 2;
        out_frame->data[2][y * out_frame->linesize[2] + x] = 64 + x + i * 5;
      }
    }

    out_frame->pts = i;

    vp_encode(out_ctx, out_frame, out_pkt, out_file);
  }

  /////////////// vector frames
  // while (!feof(vector_file)) {
  //   data_size = fread(inbuf, 1, INBUF_SIZE, vector_file);
  //   if (!data_size) {
  //     break;
  //   }

  //   data = inbuf;
  //   while (0 < data_size) {
  //     ret = av_parser_parse2(vector_parser,
  //                             vector_ctx,
  //                             &vector_pkt->data,
  //                             &vector_pkt->size,
  //                             data,
  //                             data_size,
  //                             AV_NOPTS_VALUE,
  //                             AV_NOPTS_VALUE,
  //                             0);
  //     if (0 > ret) {
  //       fprintf(stderr, "error while parsing\n");
  //       exit(1);
  //     }
  //     data      += ret;
  //     data_size -= ret;

  //     if (vector_pkt->size) {
  //       vp_decode(vector_ctx, out_frame, vector_pkt, out_filename, out_file);
  //     }
  //   }
  // }


  vp_encode(out_ctx, NULL, out_pkt, out_file);
  vp_decode(vector_ctx, vector_frame, NULL, out_filename, out_file);

  fwrite(endcode, 1, sizeof(endcode), out_file);
  fclose(out_file);

  fclose(vector_file);

  av_parser_close(vector_parser);
  avcodec_free_context(&vector_ctx);
  av_frame_free(&vector_frame);
  av_packet_free(&vector_pkt);

  return 0;
}
