#ifndef MYFFMPEGPLAYER_SDLAPP_H
#define MYFFMPEGPLAYER_SDLAPP_H

#include <functional>
#include <map>

#include <SDL.h>

namespace myffmpegplayer {

int const kSDLAppEventTimeout{1};

// #define SDLAppInstance (SDLApp::Instance());

class SDLApp {
public:
  SDLApp();

  int Exec();

  void Quit();

  void RegisterEvent(int type, std::function<void(SDL_Event *)> const &cb);

  static SDLApp *Instance();

private:
  std::map<int, std::function<void(SDL_Event *)>> user_events_;
};

} // namespace myffmpegplayer
#endif