#include "AudioPlay.h"
#include "SDL_audio.h"

namespace myffmpegplayer {

int AudioPlay::OpenDevice(const SDL_AudioSpec *spec) {
  // spec: 音频参数，代表期望的输出格式
  dev_id_ = SDL_OpenAudioDevice(nullptr, 0, spec, nullptr, 0);
  return dev_id_;
}

void AudioPlay::Start() {
  // call audio callback function
  SDL_PauseAudioDevice(dev_id_, 0);
}

void AudioPlay::Stop() { SDL_PauseAudioDevice(dev_id_, 1); }

} // namespace myffmpegplayer