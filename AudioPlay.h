#ifndef MYFFMPEGPLAYER_AUDIOPLAY_H
#define MYFFMPEGPLAYER_AUDIOPLAY_H

#include "SDL_audio.h"
#include <SDL.h>

namespace myffmpegplayer {

class AudioPlay {
public:
  AudioPlay() = default;

  ~AudioPlay();

  AudioPlay(AudioPlay const &) = delete;
  AudioPlay &operator=(AudioPlay const &) = delete;

  auto OpenDevice(SDL_AudioSpec const *spec) -> int;

  void Start();

private:
  SDL_AudioDeviceID dev_id_{0};
};

} // namespace myffmpegplayer
#endif