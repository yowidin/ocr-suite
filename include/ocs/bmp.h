//
// Created by Dennis Sitelew on 20.12.22.
//

#ifndef OCR_SUITE_BMP_H
#define OCR_SUITE_BMP_H

#include <cstdint>
#include <string_view>
#include <vector>

namespace ocs::bmp {

void save_image(std::vector<std::uint8_t> &data,
                std::uint32_t width,
                std::uint32_t height,
                std::string_view filename,
                bool flip_rb = true);

} // namespace ocs::bmp

#endif // OCR_SUITE_BMP_H
