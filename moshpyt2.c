// 11-17-2018
// vipyne
// http://dranger.com/ffmpeg/tutorial01.html

#include <stdio.h>
// #include <iostream>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
// #include <ffmpeg/swscale.h> /////////

static const char *vector_src_filename = NULL;
static const char *image_src_filename = NULL;
static const char *output_filename = NULL;

int vp_error(char *message)
{
  printf("%s\n ", message);
  return -1;
}

int main(int argc, char *argv[])
{
  vector_src_filename = argv[1];
  image_src_filename = argv[2];
  output_filename = argv[3];

  // register all codecs
  av_register_all();

  //////////////////////////////////// Motion vector src

  AVFormatContext *vector_format_ctx = NULL;

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
  AVCodecContext *vector_codec_context = NULL;
  AVCodecContext *vector_codec_context_orig = NULL;

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
  AVCodec *vector_codec = NULL;
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

  //////////////////////////////////// Image src

    AVFormatContext *image_format_ctx = NULL;

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
  AVCodecContext *image_codec_context = NULL;
  AVCodecContext *image_codec_context_orig = NULL;

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
  AVCodec *image_codec = NULL;
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

  ////////////////////////////////////

  ////////////// OUTPUT //////////////
  // AVFrame *vector_frame = NULL;
  // vector_frame = av_frame_alloc();
  AVFormatContext *output_format_ctx = NULL;
  avformat_alloc_output_context2(&output_format_ctx, NULL, NULL, output_filename);
  AVStream *in_stream = vector_format_ctx->streams[0];
  AVStream *out_stream = avformat_new_stream(output_format_ctx, in_stream->codec->codec);
  avcodec_copy_context(out_stream->codec, in_stream->codec);
  avio_open(&output_format_ctx->pb, output_filename, AVIO_FLAG_WRITE);

  // write file header
  int header_written = avformat_write_header(output_format_ctx, NULL);
  if (0 > header_written)
  {
    vp_error("header wasn't written for output file.\n");
  }

  AVPacket vector_pkt;
  AVPacket image_pkt;
  AVPacket write_to_pkt;
  av_init_packet(&write_to_pkt);
  av_init_packet(&vector_pkt);
  write_to_pkt.data = NULL;
  write_to_pkt.size = 0;
  int vector_ret = 0;
  int image_ret = 0;

  while (1)
  {
    vector_ret = av_read_frame(vector_format_ctx, &vector_pkt);
    image_ret = av_read_frame(image_format_ctx, &image_pkt);
    // if (0 > vector_ret)
    if (0 > image_ret)
      break;

    /////////// under construction ///////////
    // av_copy_packet_side_data(&vector_pkt, &image_pkt);
    size_t some_size = 26355;
    // uint8_t *vector_side_data = av_malloc();
    uint8_t *vector_side_data = av_packet_get_side_data(&vector_pkt, AV_FRAME_DATA_MOTION_VECTORS, NULL);
    if (NULL == vector_side_data)
    {
      printf("nulllllllll\n");
    }
    av_packet_add_side_data(&image_pkt, AV_FRAME_DATA_MOTION_VECTORS, &vector_side_data, some_size);
    // av_interleaved_write_frame(output_format_ctx, &image_pkt);
    /////////// under construction ///////////

    av_interleaved_write_frame(output_format_ctx, &vector_pkt);
  }
  printf("\n");

  write_to_pkt.data = NULL;
  write_to_pkt.size = 0;

  // write file end
  av_write_trailer(output_format_ctx);

  // Free packets
  av_free_packet(&write_to_pkt);
  av_free_packet(&vector_pkt);

  // // Free Frame
  // av_free(vector_frame);

  // Close Codecs
  avcodec_close(vector_codec_context);
  avcodec_close(vector_codec_context_orig);

  // Close video file
  avformat_close_input(&vector_format_ctx);
}
