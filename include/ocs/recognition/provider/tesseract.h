/**
 * @file   tesseract.h
 * @author Dennis Sitelew
 * @date   Jul. 13, 2025
 */
#pragma once

#include <ocs/recognition/provider/provider.h>

#include <lyra/lyra.hpp>

#include <memory>

namespace ocs::recognition::provider {

class tesseract final : public provider {
private:
   class api;

public:
   struct config {
      explicit config(lyra::group &cli);

      std::string data_path{};
      std::string language{"eng+rus+deu"};

      bool selected{false};

      [[nodiscard]] bool validate() const;
   };

public:
   explicit tesseract(const config &cfg);
   ~tesseract() override;

public:
   result_t do_ocr(const ffmpeg::decoder::frame &frame) override;

private:
   std::unique_ptr<api> api_;
};

} // namespace ocs::recognition::provider