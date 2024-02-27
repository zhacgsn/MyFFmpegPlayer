#include "PacketQueue.h"

extern "C" {
#include "libavcodec/packet.h"
}

#include <condition_variable>
#include <iostream>
#include <list>
#include <mutex>

namespace myffmpegplayer {

void PacketQueue::PacketPut(AVPacket const &pkt) {
  std::scoped_lock<std::mutex> lck{mutex_};

  pkts_.emplace_back(pkt);
  size_ += pkts_.size();

  cond_.notify_one();
}

auto PacketQueue::PacketGet(AVPacket &pkt, std::atomic<bool> &quit) -> int {
  int ret{0};

  while (true) {
    std::unique_lock<std::mutex> lck{mutex_};
    cond_.wait(lck, [this]() { return !pkts_.empty(); });
    pkt = pkts_.front();
    size_ -= pkt.size;

    pkts_.erase(pkts_.begin());
    lck.unlock();

    ret = 1;
    break;

    if (quit) {
      ret = -1;
      break;
    }
  }

  return ret;
}

void PacketQueue::PacketFlush() {
  std::scoped_lock<std::mutex> lck{mutex_};

  for (auto &pkt : pkts_) {
    av_packet_unref(&pkt);
  }
  pkts_.clear();
  size_ = 0;
}

auto PacketQueue::PacketSize() -> int const { return size_; }

} // namespace myffmpegplayer