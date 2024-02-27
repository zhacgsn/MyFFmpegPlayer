#ifndef MYFFMPEGPLAYER_THREADBASE_H
#define MYFFMPEGPLAYER_THREADBASE_H

#include <atomic>
#include <thread>

namespace myffmpegplayer {

class ThreadBase {
public:
  ThreadBase() = default;

  virtual ~ThreadBase();

  ThreadBase(ThreadBase const &) = delete;
  ThreadBase &operator=(ThreadBase const &) = delete;

  virtual void Run() = 0;

  void Start();

private:
  std::unique_ptr<std::thread> thread_{nullptr};

protected:
  std::atomic<bool> stop_{false};
};

} // namespace myffmpegplayer

#endif