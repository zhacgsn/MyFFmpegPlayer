#ifndef MYFFMPEGPLAYER_PACKETQUEUE_H
#define MYFFMPEGPLAYER_PACKETQUEUE_H

#include "SDL_mutex.h"
#include <SDL.h>
#include <atomic>
#include <list>

extern "C" {
#include "libavcodec/packet.h"
#include <libavcodec/avcodec.h>
}

namespace myffmpegplayer {

class PacketQueue {
public:
  PacketQueue();

  void PacketPut(AVPacket *pkt);

  int PacketGet(AVPacket *pkt, std::atomic<bool> &quit);

  void PacketFlush();

  int PacketSize() const;

private:
  std::list<AVPacket> pkts_;
  // 所有 AVPacket size之和
  std::atomic<int> size_{0};
  SDL_mutex *mutex_;
  SDL_cond *cond_;
};

} // namespace myffmpegplayer

#endif