// 11-17-2018
// vipyne
// http://dranger.com/ffmpeg/tutorial01.html

#include <stdio.h>
// #include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/motion_vector.h>

static const char *vector_src_filename = NULL;
static const char *image_src_filename = NULL;
static const char *output_filename = NULL;

int vp_error(char *message)
{
  printf("%s\n ", message);
  return -1;
}

static void vp_encode(AVCodecContext *codec_ctx, AVFrame *fframe, AVPacket *pkt, FILE *outfile)
{
  int ret;

  AVFrame *frame = av_frame_alloc();

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
      printf("EAGAIN\n");
      return;
    }
    else if (AVERROR_EOF == ret)
    {
      printf("AVERROR_EOF\n");
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

int main(int argc, char *argv[])
{
  vector_src_filename = argv[1];
  image_src_filename = argv[2];
  output_filename = argv[3];

  // register all codecs
  av_register_all();

  ////////////////////////////////////   Motion vector src
  AVFormatContext *vector_format_ctx = NULL;
  AVCodecContext *vector_codec_context = NULL;
  AVCodecContext *vector_codec_context_orig = NULL;
  AVCodec *vector_codec = NULL;
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

  ////////////////////////////////////   Image src
  AVFormatContext *image_format_ctx = NULL;
  AVCodecContext *image_codec_context = NULL;
  AVCodecContext *image_codec_context_orig = NULL;
  AVCodec *image_codec = NULL;
  {
    // Open file
    if (0 != avformat_open_input(&image_format_ctx, image_src_filename, NULL, 0))
    {
      printf("Could not open file: %s", image_src_filename);
      return -1;
    }

    // Get stream info
    if (0 > avformat_find_stream_info(image_format_ctx, NULL))
    {
      printf("Could not get stream info: %d", avformat_find_stream_info(image_format_ctx, NULL));
      return -1;
    }

    av_dump_format(image_format_ctx, 0, image_src_filename, 0);

    // Get video stream
    // {
    //   int i;
    //   int videoStream = -1;
    //   for (i = 0; i < image_format_ctx->nb_streams; i++)
    //   {
    //     printf("streams %d\n", i);
    //     printf("stream- %d\n", image_format_ctx->streams[i]->codecpar->codec_id);
    //     if (AVMEDIA_TYPE_VIDEO == image_format_ctx->streams[i]->codecpar->codec_id)
    //     {
    //       videoStream = i;
    //       printf("sup.    %d\n", i);
    //       break;
    //     }
    //   }
    //   if (-1 == videoStream)
    //   {
    //     vp_error("couldn't find video stream.");
    //   }
    //   printf("can't find videoStream but I just know it is 0, ignore %d\n", videoStream);
    // }

    // Get Codec
    image_codec = avcodec_find_decoder(image_format_ctx->streams[0]->codecpar->codec_id);
    if (NULL == image_codec)
    {
      vp_error("codec is wrong.");
    }

    // Create Codec Context
    image_codec_context = avcodec_alloc_context3(image_codec);
    if ( 0 > avcodec_parameters_from_context(image_format_ctx->streams[0]->codecpar, image_codec_context))
    {
      vp_error("couldn't copy codec context.");
    }
    // Open Codec Context
    if (0 > avcodec_open2(image_codec_context, image_codec, NULL))
    {
      vp_error("avcodec_open2 barfed");
    }
  }

  ////////////////////////////////////   OUTPUT
  AVFrame *vector_frame = NULL;
  vector_frame = av_frame_alloc();

  FILE *output_file;
  AVPacket *output_pkt;
  AVFrame *output_frame = av_frame_alloc();
  AVFormatContext *output_format_ctx = NULL;
  AVCodecContext *output_codec_ctx = NULL;
  avformat_alloc_output_context2(&output_format_ctx, NULL, NULL, output_filename);
  AVStream *in_stream = vector_format_ctx->streams[0];
  AVStream *out_stream = avformat_new_stream(output_format_ctx, in_stream->codec->codec);
  avcodec_copy_context(out_stream->codec, in_stream->codec);
  avio_open(&output_format_ctx->pb, output_filename, AVIO_FLAG_WRITE);

  av_dump_format(output_format_ctx, 0, output_filename, 1);

  output_file = fopen(output_filename, "wb");
  if (!output_file)
  {
    printf("Coult not open %s.\n", output_filename);
  }

  AVCodec *output_codec;
  output_codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
  output_codec_ctx = avcodec_alloc_context3(output_codec);
  output_pkt = av_packet_alloc();

  // // write file header
  // int header_written = avformat_write_header(output_format_ctx, NULL);
  // if (0 > header_written)
  // {
  //   vp_error("header wasn't written for output file.\n");
  // }

  int fb_ret, i, x, y;

  output_codec_ctx->bit_rate = 400000;
  output_codec_ctx->width = 1920;
  output_codec_ctx->height = 1080;
  output_codec_ctx->time_base = (AVRational){1, 25};
  output_codec_ctx->framerate = (AVRational){25, 1};
  output_codec_ctx->gop_size = 10;
  output_codec_ctx->max_b_frames = 1;
  output_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  output_frame->format = output_codec_ctx->pix_fmt;
  output_frame->width = output_codec_ctx->width;
  output_frame->height = output_codec_ctx->height;

  fb_ret = av_frame_get_buffer(output_frame, 32);
  if (0 > fb_ret)
  {
    printf("could not alloc frame data.\n");
  }
  printf("narrrrrfffff____________________________________\n");

  for (i = 0; i < 25; i++)
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

  // while (1)
  // {
  //   int decoded;

  //   do {
  //     int ret = 0;
  //     int got_frame = 0;

  //     if (0 == vector_pkt.stream_index)
  //     {
  //       printf("herer\n");
  //       decoded = vector_pkt.size;
  //       printf("vector_pkt.size %d \n", vector_pkt.size);

  //       ret = avcodec_send_packet(vector_format_ctx, &vector_pkt);
  //       printf("ret - %d\n", ret);
  //       if (0 > ret && AVERROR_EOF != 0)
  //         ret = avcodec_send_packet(vector_format_ctx, &vector_pkt);
  //       ret = avcodec_receive_frame(vector_format_ctx, vector_frame);

  //       /////////// under construction ///////////
  //       // av_copy_packet_side_data(&vector_pkt, &image_pkt);
  //       // uint8_t *vector_side_data = av_malloc();
  //       if (got_frame)
  //       {
  //         printf("(got frame)\n");
  //         AVFrameSideData *vector_side_data = av_packet_get_side_data(&vector_frame, AV_FRAME_DATA_MOTION_VECTORS, NULL);

  //         if (vector_side_data)
  //         {
  //           printf("(got side data!!!)\n");
  //           printf("side data size %d\n", vector_side_data->size);
  //           AVMotionVector *mvs = (AVMotionVector *)vector_side_data->data;
  //         }
  //       }
  //       // if (NULL != vector_side_data)
  //         // printf("________nulllllllll\n");
  //       // size_t some_size = 26355;
  //       // av_packet_add_side_data(&image_pkt, AV_FRAME_DATA_MOTION_VECTORS, &vector_side_data, some_size);
  //       // av_interleaved_write_frame(output_format_ctx, &image_pkt);
  //       /////////// under construction ///////////
  //       av_frame_unref(vector_frame);

  //       av_interleaved_write_frame(output_format_ctx, &vector_pkt);

  //       if (0 > decoded)
  //         break;

  //       printf("decoded %d \n", decoded);
  //       vector_pkt.data += decoded;
  //       vector_pkt.size -= decoded;
  //     }
  //   } while ((0 > vector_pkt.size) && (0 != decoded));
  // }
  printf("\n");
  printf("__________\n");

  vp_encode(output_codec_ctx, NULL, output_pkt, output_file);

  uint8_t trailer[] = { 0, 0, 1, 0xb7 };
  fwrite(trailer, 1, sizeof(trailer), output_file);

  // write_to_pkt.data = NULL;
  // write_to_pkt.size = 0;

  // write file end
  av_write_trailer(output_format_ctx);

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

  return 0;
}
