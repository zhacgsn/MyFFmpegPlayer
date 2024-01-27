#include "PacketQueue.h"
#include "SDL_mutex.h"
#include "libavcodec/packet.h"
#include <iostream>
#include <iterator>
#include <list>

namespace myffmpegplayer {

PacketQueue::PacketQueue()
    : mutex_{SDL_CreateMutex()}, cond_(SDL_CreateCond()) {}

void PacketQueue::PacketPut(AVPacket *pkt) {
  SDL_LockMutex(mutex_);

  pkts_.emplace_back(*pkt);
  size_ += pkts_.size();

  SDL_CondSignal(cond_);
  SDL_UnlockMutex(mutex_);
}

int PacketQueue::PacketGet(AVPacket *pkt, std::atomic<bool> &quit) {
  int ret{0};

  SDL_LockMutex(mutex_);

  while (true) {
    if (!pkts_.empty()) {
      AVPacket &first_pkt{pkts_.front()};

      size_ -= first_pkt.size;
      *pkt = first_pkt;

      pkts_.erase(pkts_.begin());

      ret = 1;
      break;
    } else {
      SDL_CondWaitTimeout(cond_, mutex_, 500);
    }

    if (quit) {
      ret = -1;
      break;
    }
  }
  SDL_UnlockMutex(mutex_);

  return ret;
}

void PacketQueue::PacketFlush() {
  SDL_LockMutex(mutex_);

  for (auto &pkt : pkts_) {
    av_packet_unref(&pkt);
  }
  pkts_.clear();
  size_ = 0;

  SDL_UnlockMutex(mutex_);
}

int PacketQueue::PacketSize() const { return size_; }

} // namespace myffmpegplayer