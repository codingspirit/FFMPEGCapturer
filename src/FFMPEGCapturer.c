/**
 * @file FFMPEGCapturer.c
 * @author Alex Li (lizhiqin46783937@live.com)
 * @brief
 * @version 0.1
 * @date 2022-05-15
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "FFMPEGCapturer.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#ifdef ENABLE_SCALE
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOG(msg, ...) printf("[FFMPEGCapturer] " msg "\n", ##__VA_ARGS__)
#define SAFE_FREE(x) \
  do {               \
    if (x != NULL) { \
      free(x);       \
      x = NULL;      \
    }                \
  } while (0)

#define DEFAULT_VIDEO_WIDTH 1920
#define DEFAULT_VIDEO_HEIGHT 1080
#define DEFAULT_VIDEO_FRAME_RATE 30
#define DEFAULT_VIDEO_TARGET_BITRATE 8000000UL
#define DEFAULT_VIDEO_INPUT_PIXEL_FORMAT AV_PIX_FMT_UYVY422
#define DEFAULT_VIDEO_INPUT_PIXEL_FORMAT_STR "uyvy422"
#ifdef ENABLE_SCALE
#define DEFAULT_VIDEO_OUTPUT_PIXEL_FORMAT AV_PIX_FMT_YUV420P
#else
#define DEFAULT_VIDEO_OUTPUT_PIXEL_FORMAT AV_PIX_FMT_UYVY422
#endif
#define DEFAULT_VIDEO_ENCODER_NAME "h264_v4l2m2m"

typedef struct {
  AVFormatContext* pFormatContext;
  AVCodecContext* pDecoderContext;
  AVCodecContext* pEncoderContext;
#ifdef ENABLE_SCALE
  struct SwsContext* pSwsContext;
#endif
  size_t pts;
} FFMPEGCapturer;

#ifdef ENABLE_SCALE
static int ffmpegCapturerScale(struct SwsContext* pSwsContext,
                               AVFrame* pInFrame, AVFrame* pOutFrame) {
  int ret = 0;

  sws_scale(pSwsContext, pInFrame->data, pInFrame->linesize, 0,
            pInFrame->height, pOutFrame->data, pOutFrame->linesize);

  pOutFrame->pts = pInFrame->pts;

  return ret;
}

static int ffmpegCapturerOpenScaler(FFMPEGCapturer* pCapturer) {
  pCapturer->pSwsContext =
      sws_getContext(DEFAULT_VIDEO_WIDTH, DEFAULT_VIDEO_HEIGHT,
                     DEFAULT_VIDEO_INPUT_PIXEL_FORMAT, DEFAULT_VIDEO_WIDTH,
                     DEFAULT_VIDEO_HEIGHT, DEFAULT_VIDEO_OUTPUT_PIXEL_FORMAT,
                     SWS_BICUBIC, NULL, NULL, NULL);

  return 0;
}
#endif

static int ffmpegCapturerOpenDecoder(FFMPEGCapturer* pCapturer) {
  if (pCapturer == NULL) {
    return -EINVAL;
  }

  int ret = 0;
  AVCodec* pDecoder = NULL;
  AVCodecParameters* pDecoderParameter = NULL;
  int videoStreamIndex = -1;

  videoStreamIndex = av_find_best_stream(
      pCapturer->pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &pDecoder, 0);
  if (videoStreamIndex < 0 || pDecoder == NULL) {
    LOG("Failed to find video stream");
    return -EINVAL;
  }

  pDecoderParameter =
      pCapturer->pFormatContext->streams[videoStreamIndex]->codecpar;
  pCapturer->pDecoderContext = avcodec_alloc_context3(pDecoder);
  if (pCapturer->pDecoderContext == NULL) {
    LOG("OOM");
    return -ENOMEM;
  }

  ret = avcodec_parameters_to_context(pCapturer->pDecoderContext,
                                      pDecoderParameter);
  if (ret != 0) {
    LOG("Failed to init decoder");
    return ret;
  }

  ret = avcodec_open2(pCapturer->pDecoderContext, pDecoder, NULL);
  if (ret != 0) {
    LOG("Failed to open decoder");
  } else {
    LOG("Opened decoder %s, %dx%d", pDecoder->long_name,
        pCapturer->pDecoderContext->width, pCapturer->pDecoderContext->height);
  }

  return ret;
}

static int ffmpegCapturerOpenEncoder(FFMPEGCapturer* pCapturer) {
  if (pCapturer == NULL) {
    return -EINVAL;
  }

  int ret = 0;
  AVCodec* pEncoder = NULL;

  pEncoder = avcodec_find_encoder_by_name(DEFAULT_VIDEO_ENCODER_NAME);
  if (pEncoder == NULL) {
    LOG("Failed to find encoder");
    return -ENODEV;
  }

  pCapturer->pEncoderContext = avcodec_alloc_context3(pEncoder);
  if (pCapturer->pEncoderContext == NULL) {
    LOG("OOM");
    return -ENOMEM;
  }

  pCapturer->pEncoderContext->profile = FF_PROFILE_H264_BASELINE;
  pCapturer->pEncoderContext->level = 42;
  pCapturer->pEncoderContext->width = DEFAULT_VIDEO_WIDTH;
  pCapturer->pEncoderContext->height = DEFAULT_VIDEO_HEIGHT;
  pCapturer->pEncoderContext->pix_fmt = DEFAULT_VIDEO_OUTPUT_PIXEL_FORMAT;
  pCapturer->pEncoderContext->bit_rate = DEFAULT_VIDEO_TARGET_BITRATE;
  pCapturer->pEncoderContext->time_base =
      (AVRational){1, DEFAULT_VIDEO_FRAME_RATE};
  pCapturer->pEncoderContext->framerate =
      (AVRational){DEFAULT_VIDEO_FRAME_RATE, 1};
  // pCapturer->pEncoderContext->refs = 3;
  // pCapturer->pEncoderContext->gop_size = 250;
  // pCapturer->pEncoderContext->keyint_min = 30;

  AVDictionary* pOptions = NULL;
  av_dict_set(&pOptions, "tune", "zerolatency", 0);
  av_dict_set(&pOptions, "preset", "veryfast", 0);

  ret = avcodec_open2(pCapturer->pEncoderContext, pEncoder, &pOptions);

  if (ret != 0) {
    LOG("Failed to open encoder");
  } else {
    LOG("Opened encoder %s", pEncoder->long_name);
  }

  return ret;
}

static int ffmpegCapturerDecode(AVCodecContext* pDecoderContext,
                                AVPacket* pEncodedPacket, AVFrame* pRawFrame) {
  if (pDecoderContext == NULL || pEncodedPacket == NULL || pRawFrame == NULL) {
    return -EINVAL;
  }

  int ret = 0;

  ret = avcodec_send_packet(pDecoderContext, pEncodedPacket);
  if (ret != 0) {
    LOG("Failed to send packet to decoder");
    return ret;
  }

  ret = avcodec_receive_frame(pDecoderContext, pRawFrame);
  if (ret != 0) {
    LOG("Failed to recevice frame from decoder");
  } else {
    pRawFrame->pict_type = AV_PICTURE_TYPE_NONE;
  }

  return ret;
}

FFMPEGCapturerHandle ffmpegCapturerOpen(const char* pDeviceName) {
  avdevice_register_all();
  av_log_set_level(AV_LOG_DEBUG);

  AVInputFormat* iFormat = av_find_input_format("video4linux2");

  if (iFormat == NULL) {
    LOG("Failed to find v4l2 device");
    return NULL;
  }

  FFMPEGCapturer* pCapturer = NULL;
  pCapturer = (FFMPEGCapturer*)malloc(sizeof(FFMPEGCapturer));
  memset(pCapturer, 0, sizeof(FFMPEGCapturer));

  if (pCapturer == NULL) {
    LOG("OOM");
    return (FFMPEGCapturerHandle)pCapturer;
  }

  AVDictionary* options = NULL;
  av_dict_set(&options, "video_size",
              STR(DEFAULT_VIDEO_WIDTH) "x" STR(DEFAULT_VIDEO_HEIGHT), 0);
  av_dict_set(&options, "framerate", STR(DEFAULT_VIDEO_FRAME_RATE), 0);
  av_dict_set(&options, "pixel_format", DEFAULT_VIDEO_INPUT_PIXEL_FORMAT_STR,
              0);

  int ret = 0;

  // open device
  ret = avformat_open_input(&pCapturer->pFormatContext, pDeviceName, iFormat,
                            &options);
  if (ret != 0) {
    LOG("Failed to open device %s, %d(%s)", pDeviceName, ret, av_err2str(ret));
    ffmpegGCapturerClose((FFMPEGCapturerHandle*)&pCapturer);
    return (FFMPEGCapturerHandle)pCapturer;
  } else {
    LOG("Opened device %s", pDeviceName);
  }

  // open decoder
  ret = ffmpegCapturerOpenDecoder(pCapturer);
  if (ret != 0) {
    ffmpegGCapturerClose((FFMPEGCapturerHandle*)&pCapturer);
    return (FFMPEGCapturerHandle)pCapturer;
  }

  // open encoder
  ret = ffmpegCapturerOpenEncoder(pCapturer);
  if (ret != 0) {
    ffmpegGCapturerClose((FFMPEGCapturerHandle*)&pCapturer);
    return (FFMPEGCapturerHandle)pCapturer;
  }

#ifdef ENABLE_SCALE
  // open scaler
  ret = ffmpegCapturerOpenScaler(pCapturer);
  if (ret != 0) {
    ffmpegGCapturerClose((FFMPEGCapturerHandle*)&pCapturer);
    return (FFMPEGCapturerHandle)pCapturer;
  }
#endif

  return (FFMPEGCapturerHandle)pCapturer;
}

void ffmpegGCapturerClose(FFMPEGCapturerHandle* ppHandle) {
  if (ppHandle != NULL && *ppHandle != NULL) {
    FFMPEGCapturer* pCapturer = (FFMPEGCapturer*)*ppHandle;
    if (pCapturer->pFormatContext != NULL) {
      avformat_close_input(&pCapturer->pFormatContext);
    }
    if (pCapturer->pDecoderContext != NULL) {
      avcodec_free_context(&pCapturer->pDecoderContext);
    }
    if (pCapturer->pEncoderContext != NULL) {
      avcodec_free_context(&pCapturer->pEncoderContext);
    }
    free(*ppHandle);
    *ppHandle = NULL;
  }
}
#ifdef ENABLE_SCALE
static AVFrame* pYuvFrame = NULL;
static uint8_t* pYuvFrameBuffer = NULL;
#endif
int ffmpegCapturerSyncGetFrame(FFMPEGCapturerHandle pHandle,
                               void* pFrameDataBuffer,
                               const size_t frameDataBufferSize,
                               size_t* pFrameSize) {
  int ret = 0;
  if (pHandle == NULL || pFrameDataBuffer == NULL || pFrameSize == NULL) {
    return -EINVAL;
  }

  FFMPEGCapturer* pCapturer = (FFMPEGCapturer*)pHandle;
  if (pCapturer->pFormatContext == NULL || pCapturer->pDecoderContext == NULL ||
      pCapturer->pEncoderContext == NULL) {
    return -EINVAL;
  }

  AVPacket* pInPacket = av_packet_alloc();
  AVFrame* pRawFrame = av_frame_alloc();
  AVPacket* pOutPacket = av_packet_alloc();

#ifdef ENABLE_SCALE
  if (pYuvFrame == NULL) {
    pYuvFrame = av_frame_alloc();
    int bufferSize = av_image_get_buffer_size(
        AV_PIX_FMT_YUV420P, DEFAULT_VIDEO_WIDTH, DEFAULT_VIDEO_HEIGHT, 1);
    pYuvFrameBuffer = av_malloc(bufferSize);
    av_image_fill_arrays(pYuvFrame->data, pYuvFrame->linesize, pYuvFrameBuffer,
                         AV_PIX_FMT_YUV420P, DEFAULT_VIDEO_WIDTH,
                         DEFAULT_VIDEO_HEIGHT, 1);
    pYuvFrame->format = AV_PIX_FMT_YUV420P;
    pYuvFrame->height = DEFAULT_VIDEO_HEIGHT;
    pYuvFrame->width = DEFAULT_VIDEO_WIDTH;
  }
#endif

  if (pInPacket == NULL || pRawFrame == NULL || pOutPacket == NULL) {
    return -ENOMEM;
  }

  if ((ret = av_read_frame(pCapturer->pFormatContext, pInPacket)) != 0) {
    LOG("Failed to read frame: %d, %s", ret, av_err2str(ret));
  } else if ((ret = ffmpegCapturerDecode(pCapturer->pDecoderContext, pInPacket,
                                         pRawFrame)) != 0) {
    LOG("Failed to decode frame: %d, %s", ret, av_err2str(ret));
  }
#ifdef ENABLE_SCALE
  else if ((ret = ffmpegCapturerScale(pCapturer->pSwsContext, pRawFrame,
                                      pYuvFrame)) != 0) {
    LOG("Failed to scale: %d, %s", ret, av_err2str(ret));
  } else if ((ret = ffmpegCapturerEncode(pCapturer->pEncoderContext, pYuvFrame,
                                         pOutPacket, pFrameDataBuffer,
                                         pFrameSize)) != 0) {
    LOG("Failed to encode frame: %d, %s", ret, av_err2str(ret));
  }
#endif
  else if ((ret = avcodec_send_frame(pCapturer->pEncoderContext, pRawFrame)) !=
           0) {
    LOG("Failed to send frame to encoder");
  } else {
    // Try to read frame back
    size_t index = 0;
    while (ret == 0) {
      ret = avcodec_receive_packet(pCapturer->pEncoderContext, pOutPacket);
      if (ret != 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR(EOF)) {
          ret = index > 0 ? 0 : -EAGAIN;
        }
        break;
      }

      memcpy(pFrameDataBuffer + index, pOutPacket->data, pOutPacket->size);
      index += pOutPacket->size;
      *pFrameSize = index;
      av_packet_unref(pOutPacket);
    }
  }

  av_packet_unref(pInPacket);
  av_packet_free(&pInPacket);

  av_frame_unref(pRawFrame);
  av_frame_free(&pRawFrame);

  av_packet_free(&pOutPacket);

  return ret;
}
