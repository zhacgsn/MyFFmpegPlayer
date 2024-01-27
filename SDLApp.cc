#include "SDLApp.h"

#include <functional>
#include <iostream>

#include "SDL.h"
#include "SDL_events.h"

namespace myffmpegplayer {

static SDLApp *GlobalInstance{nullptr};

SDLApp::SDLApp() {
  SDL_Init(SDL_INIT_EVERYTHING);

  if (!GlobalInstance) {
    GlobalInstance = this;
  } else {
    std::cerr << "Only one instance allowed" << '\n';
    exit(1);
  }
}

int SDLApp::Exec() {
  SDL_Event event;

  while (true) {
    SDL_WaitEventTimeout(&event, kSDLAppEventTimeout);

    switch (event.type) {
    case SDL_QUIT:
      SDL_Quit();
      return 0;
    case SDL_USEREVENT: {
      auto cb = *static_cast<std::function<void()> *>(event.user.data1);
      cb();
    } break;
    default:
      auto iter = user_events_.find(event.type);
      if (iter != user_events_.end()) {
        auto on_event_cb = iter->second;
        on_event_cb(&event);
      }
      break;
    }
  }
}

void SDLApp::Quit() {
  SDL_Event event;
  event.type = SDL_QUIT;
  SDL_PushEvent(&event);
}

void SDLApp::RegisterEvent(int type,
                           const std::function<void(SDL_Event *)> &cb) {
  user_events_[type] = cb;
}

SDLApp *SDLApp::Instance() { return GlobalInstance; }

} // namespace myffmpegplayer