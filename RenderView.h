#ifndef MYFFMPEGPLAYER_RENDERVIEW_H
#define MYFFMPEGPLAYER_RENDERVIEW_H

#include <SDL.h>
#include <list>
#include <mutex>

namespace myffmpegplayer {

struct RenderItem {
  SDL_Texture *texture_;
  SDL_Rect src_rect_;
  SDL_Rect dst_rect_;
};

class RenderView {
public:
  RenderView() = default;

  void SetNativeHandle(void *handle);

  void InitSDL();

  auto CreateRGB24Texture(int width, int height) -> RenderItem *;

  void UpdateTexture(RenderItem *item, unsigned char *pixel_data, int rows);

  void OnRefresh();

private:
  SDL_Window *sdl_window_{nullptr};
  SDL_Renderer *sdl_renderer_{nullptr};
  void *native_handle_{nullptr};
  std::list<RenderItem *> items_;
  std::mutex update_mutex_;
};

} // namespace myffmpegplayer
#endif