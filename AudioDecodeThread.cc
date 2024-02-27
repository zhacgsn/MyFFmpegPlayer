#include "AudioDecodeThread.h"
#include <sys/_types/_int16_t.h>
#include <sys/_types/_int32_t.h>
#include <sys/_types/_int64_t.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/packet.h"
#include "libavutil/avutil.h"
#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/rational.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
}

#include "FFmpegPlayer.h"
#include <_types/_uint8_t.h>
#include <algorithm>
#include <iostream>

namespace myffmpegplayer {

AudioDecodeThread::AudioDecodeThread(
    std::shared_ptr<FFmpegPlayerContext> player_ctx)
    : player_ctx_{player_ctx} {}

void AudioDecodeThread::GetAudioData(uint8_t *stream, int len) {
  if (!player_ctx_->audio_codec_ctx_ ||
      player_ctx_->pause_ == PauseState::PAUSE_) {
    std::fill_n(stream, len, 0);
    // std::cout << "Decoder not ready or in pause state!" << '\n';

    return;
  }

  while (len > 0) {
    if (player_ctx_->audio_buf_index_ >= player_ctx_->audio_buf_size_) {
      double pts;
      int audio_size{AudioDecodeFrame(&pts)};

      // 解码出错，缓冲区清空
      if (audio_size < 0) {
        player_ctx_->audio_buf_size_ = 1024;
        std::fill_n(player_ctx_->audio_buffer_, player_ctx_->audio_buf_size_,
                    0);
      } else {
        player_ctx_->audio_buf_size_ = audio_size;
      }
      player_ctx_->audio_buf_index_ = 0;
    }
    int len1 = player_ctx_->audio_buf_size_ - player_ctx_->audio_buf_index_;

    if (len1 > len) {
      len1 = len;
    }
    // 填充 stream
    std::copy_n(player_ctx_->audio_buffer_ + player_ctx_->audio_buf_index_,
                len1, stream);
    len -= len1;
    stream += len1;
    player_ctx_->audio_buf_index_ += len1;
  }
}

void AudioDecodeThread::Run() {
  // Does nothing
}

auto AudioDecodeThread::AudioDecodeFrame(double *pts_ptr) -> int {
  AVPacket *pkt = player_ctx_->audio_pkt_;
  int ret{0};

  while (true) {
    while (player_ctx_->audio_pkt_size_ > 0) {
      ret = avcodec_send_packet(player_ctx_->audio_codec_ctx_, pkt);

      // Error, skip frame
      if (ret != 0) {
        player_ctx_->audio_pkt_size_ = 0;
        break;
      }
      av_frame_unref(player_ctx_->audio_frame_);
      ret = avcodec_receive_frame(player_ctx_->audio_codec_ctx_,
                                  player_ctx_->audio_frame_);
      // Error, skip frame
      if (ret != 0) {
        player_ctx_->audio_pkt_size_ = 0;
        break;
      }

      // // 调整音量
      // int bytes_per_sample{av_get_bytes_per_sample(
      //     static_cast<AVSampleFormat>(player_ctx_->audio_frame_->format))};

      // for (int i = 0; i < player_ctx_->audio_frame_->nb_samples; i++) {
      //   for (int ch = 0; ch <
      //   player_ctx_->audio_frame_->ch_layout.nb_channels;
      //        ch++) {
      //     if (bytes_per_sample == 2) {
      //       int16_t *ptr{reinterpret_cast<int16_t *>(
      //           player_ctx_->audio_frame_->data[ch] + i * bytes_per_sample)};
      //       *ptr = av_clip_int16_c(*ptr * player_ctx_->factor_);
      //     } else if (bytes_per_sample == 4) {
      //       int32_t *ptr{reinterpret_cast<int32_t *>(
      //           player_ctx_->audio_frame_->data[ch] + i * bytes_per_sample)};
      //       std::cout << "*ptr before muti: " << *ptr << '\n';
      //       *ptr = *ptr / 2;

      //       // *ptr = av_clipl_int32_c(
      //       //     static_cast<int64_t>(*ptr * player_ctx->factor_));
      //       // std::cout << "*ptr after muti: " << *ptr << '\n';
      //     }
      //   }
      // }

      int data_size{};

      if (ret == 0) {
        int upper_bound_samples{swr_get_out_samples(
            player_ctx_->swr_ctx_, player_ctx_->audio_frame_->nb_samples)};
        uint8_t *out[4]{};
        out[0] = static_cast<uint8_t *>(av_malloc(upper_bound_samples * 2 * 2));

        // 重采样音频数据（采样格式转换）
        // number of samples output per channel
        int samples{swr_convert(
            player_ctx_->swr_ctx_, out, upper_bound_samples,
            const_cast<uint8_t const **>(player_ctx_->audio_frame_->data),
            player_ctx_->audio_frame_->nb_samples)};
        if (samples > 0) {
          // 转换后数据复制到音频数据缓冲区
          std::copy_n(out[0], samples * 2 * 2, player_ctx_->audio_buffer_);
        }
        av_free(out[0]);
        data_size = samples * 2 * 2;
      }
      int len1{pkt->size};
      // player_ctx->audio_pkt_data_ += len1;
      player_ctx_->audio_pkt_size_ -= len1;

      // No data yet, need more frames
      if (data_size <= 0) {
        continue;
      }
      // 更新 pts
      double pts{player_ctx_->audio_clock_};
      *pts_ptr = pts;
      int n{2 * player_ctx_->audio_codec_ctx_->ch_layout.nb_channels};
      // 补充该帧所占时间
      player_ctx_->audio_clock_ +=
          static_cast<double>(data_size) /
          static_cast<double>(n * (player_ctx_->audio_codec_ctx_->sample_rate));

      return data_size;
    }

    if (stop_) {
      // std::cout << "Request quit while decoding audio!" << '\n';

      return -1;
    }

    // 清空解码器缓冲区，比如在发生 seek后
    if (player_ctx_->flush_audio_ctx_) {
      player_ctx_->flush_audio_ctx_ = false;
      // std::cout << "avcodec_flush_buffers for seeking!" << '\n';
      avcodec_flush_buffers(player_ctx_->audio_codec_ctx_);

      continue;
    }

    // 获取新的 packet
    av_packet_unref(pkt);

    if (player_ctx_->audio_queue_.PacketGet(*pkt, stop_) < 0) {
      return -1;
    }
    // player_ctx->audio_pkt_data_ = pkt->data;
    player_ctx_->audio_pkt_size_ = pkt->size;

    if (pkt->pts != AV_NOPTS_VALUE) {
      // 把音频时钟设置为 packet的 pts，转换为秒为单位
      player_ctx_->audio_clock_ =
          av_q2d(player_ctx_->audio_stream_->time_base) * pkt->pts;
    }
  }
}

} // namespace myffmpegplayer