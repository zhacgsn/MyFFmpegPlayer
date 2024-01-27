#include "FFmpegPlayer.h"
#include "RenderView.h"
#include "SDLApp.h"
#include "Timer.h"
#include <_types/_uint8_t.h>
#include <functional>

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
  // 大坑
  // 为什么用 auto推导类型时 on_refresh_callback_test这里stack buffer overflow
  auto on_refresh_callback_test =
      std::bind(&myffmpegplayer::RenderView::OnRefresh, &view);
  std::cout << "on_refresh_callback_test type: "
            << typeid(on_refresh_callback_test).name() << '\n';

  auto on_refresh = [&view]() { view.OnRefresh(); };

  std::function<void()> on_refresh_callback =
      std::bind(&myffmpegplayer::RenderView::OnRefresh, &view);
  timer.Start(&on_refresh_callback, 30);

  RenderPairData *cb_data{new RenderPairData};
  cb_data->view = &view;

  myffmpegplayer::FFmpegPlayer player;
  player.SetFilePath(argv[1]);
  player.SetImageCallback(FNDecodeImageCallback, cb_data);

  if (player.InitPlayer() != 0) {
    std::cerr << "InitPlayer Failed!" << '\n';
    return -1;
  }
  std::cout << "FFmpegPlayer initialization successed." << '\n';

  player.Start();

  return sdl_app.Exec();
}