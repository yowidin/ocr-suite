/**
 * @file   provider.h
 * @author Dennis Sitelew
 * @date   Jul. 13, 2025
 */
#pragma once

#include <ocs/common/ocr_result.h>
#include <ocs/ffmpeg/decoder.h>

#include <optional>

namespace ocs::recognition::provider {

class provider {
public:
   using result_t = std::optional<common::ocr_result>;

public:
   virtual ~provider() = default;

public:
   virtual result_t do_ocr(const ffmpeg::decoder::frame &frame) = 0;

protected:
   int min_letters_threshold_{3};
};

} // namespace ocs::recognition::provider