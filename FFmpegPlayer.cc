#include "FFmpegPlayer.h"
#include "AudioDecodeThread.h"
#include "AudioPlay.h"
#include "DemuxThread.h"
#include "SDLApp.h"
#include "SDL_audio.h"
#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_mutex.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "VideoDecodeThread.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include <_types/_uint32_t.h>
#include <_types/_uint8_t.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <sys/_types/_int64_t.h>
#include <sys/_types/_size_t.h>

namespace myffmpegplayer {

// 返回音频当前播放时间 (pts, presentation timestamp)
static auto GetAudioClock(FFmpegPlayerContext *player_ctx) -> double {
  double pts{player_ctx->audio_clock_};
  size_t hw_buf_size{player_ctx->audio_buf_size_ -
                     player_ctx->audio_buf_index_};
  int bytes_per_sec{0};
  // 每个音频样本的字节数，假设 每个通道为 16bit即 2byte
  int n{player_ctx->audio_codec_ctx_->ch_layout.nb_channels * 2};

  if (player_ctx->audio_stream_) {
    bytes_per_sec = player_ctx->audio_codec_ctx_->sample_rate * n;
  }

  if (bytes_per_sec) {
    // 减去缓冲区中剩余数据传输所需时间
    pts -= static_cast<double>(hw_buf_size) / bytes_per_sec;
  }
  return pts;
}

static auto SDLRefreshTimerCallback(uint32_t /*interval*/, void *param)
    -> uint32_t {
  SDL_Event event;
  event.type = kFFRefreshEvent;
  event.user.data1 = param;
  SDL_PushEvent(&event);

  // If the callback returns 0, the periodic alarm is cancelled
  // 只触发一次
  return 0;
}

static void ScheduleRefresh(FFmpegPlayerContext *player_ctx, int delay) {
  // 未来调用 SDLRefreshTimerCallback
  SDL_AddTimer(delay, SDLRefreshTimerCallback, player_ctx);
}

static void VideoDisplay(FFmpegPlayerContext *player_ctx) {
  VideoPicture *video_picture{
      &player_ctx->picture_queue_[player_ctx->pict_queue_rindex_]};

  if (video_picture->frame_ && player_ctx->image_callback_) {
    player_ctx->image_callback_(
        video_picture->frame_->data[0], player_ctx->video_codec_ctx_->width,
        player_ctx->video_codec_ctx_->height, player_ctx->cb_data_);
  }
}

static void FNAudioCallback(void *userdata, uint8_t *stream, int len) {
  AudioDecodeThread *audio_decode_thread{
      static_cast<AudioDecodeThread *>(userdata)};
  audio_decode_thread->GetAudioData(stream, len);
}

void StreamSeek(FFmpegPlayerContext *player_ctx, int64_t pos, int rel) {
  if (!player_ctx->seek_req_) {
    player_ctx->seek_pos_ = pos;
    player_ctx->seek_flags_ = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
    // atomic
    player_ctx->seek_req_ = 1;
  }
}

FFmpegPlayerContext::FFmpegPlayerContext()
    : audio_frame_(av_frame_alloc()), audio_pkt_(av_packet_alloc()),
      pict_queue_mutex_(SDL_CreateMutex()), pict_queue_cond_(SDL_CreateCond()) {
}

FFmpegPlayerContext::~FFmpegPlayerContext() { CleanUp(); }

void FFmpegPlayerContext::Finit() { CleanUp(); }

void FFmpegPlayerContext::CleanUp() {
  if (!stopped_) {
    if (audio_frame_) {
      av_frame_free(&audio_frame_);
    }
    if (audio_pkt_) {
      av_packet_free(&audio_pkt_);
    }
    if (pict_queue_mutex_) {
      SDL_DestroyMutex(pict_queue_mutex_);
    }
    if (pict_queue_cond_) {
      SDL_DestroyCond(pict_queue_cond_);
    }
    stopped_ = true;
  }
}

FFmpegPlayer::FFmpegPlayer()
    : player_ctx_(std::make_shared<FFmpegPlayerContext>()),
      demux_thread_(player_ctx_), audio_decode_thread_(player_ctx_),
      video_decode_thread_(player_ctx_) {}

void FFmpegPlayer::SetFilePath(const char *file_path) {
  file_path_ = file_path;
}

void FFmpegPlayer::SetImageCallback(ImageCallback image_callback,
                                    void *user_data) {
  player_ctx_->image_callback_ = image_callback;
  // RenderPairData
  player_ctx_->cb_data_ = user_data;
}

auto FFmpegPlayer::InitPlayer() -> int {
  player_ctx_->filename_ = file_path_;

  if (demux_thread_.InitDemuxThread() != 0) {
    std::cerr << "DemuxThread init failed!" << '\n';
    return -1;
  }

  // render audio params
  audio_desired_spec_.freq = 48000;
  audio_desired_spec_.format = AUDIO_S16SYS;
  audio_desired_spec_.channels = 2;
  audio_desired_spec_.silence = 0;
  audio_desired_spec_.samples = kSDLAudioBufferSize;
  // callback that feeds the audio device
  audio_desired_spec_.callback = FNAudioCallback;
  audio_desired_spec_.userdata = &audio_decode_thread_;

  // 打开音频播放设备
  if (audio_play_.OpenDevice(&audio_desired_spec_) <= 0) {
    std::cerr << "Could not open audio device!" << '\n';
    return -1;
  }

  auto refresh_event = [this](SDL_Event *event) -> void {
    OnRefreshEvent(event);
  };

  auto key_event = [this](SDL_Event *event) -> void { OnKeyEvent(event); };

  SDLApp::Instance()->RegisterEvent(kFFRefreshEvent, refresh_event);
  SDLApp::Instance()->RegisterEvent(SDL_KEYDOWN, key_event);

  return 0;
}

void FFmpegPlayer::Start() {
  demux_thread_.Start();
  video_decode_thread_.Start();
  audio_decode_thread_.Start();
  audio_play_.Start();

  ScheduleRefresh(player_ctx_.get(), 40);

  stop_ = false;
}

FFmpegPlayer::~FFmpegPlayer() { CleanUp(); }

// 不让析构函数直接调用 Stop()是因为，万一我想在 Stop()抛出异常呢？
void FFmpegPlayer::Stop() { CleanUp(); }

void FFmpegPlayer::Pause(PauseState pause_state) {
  player_ctx_->pause_ = pause_state;
  // reset frame_timer when retoring pause state
  player_ctx_->frame_timer_ = av_gettime() / 1000000.0;
}

void FFmpegPlayer::OnRefreshEvent(SDL_Event *event) {
  if (stop_) {
    return;
  }
  FFmpegPlayerContext *player_ctx{
      static_cast<FFmpegPlayerContext *>(event->user.data1)};

  if (player_ctx->video_stream_) {
    if (player_ctx->pict_queue_size_ == 0) {
      ScheduleRefresh(player_ctx, 1);
    } else {
      VideoPicture *video_picture{
          &(player_ctx->picture_queue_[player_ctx->pict_queue_rindex_])};
      double delay{video_picture->pts_ - player_ctx->frame_lase_pts_};

      if (delay <= 0 || delay >= 1.0) {
        delay = player_ctx->frame_last_delay_;
      }
      // save for next time
      player_ctx->frame_last_delay_ = delay;
      player_ctx->frame_lase_pts_ = video_picture->pts_;

      double ref_clock{GetAudioClock(player_ctx)};
      double diff{video_picture->pts_ - ref_clock};

      double sync_threshold{delay > kAVSyncThreshold ? delay
                                                     : kAVSyncThreshold};

      if (std::fabs(diff) < kAVNoSyncThreshold) {
        if (diff <= -sync_threshold) {
          delay = 0;
        } else if (diff >= sync_threshold) {
          delay = 2 * delay;
        }
      }
      player_ctx->frame_timer_ += delay;
      double actual_delay{player_ctx->frame_timer_ - av_gettime() / 1000000.0};

      if (actual_delay < 0.010) {
        actual_delay = 0.010;
      }
      ScheduleRefresh(player_ctx, static_cast<int>(actual_delay * 1000 + 0.5));

      VideoDisplay(player_ctx);

      if (++player_ctx->pict_queue_rindex_ == kVideoPictureQueueSize) {
        player_ctx->pict_queue_rindex_ = 0;
      }
      SDL_LockMutex(player_ctx->pict_queue_mutex_);
      player_ctx->pict_queue_size_--;
      SDL_CondSignal(player_ctx->pict_queue_cond_);
      SDL_UnlockMutex(player_ctx->pict_queue_mutex_);
    }
  } else {
    ScheduleRefresh(player_ctx, 100);
  }
}

void FFmpegPlayer::OnKeyEvent(SDL_Event *event) {
  double incr;
  double factor;

  switch (event->key.keysym.sym) {
  case SDLK_UP:
    factor = 1.1;
    DoVolumeAdjustOnKeyEvent(factor);
    break;
  case SDLK_DOWN:
    factor = 0.5;
    DoVolumeAdjustOnKeyEvent(factor);
    break;
  case SDLK_LEFT:
    incr = -10.0;
    DoSeekOnKeyEvent(incr);
    break;
  case SDLK_RIGHT:
    incr = 10.0;
    DoSeekOnKeyEvent(incr);
    break;
  case SDLK_q:
    // std::cout << "Quiting..." << '\n';
    Stop();
    SDLApp::Instance()->Quit();
    break;
  case SDLK_SPACE:
    // std::cout << "Pause" << '\n';

    if (player_ctx_->pause_ == PauseState::UNPAUSE_) {
      Pause(PauseState::PAUSE_);
    } else {
      Pause(PauseState::UNPAUSE_);
    }
    break;
  default:
    break;
  }
}

void FFmpegPlayer::DoSeekOnKeyEvent(double incr) {
  double pos{GetAudioClock(player_ctx_.get())};
  pos += incr;

  if (pos < 0) {
    pos = 0;
  }
  // std::cout << "Seek to " << pos << " v: " << GetAudioClock(&player_ctx_)
  // << " a: " << GetAudioClock(&player_ctx_) << '\n';
  StreamSeek(player_ctx_.get(), static_cast<int64_t>(pos * AV_TIME_BASE),
             static_cast<int>(incr));
}

void FFmpegPlayer::DoVolumeAdjustOnKeyEvent(double factor) {
  player_ctx_->factor_ *= factor;
}

void FFmpegPlayer::CleanUp() {
  if (!stop_) {
    // std::cout << "Player context cleaning..." << '\n';
    player_ctx_->Finit();
    // std::cout << "Player context finished." << '\n';

    stop_ = true;
  }
}

} // namespace myffmpegplayer