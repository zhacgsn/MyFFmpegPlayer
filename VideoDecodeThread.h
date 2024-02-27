#ifndef MYFFMPEGPLAYER_VIDEODECODETHREAD_H
#define MYFFMPEGPLAYER_VIDEODECODETHREAD_H

extern "C" {
#include "libavutil/frame.h"
}
#include "ThreadBase.h"

namespace myffmpegplayer {

struct FFmpegPlayerContext;
class VideoDecodeThread : public ThreadBase {
public:
  explicit VideoDecodeThread(std::shared_ptr<FFmpegPlayerContext> player_ctx);

  ~VideoDecodeThread() = default;

  void Run() override;

private:
  auto VideoEntry() -> int;

  auto QueuePicture(AVFrame *p_frame, double pts) -> int;

  std::shared_ptr<FFmpegPlayerContext> player_ctx_;
};

} // namespace myffmpegplayer

#endif