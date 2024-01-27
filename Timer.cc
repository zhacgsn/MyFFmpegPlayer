#include "Timer.h"
#include <_types/_uint32_t.h>
#include <iostream>

namespace myffmpegplayer {

static uint32_t TimerCallback(uint32_t interval, void *param) {
  // std::cout << "In CallbackFunc" << '\n';
  SDL_Event event;
  SDL_UserEvent user_event;

  user_event.type = SDL_USEREVENT;
  user_event.code = 0;
  // RenderView::OnRefresh()
  user_event.data1 = param;
  user_event.data2 = nullptr;

  event.type = SDL_USEREVENT;
  event.user = user_event;

  SDL_PushEvent(&event);

  // 定时器继续
  return interval;
}

void Timer::Start(void *cb, int interval) {
  if (timer_id_ != 0) {
    return;
  }
  SDL_TimerID timer_id = SDL_AddTimer(interval, TimerCallback, cb);

  if (timer_id == 0) {
    return;
  }
  timer_id_ = timer_id;
}

void Timer::Stop() {
  if (timer_id_ != 0) {
    SDL_RemoveTimer(timer_id_);
    timer_id_ = 0;
  }
}
} // namespace myffmpegplayer