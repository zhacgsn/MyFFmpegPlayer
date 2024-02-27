#include "VideoDecodeThread.h"

#include "FFmpegPlayer.h"
#include "SDL_mutex.h"
#include "SDL_timer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/packet.h"
#include "libavutil/avutil.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"
#include "libswscale/swscale.h"
}
#include <_types/_uint64_t.h>
#include <_types/_uint8_t.h>
#include <algorithm>
#include <iostream>

namespace myffmpegplayer {

// ? 同步 pts和视频时钟？
// 矫正 pts
static auto SynchronizeVideo(FFmpegPlayerContext *player_ctx,
                             AVFrame *src_frame, double pts) -> double {
  // std::cout << "In SynchronizeVideo" << '\n';

  if (pts != 0) {
    // 如果有 pts，将视频时钟设置为 pts
    player_ctx->video_clock_ = pts;
  } else {
    // 如果没有 pts，将 pts设置为视频时钟值
    pts = player_ctx->video_clock_;
  }
  double frame_delay{av_q2d(player_ctx->video_codec_ctx_->time_base)};
  // 如果要重复帧，相应调整时钟
  frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
  player_ctx->video_clock_ += frame_delay;

  return pts;
}

VideoDecodeThread::VideoDecodeThread(
    std::shared_ptr<FFmpegPlayerContext> player_ctx)
    : player_ctx_(player_ctx) {}

void VideoDecodeThread::Run() {
  VideoEntry();
  // std::cout << "VideoDecodeThread finished, ret=" << ret << '\n';
}

auto VideoDecodeThread::VideoEntry() -> int {
  // FFmpegPlayerContext *player_ctx{player_ctx_};
  AVPacket *packet{av_packet_alloc()};
  AVCodecContext *p_codec_ctx{player_ctx_->video_codec_ctx_};

  int ret{-1};
  double pts{0.0};

  AVFrame *p_frame{av_frame_alloc()};
  AVFrame *p_frame_rgb{av_frame_alloc()};

  // 给 AVFrame分配图像数据
  av_image_alloc(p_frame_rgb->data, p_frame_rgb->linesize, p_codec_ctx->width,
                 p_codec_ctx->height, AV_PIX_FMT_RGB24, 32);

  while (true) {
    if (stop_) {
      break;
    }
    if (player_ctx_->pause_ == PauseState::PAUSE_) {
      SDL_Delay(5);
      continue;
    }
    if (player_ctx_->flush_video_ctx_) {
      // std::cout << "av_codec_flush_buffers for seeking!" << '\n';
      avcodec_flush_buffers(player_ctx_->video_codec_ctx_);
      player_ctx_->flush_video_ctx_ = false;
      continue;
    }
    av_packet_unref(packet);

    if (player_ctx_->video_queue_.PacketGet(*packet, stop_) < 0) {
      break;
    }
    // 视频解码
    ret = avcodec_send_packet(p_codec_ctx, packet);

    if (ret == 0) {
      ret = avcodec_receive_frame(p_codec_ctx, p_frame);
    }
    // pts: presentation timestamp, 一个帧应该何时被展示
    // dts: decompression timestamp, 一个帧应该何时被解码
    // pts应该 >= dts

    // dts = AV_NOPTS_VALUE代表在文件中没有存储
    // 对 pts取值
    if (packet->dts == AV_NOPTS_VALUE && p_frame->opaque &&
        *static_cast<int64_t *>(p_frame->opaque) != AV_NOPTS_VALUE) {
      pts = static_cast<double>(*static_cast<uint64_t *>(p_frame->opaque));
    } else if (packet->dts != AV_NOPTS_VALUE) {
      pts = static_cast<double>(packet->dts);
    } else {
      pts = 0;
    }
    pts *= av_q2d(player_ctx_->video_stream_->time_base);

    // frame ready
    if (ret == 0) {
      ret = sws_scale(player_ctx_->sws_ctx_,
                      static_cast<uint8_t const *const *>(p_frame->data),
                      p_frame->linesize, 0, p_codec_ctx->height,
                      p_frame_rgb->data, p_frame_rgb->linesize);
      pts = SynchronizeVideo(player_ctx_.get(), p_frame, pts);

      if (ret == p_codec_ctx->height) {
        if (QueuePicture(p_frame_rgb, pts) < 0) {
          break;
        }
      }
    }
  }
  av_frame_free(&p_frame);
  av_frame_free(&p_frame_rgb);
  av_packet_free(&packet);

  return 0;
}

auto VideoDecodeThread::QueuePicture(AVFrame *p_frame, double pts) -> int {
  SDL_LockMutex(player_ctx_->pict_queue_mutex_);

  // 等待至有空间给新 picture
  while (player_ctx_->pict_queue_size_ >=
         static_cast<int>(kVideoPictureQueueSize)) {
    SDL_CondWaitTimeout(player_ctx_->pict_queue_cond_,
                        player_ctx_->pict_queue_mutex_, 500);

    if (stop_) {
      break;
    }
  }
  SDL_UnlockMutex(player_ctx_->pict_queue_mutex_);

  if (stop_) {
    return 0;
  }
  VideoPicture *video_picture{
      &player_ctx_->picture_queue_[player_ctx_->pict_queue_windex_]};
  if (!video_picture->frame_) {
    SDL_LockMutex(player_ctx_->pict_queue_mutex_);
    video_picture->frame_ = av_frame_alloc();
    // The allocated image buffer has to be freed
    // by using *av_freep(&pointers[0])
    av_image_alloc(video_picture->frame_->data, video_picture->frame_->linesize,
                   player_ctx_->video_codec_ctx_->width,
                   player_ctx_->video_codec_ctx_->height, AV_PIX_FMT_RGB24, 32);
    SDL_UnlockMutex(player_ctx_->pict_queue_mutex_);
  }
  std::copy_n(p_frame->data[0],
              player_ctx_->video_codec_ctx_->height * p_frame->linesize[0],
              video_picture->frame_->data[0]);
  video_picture->pts_ = pts;

  // 通知显示线程有 picture ready
  if (++player_ctx_->pict_queue_windex_ == kVideoPictureQueueSize) {
    player_ctx_->pict_queue_windex_ = 0;
  }
  SDL_LockMutex(player_ctx_->pict_queue_mutex_);
  player_ctx_->pict_queue_size_++;
  SDL_UnlockMutex(player_ctx_->pict_queue_mutex_);

  return 0;
}

} // namespace myffmpegplayer