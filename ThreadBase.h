#ifndef MYFFMPEGPLAYER_THREADBASE_H
#define MYFFMPEGPLAYER_THREADBASE_H

#include <atomic>
#include <memory>
#include <thread>

namespace myffmpegplayer {

class ThreadBase {
public:
  ThreadBase() = default;

  virtual ~ThreadBase() { Stop(); }

  virtual void Run() = 0;

  void Start();

  void Stop();

  // bool IsRunning() const;

private:
  std::unique_ptr<std::thread> thread_{nullptr};
  std::thread *th_{nullptr};

  // static void ThreadEntry(ThreadBase *thread_base) { thread_base->Run(); }
  static void ThreadEntry(void *arg) {
    ThreadBase *th = (ThreadBase*)arg;
    th->Run();
  }

protected:
  std::atomic<bool> stop_{false};
};

} // namespace myffmpegplayer

#endif