//
// Created by Dennis Sitelew on 12.03.23.
//

#pragma once

// We are including all ffmpeg-related suff here, which might be unused in the header itself, but will be used in
// the implementation
// ReSharper disable CppUnusedIncludeDirective
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
// ReSharper restore CppUnusedIncludeDirective

#include <string>
#include <vector>
#include <stdexcept>

namespace ocs::ffmpeg::traits {

////////////////////////////////////////////////////////////////////////////////
/// FFMPEG traits
////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct resource_trait {
   using type = T;

   static T *allocate() { return nullptr; }
   static void deallocate(T *ptr) { (void)ptr; }
   static std::string name() { return ""; }
};

template <>
struct resource_trait<AVPacket> {
   using type = AVPacket;

   static AVPacket *allocate() { return av_packet_alloc(); }
   static void deallocate(AVPacket *ptr) { av_packet_free(&ptr); }
   static std::string name() { return "AVPacket"; }
};

template <>
struct resource_trait<AVFrame> {
   using type = AVFrame;

   static AVFrame *allocate() { return av_frame_alloc(); }
   static void deallocate(AVFrame *ptr) { av_frame_free(&ptr); }
   static std::string name() { return "AVFrame"; }
};

template <>
struct resource_trait<AVFormatContext> {
   using type = AVFormatContext;

   static AVFormatContext *allocate() { return avformat_alloc_context(); }
   static void deallocate(AVFormatContext *ptr) { avformat_free_context(ptr); }
   static std::string name() { return "AVFormatContext"; }
};

template <>
struct resource_trait<AVCodecContext> {
   using type = AVCodecContext;

   static AVCodecContext *allocate() { return avcodec_alloc_context3(nullptr); }
   static void deallocate(AVCodecContext *ptr) { avcodec_free_context(&ptr); }
   static std::string name() { return "AVCodecContext"; }
};

////////////////////////////////////////////////////////////////////////////////
/// Class: ffmpeg_resource
////////////////////////////////////////////////////////////////////////////////
template <typename T>
class resource {
   using trait = resource_trait<T>;

public:
   resource()
      : ptr_(trait::allocate()) {
      if (ptr_ == nullptr) {
         throw std::runtime_error("Failed to allocate " + trait::name());
      }
   }
   ~resource() { trait::deallocate(ptr_); }

public:
   resource(const resource &) = delete;
   resource &operator=(const resource &) = delete;

   resource(resource &&other) noexcept
      : ptr_(other.ptr_) {
      other.ptr_ = nullptr;
   }

   resource &operator=(resource &&other) noexcept {
      if (this != &other) {
         ptr_ = other.ptr_;
         other.ptr_ = nullptr;
      }
      return *this;
   }

public:
   void reset() {
      trait::deallocate(ptr_);
      ptr_ = trait::allocate();
   }

public:
   T *get() { return ptr_; }
   T *operator->() { return ptr_; }
   T &operator*() { return *ptr_; }

   // We are intentionally want to allow implicit convertion here, it's a resource wrapper after all
   // ReSharper disable CppNonExplicitConversionOperator
   operator T *() { return ptr_; } // NOLINT(*-explicit-constructor)
   operator T **() { return &ptr_; } // NOLINT(*-explicit-constructor)
   // ReSharper restore CppNonExplicitConversionOperator

private:
   T *ptr_;
};

////////////////////////////////////////////////////////////////////////////////
/// FFMPEG resources
////////////////////////////////////////////////////////////////////////////////
using packet = resource<AVPacket>;
using frame = resource<AVFrame>;
using format_context = resource<AVFormatContext>;
using codec_context = resource<AVCodecContext>;

////////////////////////////////////////////////////////////////////////////////
/// Utility functions
////////////////////////////////////////////////////////////////////////////////
std::vector<std::string> get_hw_decoders();

AVHWDeviceType get_default_hw_decoder_type();

AVPixelFormat find_pixel_format_for_decoder(const AVCodec *decoder, AVHWDeviceType type);

struct log_setup {
   log_setup();
   ~log_setup();
};

} // namespace ocs::ffmpeg::traits
