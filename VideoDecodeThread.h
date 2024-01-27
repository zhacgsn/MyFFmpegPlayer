#ifndef MYFFMPEGPLAYER_VIDEODECODETHREAD_H
#define MYFFMPEGPLAYER_VIDEODECODETHREAD_H

#include "FFmpegPlayer.h"
#include "ThreadBase.h"

namespace myffmpegplayer {

class VideoDecodeThread : public ThreadBase {
public:
  VideoDecodeThread();

  void SetPlayerCtx(FFmpegPlayerContext *player_ctx);

  void Run();

private:
  int VideoEntry();

  int QueuePicture(FFmpegPlayerContext *player_ctx, AVFrame *p_frame,
                   double pts);

  FFmpegPlayerContext *player_ctx_{nullptr};
};

} // namespace myffmpegplayer

#endif