#ifndef MYFFMPEGPLAYER_PACKETQUEUE_H
#define MYFFMPEGPLAYER_PACKETQUEUE_H

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>

extern "C" {
#include "libavcodec/packet.h"
}

namespace myffmpegplayer {

class PacketQueue {
public:
  PacketQueue() = default;

  PacketQueue(PacketQueue const &) = delete;
  PacketQueue &operator=(PacketQueue const &) = delete;

  void PacketPut(AVPacket const &pkt);

  auto PacketGet(AVPacket &pkt, std::atomic<bool> &quit) -> int;

  void PacketFlush();

  auto PacketSize() -> int const;

private:
  std::list<AVPacket> pkts_;
  // 所有 AVPacket size之和
  std::atomic<int> size_{0};
  std::mutex mutex_;
  std::condition_variable cond_;
};

} // namespace myffmpegplayer

#endif