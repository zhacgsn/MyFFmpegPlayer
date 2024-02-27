#include "AudioPlay.h"
#include "SDL_audio.h"

namespace myffmpegplayer {

AudioPlay::~AudioPlay() { SDL_PauseAudioDevice(dev_id_, 1); }

auto AudioPlay::OpenDevice(const SDL_AudioSpec *spec) -> int {
  // spec: 音频参数，代表期望的输出格式
  dev_id_ = SDL_OpenAudioDevice(nullptr, 0, spec, nullptr, 0);
  return dev_id_;
}

void AudioPlay::Start() {
  // call audio callback function
  SDL_PauseAudioDevice(dev_id_, 0);
}

} // namespace myffmpegplayer