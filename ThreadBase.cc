#include "ThreadBase.h"
#include <iostream>
#include <memory>
#include <thread>

namespace myffmpegplayer {

// void ThreadBase::Start() {
//   if (IsRunning()) {
//     std::cout << "Thread is already runnning" << '\n';
//     return;
//   }
//   stop_ = false;

//   thread_ = std::make_unique<std::thread>(&ThreadBase::ThreadEntry, this);
// }

// void ThreadBase::Stop() {
//   stop_ = true;

//   if (IsRunning()) {
//     thread_->join();
//     // 置为空指针
//     thread_.reset();
//   }
// }

// bool ThreadBase::IsRunning() const { return thread_ && thread_->joinable(); }

void ThreadBase::Start() {
  stop_ = false;
  if (th_) {
    std::cout << "Thread is already runnning" << '\n';
    return;
  }

  th_ = new std::thread(ThreadEntry, this);
}

void ThreadBase::Stop() {
  stop_ = true;

  if (th_){
    th_->join();
  }
}

} // namespace myffmpegplayer