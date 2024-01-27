#ifndef MYFFMPEGPLAYER_AUDIODECODETHREAD_H
#define MYFFMPEGPLAYER_AUDIODECODETHREAD_H

#include "ThreadBase.h"
#include <_types/_uint8_t.h>

namespace myffmpegplayer {

struct FFmpegPlayerContext;

class AudioDecodeThread : public ThreadBase {
public:
  AudioDecodeThread();

  void SetPlayerCtx(FFmpegPlayerContext *player_ctx);

  void GetAudioData(uint8_t *stream, int len);

  void Run() override;

private:
  int AudioDecodeFrame(FFmpegPlayerContext *player_ctx, double *pts_ptr);

  FFmpegPlayerContext *player_ctx_{nullptr};
};

} // namespace myffmpegplayer

#endif