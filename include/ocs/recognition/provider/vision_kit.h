/**
 * @file   tesseract.h
 * @author Dennis Sitelew
 * @date   Aug. 17, 2025
 */
#pragma once

#include <ocs/recognition/provider/provider.h>

#include <future>
#include <lyra/lyra.hpp>

namespace ocs::recognition::provider {

class vision_kit final : public provider {
public:
   struct config {
      explicit config(lyra::group &cli);

      std::string languages{"en-US+ru-RU+de-DE"};

      bool selected{false};

      [[nodiscard]] bool validate();
   };

public:
   explicit vision_kit(config cfg);
   ~vision_kit() override;

public:
   result_t do_ocr(const ffmpeg::decoder::frame &frame) override;

private:
   static void handle_ocr_results(std::uint32_t frame_number,
                                  std::uint32_t count,
                                  char **texts,
                                  std::uint32_t *lefts,
                                  std::uint32_t *tops,
                                  std::uint32_t *rights,
                                  std::uint32_t *bottoms,
                                  float *confidences,
                                  char *error,
                                  void *user_data);

private:
   config config_;

   std::promise<result_t> promise_;
};

} // namespace ocs::recognition::provider