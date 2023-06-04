//
// Created by Dennis Sitelew on 20.12.22.
//

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace ocs::recognition::bmp {

void save_image(std::vector<std::uint8_t> &data,
                std::uint32_t width,
                std::uint32_t height,
                std::string_view filename,
                bool flip_rb = true);

} // namespace ocs::recognition::bmp
