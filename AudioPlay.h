#ifndef MYFFMPEGPLAYER_AUDIOPLAY_H
#define MYFFMPEGPLAYER_AUDIOPLAY_H

#include "SDL_audio.h"
#include <SDL.h>

namespace myffmpegplayer {

class AudioPlay {
public:
  AudioPlay() = default;

  int OpenDevice(SDL_AudioSpec const *spec);

  void Start();

  void Stop();

private:
  SDL_AudioDeviceID dev_id_{0};
};

} // namespace myffmpegplayer
#endif