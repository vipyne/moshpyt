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
const char *output_filename = NULL;

AVFrame *vector_frame = NULL;
AVPacket *vector_pkt;

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

    fwrite(pkt->data, 1, pkt->size, outfile);
    av_packet_unref(pkt);
    printf(">>> > > encoded frame %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
  }
  printf("(after while)\n");
}

static void vp_decode(AVCodecContext *codec_ctx,
                      AVFrame *frame,
                      AVPacket *pkt,
                      const char *filename,
                      FILE *output_file)
{
  char buf[1024];
  int ret;

  ret = avcodec_send_packet(codec_ctx, pkt);
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

  vp_encode(codec_ctx, frame, pkt, output_file);
}

int main(int argc, char *argv[])
{
  vector_src_filename = argv[1];
  image_src_filename = argv[2];
  output_filename = argv[3];

  // register all codecs
  av_register_all();
  ////////////////////////////////////   Motion vector src
  FILE *vector_file;
  AVCodecContext *vector_codec_context = NULL;
  AVCodecContext *vector_codec_context_orig = NULL;
  AVCodec *vector_codec = NULL;
  AVFormatContext *vector_format_ctx = NULL;

  {
    // Open file
    if (0 != avformat_open_input(&vector_format_ctx, vector_src_filename, NULL, 0))
    {
      printf("Could not open file: %s", vector_src_filename);
      return -1;
    }

    // Get stream info
    if (0 > avformat_find_stream_info(vector_format_ctx, NULL))
    {
      printf("Could not get stream info: %d", avformat_find_stream_info(vector_format_ctx, NULL));
      return -1;
    }

    av_dump_format(vector_format_ctx, 0, vector_src_filename, 0);

    // Get video stream
    {
      int i;
      int videoStream = -1;

      for (i = 0; i < vector_format_ctx->nb_streams; i++)
      {
        printf("streams %d\n", i);
        printf("stream- %d\n", vector_format_ctx->streams[i]->codecpar->codec_id);
        if (AVMEDIA_TYPE_VIDEO == vector_format_ctx->streams[i]->codecpar->codec_id)
        {
          videoStream = i;
          printf("sup.    %d\n", i);
          break;
        }
      }
      if (-1 == videoStream)
      {
        vp_error("couldn't find video stream.");
      }
      printf("can't find videoStream but I just know it is 0, ignore %d\n", videoStream);
    }

    // Get Codec
    vector_codec = avcodec_find_decoder(vector_format_ctx->streams[0]->codecpar->codec_id);
    if (NULL == vector_codec)
    {
      vp_error("codec is wrong.");
    }

    // Create Codec Context
    vector_codec_context = avcodec_alloc_context3(vector_codec);
    if ( 0 > avcodec_parameters_from_context(vector_format_ctx->streams[0]->codecpar, vector_codec_context))
    {
      vp_error("couldn't copy codec context.");
    }
    // Open Codec Context
    if (0 > avcodec_open2(vector_codec_context, vector_codec, NULL))
    {
      vp_error("avcodec_open2 barfed");
    }
  }

  // ////////////////////////////////////   Image src


  ////////////////////////////////////   OUTPUT
  FILE *output_file;
  AVPacket *output_pkt;
  AVFrame *output_frame;
  AVFormatContext *output_format_ctx = NULL;
  AVCodecContext *output_codec_ctx = NULL;
  avformat_alloc_output_context2(&output_format_ctx, NULL, NULL, output_filename);
  // AVStream *in_stream = vector_format_ctx->streams[0];
  // AVStream *out_stream = avformat_new_stream(output_format_ctx, in_stream->codec->codec);
  // avcodec_copy_context(out_stream->codec, in_stream->codec);
  avio_open(&output_format_ctx->pb, output_filename, AVIO_FLAG_WRITE);

  av_dump_format(output_format_ctx, 0, output_filename, 1);

  output_file = fopen(output_filename, "wb");
  if (!output_file)
  {
    printf("Coult not open %s.\n", output_filename);
  }

  AVCodec *output_codec;
  output_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  // output_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
  if (!output_codec)
  {
    printf("no codec found\n");
    exit(1);
  }

  output_codec_ctx = avcodec_alloc_context3(output_codec);
  output_frame = av_frame_alloc();
  output_pkt = av_packet_alloc();

  int fb_ret, i, x, y;

  output_codec_ctx->bit_rate = 400000;
  output_codec_ctx->width = 1920;
  output_codec_ctx->height = 1080;
  output_codec_ctx->time_base = (AVRational){1, 30};
  output_codec_ctx->framerate = (AVRational){30, 1};
  output_codec_ctx->gop_size = 10;
  output_codec_ctx->max_b_frames = 1;
  output_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  if (0 > avcodec_open2(output_codec_ctx, output_codec, NULL))
  {
    printf("could not open codec\n");
    exit(1);
  }

  output_frame->format = AV_PIX_FMT_YUV420P;
  output_frame->width = 1920;
  output_frame->height = 1080;

  fb_ret = av_frame_get_buffer(output_frame, 32);
  if (0 > fb_ret)
  {
    printf("could not alloc frame data.\n");
  }

  // avformat_alloc_output_context2(&output_format_ctx, NULL, NULL, output_filename);
  av_dump_format(output_format_ctx, 0, output_filename, 1);

  vector_frame = av_frame_alloc();
  av_init_packet(&vector_pkt);

  printf("narrrrrfffff____________________________________\n");
  for (i = 0; i < (30*1); i++)
  {
    fflush(stdout);

    fb_ret = av_frame_make_writable(output_frame);
    if (0 > fb_ret)
    {
      printf("frame could not be made writable.\n");
      exit(1);
    }

    //  Y
    for (y = 0; y < output_codec_ctx->height; y++)
    {
      for (x = 0; x < output_codec_ctx->width; x++)
      {
        output_frame->data[0][y * output_frame->linesize[0] + x] = x + y + i * 3;
      }
    }

    //  Cb & Cr
    for (y = 0; y < output_codec_ctx->height / 2; y++)
    {
      for (x = 0; x < output_codec_ctx->width / 2; x++)
      {
        output_frame->data[1][y * output_frame->linesize[1] + x] = 128 + y + i * 2;
        output_frame->data[2][y * output_frame->linesize[2] + x] = 64 + x + i * 5;
      }
    }

    output_frame->pts = i;

    vp_encode(output_codec_ctx, output_frame, output_pkt, output_file);
  }

  // uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
  // uint8_t *data;
  // size_t data_size;

  // AVCodecParserContext *parser;

  // errno = 0;
  // vector_file = fopen(vector_src_filename, "rb");
  // printf("-------------------derp\n");
  // if (!vector_file) {
  //   printf("_ _ _ _ _ nope, %d\n", errno);
  //   exit(11);
  // }

  // printf("vector_codec->id %d\n", vector_codec->id);
  // parser = av_parser_init(vector_codec->id);
  // if (!parser) {
  //   printf("parser not found\n");
  //   exit(1);
  // }

  // int eret;
  // while (!feof(vector_file))
  // {
  //   printf("helllooo\n");
  //   data_size = fread(inbuf, 1, INBUF_SIZE, vector_file);
  //   if(!data_size)
  //   {
  //     printf("no data_size\n");
  //     break;
  //   }
  //   printf("data_size %d\n", data_size);

  //   // parser splits data into frames
  //   data = inbuf;
  //   // while (data_size > 0)
  //   // {
  //   //   printf("while....\n");
  //   //   eret = av_parser_parse2(parser,
  //   //                           vector_codec_context,
  //   //                           &vector_pkt->data,
  //   //                           &vector_pkt->size,
  //   //                           data,
  //   //                           data_size,
  //   //                           AV_NOPTS_VALUE,
  //   //                           AV_NOPTS_VALUE,
  //   //                           0);
  //   //   if (eret < 0) {
  //   //     fprintf(stderr, "Error while parsing\n");
  //   //     exit(1);
  //   //   }
  //   //   printf("while.... 2\n");

  //   //   data      += eret;
  //   //   data_size -= eret;

  //   //   if (vector_pkt->size)
  //   //   {
  //   //     printf("(+ + + + decccooooding _+ + + + +)\n");
  //   //     vp_decode(vector_codec_context, vector_frame, vector_pkt, vector_src_filename, output_file);
  //   //   }

  //   // }
  // }

  printf("\n");
  printf("__________\n");

  vp_encode(output_codec_ctx, NULL, output_pkt, output_file);
  printf("__________\n");

  uint8_t trailer[] = { 0, 0, 1, 0xb7 };
  printf("__________\n");
  fwrite(trailer, 1, sizeof(trailer), output_file);
  fclose(output_file);

  avcodec_free_context(&output_codec_ctx);
  av_frame_free(&output_frame);
  av_packet_free(&output_pkt);

  // av_packet_free(&vector_pkt); ///////

  // write_to_pkt.data = NULL;
  // write_to_pkt.size = 0;

  // // write file end
  // av_write_trailer(output_format_ctx);

  // // Free packets
  // av_free_packet(&write_to_pkt);
  // av_free_packet(&vector_pkt);

  // Free Frame
  av_free(vector_frame);

  // Close Codecs
  avcodec_close(vector_codec_context);
  avcodec_close(vector_codec_context_orig);

  // Close video file
  avformat_close_input(&vector_format_ctx);
  // avformat_close_input(&image_format_ctx);

  fclose(vector_file);

  return 0;
}
