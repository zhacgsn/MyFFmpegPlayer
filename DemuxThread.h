#ifndef MYFFMPEGPLAYER_DEMUXTHREAD_H
#define MYFFMPEGPLAYER_DEMUXTHREAD_H

#include "FFmpegPlayer.h"
#include "ThreadBase.h"

namespace myffmpegplayer {

class DemuxThread : public ThreadBase {
public:
  DemuxThread();

  void SetPlayerCtx(FFmpegPlayerContext *player_ctx);

  int InitDemuxThread();

  void FinitDemuxThread();

  void Run();

private:
  int DecodeLoop();

  int AudioDecodeFrame(FFmpegPlayerContext *player_ctx, double *pts_ptr);

  int StreamOpen(FFmpegPlayerContext *player_ctx, int media_type);

  FFmpegPlayerContext *player_ctx_{nullptr};
};

} // namespace myffmpegplayer

#endif