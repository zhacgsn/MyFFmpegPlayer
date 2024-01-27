#include "RenderView.h"
#include "SDL_hints.h"
#include "SDL_pixels.h"
#include "SDL_render.h"
#include "SDL_video.h"
#include <cstring>
#include <iostream>
#include <list>
#include <mutex>
#include <stdexcept>

namespace myffmpegplayer {

int const kSDLWindowDefaultWidth{1280};
int const kSDLWindowDefaultHeight{720};

static SDL_Rect MakeRect(int x, int y, int w, int h) {
  std::cout << "In MakeRecr" << '\n';

  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;

  std::cout << "rect.x: " << rect.x << " rect.y: " << rect.y
            << " rect.w: " << rect.w << " rect.h: " << rect.h << '\n';

  return rect;
}

void RenderView::SetNativeHandle(void *handle) { native_handle_ = handle; }

void RenderView::InitSDL() {
  if (native_handle_) {
    sdl_window_ = SDL_CreateWindowFrom(native_handle_);
  } else {
    sdl_window_ = SDL_CreateWindow(
        "MyFFmpegPlayer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kSDLWindowDefaultWidth, kSDLWindowDefaultHeight, SDL_WINDOW_RESIZABLE);
  }
  if (!sdl_window_) {
    std::cerr << "!sdl_window in " << __FUNCTION__ << '\n';
    throw std::runtime_error("!RenderView::sdl_window_");
  }
  sdl_renderer_ = SDL_CreateRenderer(sdl_window_, -1, SDL_RENDERER_ACCELERATED);

  if (!sdl_renderer_) {
    std::cerr << "!sdl_renderer_ in " << __FUNCTION__ << '\n';
    throw std::runtime_error("!RenderView::sdl_renderer_");
  }
  SDL_RenderSetLogicalSize(sdl_renderer_, kSDLWindowDefaultWidth,
                           kSDLWindowDefaultHeight);

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
}

RenderItem *RenderView::CreateRGB24Texture(int width, int height) {
  std::scoped_lock<std::mutex> mutex{update_mutex_};

  RenderItem *ret{new RenderItem};
  SDL_Texture *texture{SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_RGB24,
                                         SDL_TEXTUREACCESS_STREAMING, width,
                                         height)};
  ret->texture_ = texture;
  ret->src_rect_ = MakeRect(0, 0, width, height);
  ret->dst_rect_ =
      MakeRect(0, 0, kSDLWindowDefaultWidth, kSDLWindowDefaultHeight);

  items_.emplace_back(ret);

  return ret;
}

void RenderView::UpdateTexture(RenderItem *item, unsigned char *pixel_data,
                               int rows) {
  std::scoped_lock<std::mutex> mutex{update_mutex_};

  void *pixels{nullptr};
  int pitch;
  SDL_LockTexture(item->texture_, nullptr, &pixels, &pitch);
  memcpy(pixels, pixel_data, pitch * rows);
  SDL_UnlockTexture(item->texture_);

  SDL_RenderClear(sdl_renderer_);

  for (auto it : items_) {
    SDL_RenderCopy(sdl_renderer_, it->texture_, &it->src_rect_, &it->dst_rect_);
  }
}

void RenderView::OnRefresh() {
  std::scoped_lock<std::mutex> mutex{update_mutex_};

  if (sdl_renderer_) {
    // update the screen
    SDL_RenderPresent(sdl_renderer_);
  }
}

} // namespace myffmpegplayer