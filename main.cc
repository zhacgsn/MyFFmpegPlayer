#include <_types/_uint8_t.h>
#include <functional>
#include <memory>

#include "FFmpegPlayer.h"
#include "RenderView.h"
#include "SDLApp.h"
#include "Timer.h"

struct RenderPairData {
  myffmpegplayer::RenderItem *item{nullptr};
  myffmpegplayer::RenderView *view{nullptr};
};

static void FNDecodeImageCallback(uint8_t *data, int w, int h, void *userdata) {
  RenderPairData *cb_data{static_cast<RenderPairData *>(userdata)};

  if (!cb_data->item) {
    cb_data->item = cb_data->view->CreateRGB24Texture(w, h);
  }
  cb_data->view->UpdateTexture(cb_data->item, data, h);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Usage: ./MyFFmpegPlayer media_file_path" << '\n';
    return -1;
  }
  myffmpegplayer::SDLApp sdl_app;

  // Render video
  myffmpegplayer::RenderView view;
  view.InitSDL();

  myffmpegplayer::Timer timer;

  std::function<void()> on_refresh_callback = [&view]() { view.OnRefresh(); };
  timer.Start(&on_refresh_callback, 30);

  std::unique_ptr<RenderPairData> cb_data = std::make_unique<RenderPairData>();
  cb_data->view = &view;

  myffmpegplayer::FFmpegPlayer player;
  player.SetFilePath(argv[1]);
  player.SetImageCallback(FNDecodeImageCallback, cb_data.get());

  if (player.InitPlayer() != 0) {
    // std::cerr << "InitPlayer Failed!" << '\n';
    return -1;
  }
  // std::cout << "FFmpegPlayer initialization successed." << '\n';

  player.Start();

  return sdl_app.Exec();
}