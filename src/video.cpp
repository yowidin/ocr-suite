//
// Created by Dennis Sitelew on 18.12.22.
//

#include <ocs/util.h>
#include <ocs/video.h>

#include <spdlog/spdlog.h>

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

#include <cstdarg>

namespace ffmpeg {

////////////////////////////////////////////////////////////////////////////////
/// FFMPEG traits
////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct resource_trait {
   using type = T;

   static T *allocate();
   static void deallocate(T *ptr);
   static std::string name();
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
      if (!ptr_) {
         throw std::runtime_error("Failed to allocate " + trait::name());
      }
   }
   ~resource() { trait::deallocate(ptr_); }

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

   void reset() {
      trait::deallocate(ptr_);
      ptr_ = trait::allocate();
   }

public:
   T *get() { return ptr_; }
   T *operator->() { return ptr_; }
   T &operator*() { return *ptr_; }
   operator T *() { return ptr_; }
   operator T **() { return &ptr_; }

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
std::vector<std::string> get_hw_decoders() {
   std::vector<std::string> hw_decoders;
   AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;

   while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
      const char *name = av_hwdevice_get_type_name(type);
      if (name) {
         hw_decoders.emplace_back(name);
      }
   }

   return hw_decoders;
}

AVHWDeviceType get_default_hw_decoder_type() {
   std::vector<std::string> hw_decoders = get_hw_decoders();
   if (hw_decoders.empty()) {
      throw std::runtime_error("No HW decoders found");
   }

   auto result = av_hwdevice_find_type_by_name(hw_decoders[0].c_str());
   if (result == AV_HWDEVICE_TYPE_NONE) {
      throw std::runtime_error("Failed to find HW decoder type");
   }

   return result;
}

// Convert the FFMPEG logging level to the spdlog logging level
spdlog::level::level_enum ffmpeg_level_to_spdlog_level(int level) {
   switch (level) {
      case AV_LOG_QUIET:
         return spdlog::level::off;
      case AV_LOG_PANIC:
      case AV_LOG_FATAL:
         return spdlog::level::critical;
      case AV_LOG_ERROR:
         return spdlog::level::err;
      case AV_LOG_WARNING:
         return spdlog::level::warn;
      case AV_LOG_INFO:
         return spdlog::level::info;
      case AV_LOG_VERBOSE:
      case AV_LOG_DEBUG:
         return spdlog::level::debug;
      case AV_LOG_TRACE:
         return spdlog::level::trace;
      default:
         return spdlog::level::off;
   }
}

void log_callback(void *avcl, int level, const char *fmt, va_list vl) {
   const auto selected_level = av_log_get_level();
   if (level > selected_level) {
      return;
   }

   const AVClass *avc = avcl ? *(AVClass **)avcl : nullptr;
   const spdlog::level::level_enum lvl = ffmpeg_level_to_spdlog_level(level);

   std::va_list args_copy;
   va_copy(args_copy, vl);

   const auto required = std::vsnprintf(nullptr, 0, fmt, args_copy) + 1;
   va_end(args_copy);

   std::string msg(required, ' ');
   std::vsnprintf(msg.data(), msg.size(), fmt, vl);
   msg.resize(msg.size() - 1); // Remove the trailing null character
   ocs::trim(msg);

   if (avc) {
      spdlog::log(lvl, "[{}] {}", avc->class_name, msg);
   } else {
      spdlog::log(lvl, "{}", msg);
   }
}

} // namespace ffmpeg

using namespace ocs;

//! Helper class for handling the ffmpeg video stream using the hardware
//! acceleration.
class video::ffmpeg_video_stream {
public:
   ffmpeg_video_stream(const options &opts, std::int64_t starting_frame, video &video)
      : filename_(opts.video_file)
      , filter_(static_cast<ocs::video::frame_filter>(opts.frame_filter))
      , video_{&video} {
      av_log_set_level(AV_LOG_ERROR);
      av_log_set_callback(ffmpeg::log_callback);

      /* open the input file */
      if (avformat_open_input(input_ctx_, filename_.c_str(), nullptr, nullptr) != 0) {
         throw std::runtime_error("Failed to open input file: " + filename_);
      }

      if (avformat_find_stream_info(input_ctx_, nullptr) < 0) {
         throw std::runtime_error("Failed to find stream information");
      }

      /* find the video stream information */
      const AVCodec *decoder{nullptr};
      int ret = av_find_best_stream(input_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
      if (ret < 0) {
         throw std::runtime_error("Failed to find a video stream in the input file");
      }
      video_stream_idx_ = ret;

      const AVStream *video_stream = input_ctx_->streams[video_stream_idx_];
      if (avcodec_parameters_to_context(decoder_ctx_, video_stream->codecpar) < 0) {
         throw std::runtime_error("Failed to copy codec parameters to decoder context");
      }

      try {
         hw_device_type_ = ffmpeg::get_default_hw_decoder_type();
         hw_pix_fmt_ = find_pixel_format_for_decoder(decoder, hw_device_type_);
         decoder_ctx_->get_format = ffmpeg_video_stream::get_hw_format;
         hw_decoder_init();
      } catch (const std::runtime_error &ex) {
         SPDLOG_ERROR("Failed to initialize HW decoder: {}", ex.what());
         SPDLOG_INFO("Falling back to the software decoder");
         hw_pix_fmt_ = AV_PIX_FMT_NONE;
         decoder_ctx_.reset();
         if (avcodec_parameters_to_context(decoder_ctx_, video_stream->codecpar) < 0) {
            throw;
         }
      }

      if (avcodec_open2(decoder_ctx_, decoder, nullptr) < 0) {
         throw std::runtime_error("Failed to open codec for stream #" + std::to_string(video_stream_idx_));
      }

      // Seek to the starting frame
      frame_ratio_ = av_q2d(video_stream->avg_frame_rate);
      time_ratio_ = av_q2d(video_stream->time_base);

      seek_to_closest_frame(0, starting_frame, 0);

      auto frame_count =
          static_cast<std::int64_t>((static_cast<double>(input_ctx_->duration) / AV_TIME_BASE) * frame_ratio_);
      if (frame_count > 0) {
         frame_count_ = frame_count;
      }
   }

   ~ffmpeg_video_stream() {
      if (hw_device_ctx_) {
         av_buffer_unref(&hw_device_ctx_);
      }

      avformat_close_input(input_ctx_);
   }

public:
   void start() {
      bool can_run = true;
      while (can_run) {
         int ret = av_read_frame(input_ctx_, packet_);
         if (ret < 0) {
            break;
         }

         if (video_stream_idx_ == packet_->stream_index) {
            can_run = decode_write(packet_.get());
         }

         av_packet_unref(packet_);
      }

      // flush the decoder
      decode_write(nullptr);

      // We are done decoding
      video_->queue_->shutdown();
   }

   [[nodiscard]] std::optional<std::int64_t> frame_count() const { return frame_count_; }

   [[nodiscard]] std::int64_t frame_number_to_timestamp(std::int64_t frame_number) const {
      const auto time_in_seconds = static_cast<double>(frame_number) / frame_ratio_;
      return static_cast<std::int64_t>(time_in_seconds / time_ratio_);
   }

   [[nodiscard]] std::int64_t frame_number_to_seconds(std::int64_t frame_number) const {
      return static_cast<std::int64_t>(static_cast<double>(frame_number) / frame_ratio_);
   }

private:
   static AVPixelFormat find_pixel_format_for_decoder(const AVCodec *decoder, AVHWDeviceType type) {
      for (int i = 0;; ++i) {
         const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
         if (!config) {
            throw std::runtime_error("Decoder does not support device type");
         }

         if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type) {
            return config->pix_fmt;
         }
      }
   }

   static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
      for (const AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
         if (*p == hw_pix_fmt_) {
            return *p;
         }
      }

      SPDLOG_ERROR("Failed to get HW surface format");
      return AV_PIX_FMT_NONE;
   }

   void hw_decoder_init() {
      if (av_hwdevice_ctx_create(&hw_device_ctx_, hw_device_type_, nullptr, nullptr, 0) < 0) {
         throw std::runtime_error("Failed to create specified HW device");
      }

      decoder_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
   }

   //! @return true if the decoding can continue, false otherwise
   bool decode_write(AVPacket *packet) {
      int ret = avcodec_send_packet(decoder_ctx_, packet);
      if (ret < 0) {
         SPDLOG_ERROR("Error sending a packet for decoding: {}", ret);
         return false;
      }

      ffmpeg::frame rgb_frame;

      while (true) {
         ffmpeg::frame frame, sw_frame;

         ret = avcodec_receive_frame(decoder_ctx_, frame);
         if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
         } else if (ret < 0) {
            SPDLOG_ERROR("Error during decoding: {}", ret);
            return false;
         }

         const auto frame_mask = static_cast<int>(picture_type_to_filter(frame->pict_type));
         const auto filter_mask = static_cast<int>(filter_);
         if ((frame_mask & filter_mask) == 0) {
            // Skip this frame
            continue;
         }

         AVFrame *tmp_frame;
         if (frame->format == hw_pix_fmt_) {
            // retrieve data from GPU to CPU
            if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) {
               SPDLOG_ERROR("Error transferring the data to system memory");
               return false;
            }
            tmp_frame = sw_frame.get();
         } else {
            tmp_frame = frame.get();
         }

         auto opt_frame = video_->queue_->get_producer_value();
         if (!opt_frame.has_value()) {
            // The queue is closed, we are done
            return false;
         }

         auto &frame_data = opt_frame.value();
         auto frame_number = static_cast<std::int64_t>(static_cast<double>(frame->pts) * time_ratio_ * frame_ratio_);

         frame_data->frame_number = frame_number;
         frame_data->width = tmp_frame->width;
         frame_data->height = tmp_frame->height;
         convert_to_rgb_and_copy(*tmp_frame, *rgb_frame, frame_data->data);
         frame_data->bytes_per_line = rgb_frame->linesize[0];

         video_->queue_->add_consumer_value(frame_data);
      }
   }

   static video::frame_filter picture_type_to_filter(AVPictureType type) {
      switch (type) {
         case AV_PICTURE_TYPE_I:
            return video::frame_filter::I;
         case AV_PICTURE_TYPE_P:
            return video::frame_filter::P;
         case AV_PICTURE_TYPE_B:
            return video::frame_filter::B;
         default:
            // Fallback to the I-frame filter for unknown types
            return video::frame_filter::I;
      }
   }

   void convert_to_rgb_and_copy(const AVFrame &src, AVFrame &dst, std::vector<std::uint8_t> &target) {
      dst.format = AV_PIX_FMT_RGB24;
      dst.width = src.width;
      dst.height = src.height;

      int ret = av_image_alloc(dst.data, dst.linesize, src.width, src.height, (AVPixelFormat)dst.format, 1);
      if (ret < 0) {
         throw std::runtime_error("Could not allocate destination image");
      }

      // Convert the image from its native format to RGB
      sws_context_ = sws_getCachedContext(sws_context_, src.width, src.height, (AVPixelFormat)src.format, dst.width,
                                          dst.height, (AVPixelFormat)dst.format, 0, nullptr, nullptr, nullptr);
      sws_scale(sws_context_, (const uint8_t *const *)src.data, src.linesize, 0, src.height, dst.data, dst.linesize);

      auto size = av_image_get_buffer_size(static_cast<AVPixelFormat>(dst.format), dst.width, dst.height, 1);
      target.resize(size);

      ret = av_image_copy_to_buffer(target.data(), size, (const uint8_t *const *)dst.data, (const int *)dst.linesize,
                                    static_cast<AVPixelFormat>(dst.format), dst.width, dst.height, 1);
      if (ret < 0) {
         throw std::runtime_error("Could not copy image to buffer");
      }

      av_freep(&dst.data);
   }

   //! Recursively try to find the closest seekable frame to the `max_frame`
   void seek_to_closest_frame(std::int64_t min_frame, std::int64_t max_frame, std::int64_t last_working) {
      if (max_frame == 0) {
         seek_to_frame(last_working);
         return;
      }

      if (seek_to_frame(max_frame)) {
         return;
      }

      auto middle_frame = min_frame + (max_frame - min_frame) / 2;
      if (middle_frame == min_frame || middle_frame == max_frame) {
         // We went too deep, no more divisions are possible
         seek_to_frame(last_working);
         return;
      }

      if (seek_to_frame(middle_frame)) {
         seek_to_closest_frame(middle_frame, max_frame, middle_frame);
      } else {
         seek_to_closest_frame(min_frame, middle_frame, last_working);
      }
   }

   bool seek_to_frame(std::int64_t frame_number) {
      const auto starting_time = frame_number_to_timestamp(frame_number);
      int res = av_seek_frame(input_ctx_, video_stream_idx_, starting_time, 0);
      return res >= 0;
   }

private:
   std::string filename_;
   video::frame_filter filter_;
   video *video_;

   ffmpeg::format_context input_ctx_;
   ffmpeg::codec_context decoder_ctx_;

   int video_stream_idx_ = -1;

   AVBufferRef *hw_device_ctx_{nullptr};
   AVHWDeviceType hw_device_type_{AV_HWDEVICE_TYPE_NONE};
   static AVPixelFormat hw_pix_fmt_;

   ffmpeg::packet packet_;
   SwsContext *sws_context_{nullptr};

   double frame_ratio_{0.0};
   double time_ratio_{0.0};
   std::optional<std::int64_t> frame_count_{std::nullopt};
};

AVPixelFormat video::ffmpeg_video_stream::hw_pix_fmt_ = AV_PIX_FMT_NONE;

////////////////////////////////////////////////////////////////////////////////
/// Class: video
////////////////////////////////////////////////////////////////////////////////
video::video(const options &opts, queue_ptr_t queue, std::int64_t starting_frame)
   : queue_{std::move(queue)}
   , stream_(std::make_unique<ffmpeg_video_stream>(opts, starting_frame, *this)) {
   // Load the video file.
}

video::~video() = default;

void video::start() {
   stream_->start();
}

std::optional<std::int64_t> video::frame_count() const {
   return stream_->frame_count();
}

[[nodiscard]] std::int64_t video::frame_number_to_seconds(std::int64_t num) const {
   return stream_->frame_number_to_seconds(num);
}
