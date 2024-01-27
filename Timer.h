#ifndef MYFFMPEGPLAYER_TIMER_H
#define MYFFMPEGPLAYER_TIMER_H

#include <SDL.h>

namespace myffmpegplayer {

class Timer {
public:
  Timer() = default;

  void Start(void *cb, int interval);

  void Stop();

private:
  SDL_TimerID timer_id_{0};
};

} // namespace myffmpegplayer
#endif