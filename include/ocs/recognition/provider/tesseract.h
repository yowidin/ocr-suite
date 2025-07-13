/**
 * @file   tesseract.h
 * @author Dennis Sitelew
 * @date   Jul. 13, 2025
 */
#pragma once

#include <ocs/recognition/provider/provider.h>

#include <memory>

namespace ocs::recognition::provider {

class tesseract final : public provider {
private:
   class api;

public:
   tesseract(const char *tess_data_path, const char *language);
   ~tesseract();

public:
   result_t do_ocr(const ffmpeg::decoder::frame &frame) override;

private:
   std::unique_ptr<api> api_;
};

} // namespace ocs::recognition::provider