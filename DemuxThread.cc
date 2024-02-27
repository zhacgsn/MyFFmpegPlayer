#include "DemuxThread.h"

#include "FFmpegPlayer.h"
#include "SDL_events.h"
#include "SDL_timer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/channel_layout.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/pixfmt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}
#include <iostream>
#include <sys/_types/_int64_t.h>

namespace myffmpegplayer {

DemuxThread::DemuxThread(std::shared_ptr<FFmpegPlayerContext> player_ctx)
    : player_ctx_(player_ctx) {}

auto DemuxThread::InitDemuxThread() -> int {
  AVFormatContext *fmt_ctx{nullptr};

  int ret = avformat_open_input(&fmt_ctx, player_ctx_->filename_.c_str(),
                                nullptr, nullptr);

  if (ret != 0) {
    std::cerr << "avformat_open_input failed!" << '\n';
    char err_str[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(ret, err_str, AV_ERROR_MAX_STRING_SIZE);
    std::cerr << "Error in avformat_open_input: " << err_str << '\n';
    return -1;
  }
  player_ctx_->format_ctx_ = fmt_ctx;

  if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
    std::cerr << "avformat_find_stream_info failed!" << '\n';
    return -1;
  }
  av_dump_format(fmt_ctx, 0, player_ctx_->filename_.c_str(), 0);

  if (StreamOpen(AVMEDIA_TYPE_AUDIO) < 0) {
    std::cerr << "Open audio stream failed!" << '\n';
    return -1;
  }
  if (StreamOpen(AVMEDIA_TYPE_VIDEO) < 0) {
    std::cerr << "Open video stream failed!" << '\n';
    return -1;
  }
  return 0;
}

void DemuxThread::FinitDemuxThread() {
  // std::cout << "In FinitDemuxThread" << '\n';

  if (player_ctx_->format_ctx_) {
    avformat_close_input(&(player_ctx_->format_ctx_));
    player_ctx_->format_ctx_ = nullptr;
  }
  if (player_ctx_->audio_codec_ctx_) {
    avcodec_free_context(&(player_ctx_->audio_codec_ctx_));
    player_ctx_->audio_codec_ctx_ = nullptr;
  }
  if (player_ctx_->video_codec_ctx_) {
    avcodec_free_context(&(player_ctx_->video_codec_ctx_));
    player_ctx_->video_codec_ctx_ = nullptr;
  }
  if (player_ctx_->swr_ctx_) {
    swr_free(&(player_ctx_->swr_ctx_));
    player_ctx_->swr_ctx_ = nullptr;
  }
  if (player_ctx_->sws_ctx_) {
    sws_freeContext(player_ctx_->sws_ctx_);
    player_ctx_->sws_ctx_ = nullptr;
  }
}

void DemuxThread::Run() { DecodeLoop(); }

auto DemuxThread::DecodeLoop() -> int {
  AVPacket *packet{av_packet_alloc()};

  // 持续读取帧数据
  while (true) {
    if (stop_) {
      // std::cout << "Request quit in DecodeLoop!" << '\n';
      break;
    }

    // 开始 seek
    // seek是跨线程操作，共享变量 seek_req_为 std::atomic<int>类型
    if (player_ctx_->seek_req_) {
      int stream_index{-1};
      int64_t seek_target{player_ctx_->seek_pos_};

      if (player_ctx_->video_stream_index_ >= 0) {
        stream_index = player_ctx_->video_stream_index_;
      } else if (player_ctx_->audio_stream_index_ >= 0) {
        stream_index = player_ctx_->audio_stream_index_;
      }

      if (stream_index >= 0) {
        // 转换时间基准
        seek_target = av_rescale_q(
            seek_target, {1, AV_TIME_BASE},
            player_ctx_->format_ctx_->streams[stream_index]->time_base);
      }

      if (av_seek_frame(player_ctx_->format_ctx_, stream_index, seek_target,
                        player_ctx_->seek_flags_) < 0) {
        std::cerr << player_ctx_->filename_ << ": error while seeking!" << '\n';
      } else {
        // seek后，原视频队列中的 packet就不再使用，进行清除
        if (player_ctx_->audio_stream_index_ >= 0) {
          player_ctx_->audio_queue_.PacketFlush();
          // AudioDecodeThread据此重置解码器状态 (avcodec_flush_buffers)
          player_ctx_->flush_audio_ctx_ = true;
        }
        if (player_ctx_->video_stream_index_ >= 0) {
          player_ctx_->video_queue_.PacketFlush();
          // VideoDecodeThread据此重置解码器状态 (avcodec_flush_buffers)
          player_ctx_->flush_video_ctx_ = true;
        }
      }
      // reset to zero after seeking is done
      player_ctx_->seek_req_ = 0;
    }

    // 队列过满时，延时，然后继续读数据
    if (player_ctx_->audio_queue_.PacketSize() >
            static_cast<int>(kMaxAudioQSize) ||
        player_ctx_->video_queue_.PacketSize() >
            static_cast<int>(kMaxVideoQSize)) {
      SDL_Delay(10);
      continue;
    }
    // 读取未解码数据，放在 packet中
    if (av_read_frame(player_ctx_->format_ctx_, packet) < 0) {
      std::cerr << "av_read_frame error!" << '\n';
      break;
    }

    // 判断 packet是视频帧还是音频帧，放入对应 PacketQueue队列
    if (packet->stream_index == player_ctx_->video_stream_index_) {
      player_ctx_->video_queue_.PacketPut(*packet);
    } else if (packet->stream_index == player_ctx_->audio_stream_index_) {
      player_ctx_->audio_queue_.PacketPut(*packet);
    } else {
      // TODO: 除了音视频还可能有其他 packet如字幕...
      av_packet_unref(packet);
    }
  }

  while (!stop_) {
    SDL_Delay(100);
  }
  av_packet_free(&packet);

  SDL_Event event;
  event.type = kFFQuitEvent;
  event.user.data1 = player_ctx_.get();
  SDL_PushEvent(&event);

  return 0;
}

auto DemuxThread::StreamOpen(int media_type) -> int {
  AVFormatContext *fmt_ctx = player_ctx_->format_ctx_;
  AVCodec *codec{nullptr};

  // 找到 best stream并且返回对应 decoder
  int stream_index{
      av_find_best_stream(fmt_ctx, static_cast<AVMediaType>(media_type), -1, -1,
                          const_cast<AVCodec const **>(&codec), 0)};

  if (stream_index < 0 ||
      stream_index >= static_cast<int>(fmt_ctx->nb_streams)) {
    // std::cout << "Cannot find an audio stream in the input file" << '\n';
    return -1;
  }

  // 根据解码器创建解码器上下文
  AVCodecContext *codec_ctx{avcodec_alloc_context3(codec)};
  // 根据format context填充解码器上下文参数
  avcodec_parameters_to_context(codec_ctx,
                                fmt_ctx->streams[stream_index]->codecpar);

  // 打开解码器
  if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
    std::cerr << "Failed to open codec for stream #" << stream_index << '\n';
    return -1;
  }

  switch (codec_ctx->codec_type) {
  case AVMEDIA_TYPE_AUDIO: {
    player_ctx_->audio_stream_index_ = stream_index;
    player_ctx_->audio_codec_ctx_ = codec_ctx;
    player_ctx_->audio_stream_ = fmt_ctx->streams[stream_index];
    // 通过 swr context处理音频重采样
    player_ctx_->swr_ctx_ = swr_alloc();

    av_opt_set_chlayout(player_ctx_->swr_ctx_, "in_chlayout",
                        &(codec_ctx->ch_layout), 0);
    av_opt_set_int(player_ctx_->swr_ctx_, "in_sample_rate",
                   codec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(player_ctx_->swr_ctx_, "in_sample_fmt",
                          codec_ctx->sample_fmt, 0);

    AVChannelLayout out_layout;
    // use stereo
    av_channel_layout_default(&out_layout, 2);

    av_opt_set_chlayout(player_ctx_->swr_ctx_, "out_chlayout", &out_layout, 0);
    av_opt_set_int(player_ctx_->swr_ctx_, "out_sample_rate", 48000, 0);
    av_opt_set_sample_fmt(player_ctx_->swr_ctx_, "out_sample_fmt",
                          AV_SAMPLE_FMT_S16, 0);
    swr_init(player_ctx_->swr_ctx_);
  } break;
  case AVMEDIA_TYPE_VIDEO: {
    player_ctx_->video_stream_index_ = stream_index;
    player_ctx_->video_codec_ctx_ = codec_ctx;
    player_ctx_->video_stream_ = fmt_ctx->streams[stream_index];
    player_ctx_->frame_timer_ = static_cast<double>(av_gettime()) / 1000000.0;
    player_ctx_->frame_last_delay_ = 40e-3;
    // 通过 sws context处理视频图像缩放和颜色空间转换
    player_ctx_->sws_ctx_ =
        sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                       codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
                       SWS_BILINEAR, nullptr, nullptr, nullptr);
  } break;
  default:
    break;
  }
  return 0;
}

} // namespace myffmpegplayer