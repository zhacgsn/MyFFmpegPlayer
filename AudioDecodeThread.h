#ifndef MYFFMPEGPLAYER_AUDIODECODETHREAD_H
#define MYFFMPEGPLAYER_AUDIODECODETHREAD_H

#include "ThreadBase.h"
#include <_types/_uint8_t.h>
#include <memory>

namespace myffmpegplayer {

struct FFmpegPlayerContext;

class AudioDecodeThread : public ThreadBase {
public:
  explicit AudioDecodeThread(std::shared_ptr<FFmpegPlayerContext> player_ctx);

  ~AudioDecodeThread() = default;

  // void SetPlayerCtx(FFmpegPlayerContext *player_ctx);

  void GetAudioData(uint8_t *stream, int len);

  void Run() override;

private:
  auto AudioDecodeFrame(double *pts_ptr) -> int;

  // FFmpegPlayerContext *player_ctx_;
  std::shared_ptr<FFmpegPlayerContext> player_ctx_;
};

} // namespace myffmpegplayer

#endif