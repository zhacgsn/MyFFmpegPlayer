#include "ThreadBase.h"

#include <iostream>
#include <memory>
#include <thread>

namespace myffmpegplayer {

ThreadBase::~ThreadBase() {
  stop_ = true;

  if (thread_ && thread_->joinable()) {
    thread_->join();
  }
}

void ThreadBase::Start() {
  stop_ = false;

  if (thread_) {
    // std::cout << "Thread is already runnning" << '\n';
    return;
  }
  thread_ = std::make_unique<std::thread>([this]() -> void { Run(); });
}

} // namespace myffmpegplayer