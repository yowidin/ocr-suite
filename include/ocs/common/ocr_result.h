//
// Created by Dennis Sitelew on 17.06.2023.
//

#ifndef OCS_COMMON_OCR_RESULT_H
#define OCS_COMMON_OCR_RESULT_H

#include <string>
#include <vector>

namespace ocs::common {

struct text_entry {
   int left, top, right, bottom;
   float confidence;
   std::string text;
};

struct ocr_result {
   std::int64_t frame_number{};
   std::vector<text_entry> entries{};
};

} // namespace ocs::common

#endif // OCR_SUITE_OCR_RESULT_H
