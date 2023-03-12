//
// Created by Dennis Sitelew on 12.03.23.
//

#include <ocs/ffmpeg/traits.h>
#include <ocs/util.h>

#include <spdlog/spdlog.h>

#include <cstdarg>

namespace {

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
   ocs::util::trim(msg);

   if (avc) {
      spdlog::log(lvl, "[{}] {}", avc->class_name, msg);
   } else {
      spdlog::log(lvl, "{}", msg);
   }
}

} // namespace

namespace ocs::ffmpeg::traits {

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

AVPixelFormat find_pixel_format_for_decoder(const AVCodec *decoder, AVHWDeviceType type) {
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

log_setup::log_setup() {
   av_log_set_level(AV_LOG_ERROR);
   av_log_set_callback(log_callback);
}

log_setup::~log_setup() {
   av_log_set_callback(av_log_default_callback);
}

} // namespace ocs::ffmpeg::traits