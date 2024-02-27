#ifndef MYFFMPEGPLAYER_DEMUXTHREAD_H
#define MYFFMPEGPLAYER_DEMUXTHREAD_H

#include "ThreadBase.h"

namespace myffmpegplayer {

struct FFmpegPlayerContext;
class DemuxThread : public ThreadBase {
public:
  explicit DemuxThread(std::shared_ptr<FFmpegPlayerContext> player_ctx);

  ~DemuxThread() = default;

  auto InitDemuxThread() -> int;

  void FinitDemuxThread();

  void Run();

private:
  auto DecodeLoop() -> int;

  auto AudioDecodeFrame(double *pts_ptr) -> int;

  auto StreamOpen(int media_type) -> int;

  std::shared_ptr<FFmpegPlayerContext> player_ctx_;
};

} // namespace myffmpegplayer

#endif