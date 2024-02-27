#ifndef MYFFMPEGPLAYER_FFMPEGPLAYER_H
#define MYFFMPEGPLAYER_FFMPEGPLAYER_H

#include "PacketQueue.h"
#include "SDL_audio.h"
#include "SDL_events.h"
#include "SDL_mutex.h"
#include <_types/_uint32_t.h>
#include <_types/_uint8_t.h>
#include <sys/_types/_int64_t.h>

extern "C" {
#include "libavcodec/packet.h"
#include "libavutil/frame.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "AudioDecodeThread.h"
#include "AudioPlay.h"
#include "DemuxThread.h"
#include "VideoDecodeThread.h"
#include <SDL.h>
#include <atomic>
#include <functional>
#include <iostream>
#include <string>

namespace myffmpegplayer {

size_t constexpr kMaxAudioFrameSize{192000};

size_t constexpr kVideoPictureQueueSize{1};

// SDL_USEREVENT到 SDL_LASTEVENT用于自定义事件
int constexpr kFFBaseEvent{SDL_USEREVENT + 100};
int constexpr kFFRefreshEvent{kFFBaseEvent + 20};
int constexpr kFFQuitEvent{kFFBaseEvent + 30};

size_t constexpr kMaxAudioQSize{5 * 16 * 1024};
size_t constexpr kMaxVideoQSize{5 * 256 * 1024};

float constexpr kAVSyncThreshold{0.01};
float constexpr kAVNoSyncThreshold{10.0};

size_t constexpr kSDLAudioBufferSize{1024};

// typedef void (*Image_Cb)(unsigned char* data, int w, int h, void* userdata)

struct VideoPicture {
  AVFrame *frame_{nullptr};
  double pts_{0.0};
};

enum class PauseState { UNPAUSE_ = 0, PAUSE_ = 1 };

using ImageCallback =
    std::function<void(uint8_t *data, int w, int h, void *userdata)>;

struct FFmpegPlayerContext {
  FFmpegPlayerContext();

  ~FFmpegPlayerContext();

  void Finit();

  AVFormatContext *format_ctx_{nullptr};

  AVCodecContext *audio_codec_ctx_{nullptr};
  AVCodecContext *video_codec_ctx_{nullptr};

  int audio_stream_index_{-1};
  int video_stream_index_{-1};

  AVStream *audio_stream_{nullptr};
  AVStream *video_stream_{nullptr};

  PacketQueue audio_queue_;
  PacketQueue video_queue_;

  uint8_t audio_buffer_[(kMaxAudioFrameSize * 3) / 2];
  size_t audio_buf_size_{0};
  uint32_t audio_buf_index_{0};
  AVFrame *audio_frame_{nullptr};
  AVPacket *audio_pkt_{nullptr};
  // uint8_t *audio_pkt_data_{nullptr};
  size_t audio_pkt_size_{0};

  // 视频跳转 flag
  std::atomic<int> seek_req_;
  int seek_flags_;
  int64_t seek_pos_;

  // 音量调整 flag
  double factor_{1.0};

  // Flush flag to set after seeking
  std::atomic<bool> flush_audio_ctx_{false};
  std::atomic<bool> flush_video_ctx_{false};

  double audio_clock_{0.0};
  double frame_timer_{0.0};
  double frame_lase_pts_{0.0};
  double frame_last_delay_{0.0};
  double video_clock_{0.0};

  VideoPicture picture_queue_[kVideoPictureQueueSize];
  std::atomic<int> pict_queue_size_{0};
  std::atomic<int> pict_queue_rindex_{0};
  std::atomic<int> pict_queue_windex_{0};
  SDL_mutex *pict_queue_mutex_{nullptr};
  SDL_cond *pict_queue_cond_{nullptr};

  std::string filename_;

  SwsContext *sws_ctx_{nullptr};
  SwrContext *swr_ctx_{nullptr};

  std::atomic<PauseState> pause_{PauseState::UNPAUSE_};

  std::atomic<bool> stopped_{false};

  ImageCallback image_callback_;
  void *cb_data_{nullptr};

private:
  void CleanUp();
};

class FFmpegPlayer {
public:
  FFmpegPlayer();

  ~FFmpegPlayer();

  FFmpegPlayer(FFmpegPlayer const &) = delete;

  FFmpegPlayer &operator=(FFmpegPlayer const &) = delete;

  void SetFilePath(char const *file_path);

  void SetImageCallback(ImageCallback image_callback, void *user_data);

  auto InitPlayer() -> int;

  void Start();

  void Stop();

  void Pause(PauseState pause_state);

  void OnRefreshEvent(SDL_Event *event);
  void OnKeyEvent(SDL_Event *event);

private:
  void DoSeekOnKeyEvent(double incr);

  void DoVolumeAdjustOnKeyEvent(double factor);

  void CleanUp();

  std::shared_ptr<FFmpegPlayerContext> player_ctx_;
  std::string file_path_;
  SDL_AudioSpec audio_desired_spec_;
  std::atomic<bool> stop_{false};

  DemuxThread demux_thread_;
  AudioDecodeThread audio_decode_thread_;
  VideoDecodeThread video_decode_thread_;
  AudioPlay audio_play_;
};

} // namespace myffmpegplayer

#endif