//
// Created by Dennis Sitelew on 20.12.22.
//

#include <ocs/bmp.h>
#include <cstdio>

#include <array>

using namespace std;

namespace {

constexpr std::size_t file_header_size = 14;
using file_header_t = std::array<std::uint8_t, file_header_size>;

constexpr std::size_t info_header_size = 40;
using info_header_t = std::array<std::uint8_t, info_header_size>;

file_header_t make_file_header(std::uint32_t height, std::uint32_t stride) {
   const auto file_size = file_header_size + info_header_size + (stride * height);

   file_header_t result{};
   result[0] = 'B';
   result[1] = 'M';
   result[2] = static_cast<std::uint8_t>(file_size);
   result[3] = static_cast<std::uint8_t>(file_size >> 8);
   result[4] = static_cast<std::uint8_t>(file_size >> 16);
   result[5] = static_cast<std::uint8_t>(file_size >> 24);
   result[10] = static_cast<std::uint8_t>(file_header_size + info_header_size);
   return result;
}

info_header_t make_info_header(std::uint32_t width, std::uint32_t height) {
   info_header_t result{};
   result[0] = static_cast<std::uint8_t>(info_header_size);
   result[4] = static_cast<std::uint8_t>(width);
   result[5] = static_cast<std::uint8_t>(width >> 8);
   result[6] = static_cast<std::uint8_t>(width >> 16);
   result[7] = static_cast<std::uint8_t>(width >> 24);
   result[8] = static_cast<std::uint8_t>(height);
   result[9] = static_cast<std::uint8_t>(height >> 8);
   result[10] = static_cast<std::uint8_t>(height >> 16);
   result[11] = static_cast<std::uint8_t>(height >> 24);
   result[12] = 1;
   result[14] = 24;
   return result;
}

} // namespace

void ocs::bmp::save_image(std::vector<std::uint8_t> &data,
                          std::uint32_t width,
                          std::uint32_t height,
                          std::string_view filename,
                          bool flip_rb) {
   constexpr std::uint16_t bytes_per_pixel = 3;
   const std::size_t width_in_bytes = width * bytes_per_pixel;

   const unsigned char padding[3] = {0, 0, 0};
   const std::size_t padding_size = (4 - (width_in_bytes) % 4) % 4;

   const std::size_t stride = width_in_bytes + padding_size;

   FILE *image_file = fopen(filename.data(), "wb");

   const auto file_header = make_file_header(height, stride);
   fwrite(file_header.data(), 1, file_header.size(), image_file);

   const auto info_header = make_info_header(width, height);
   fwrite(info_header.data(), 1, info_header.size(), image_file);

   auto *data_ptr = data.data();

   if (flip_rb) {
      for (std::size_t i = 0; i < data.size(); i += 3) {
         std::swap(data_ptr[i], data_ptr[i + 2]);
      }
   }

   for (std::int64_t i = height - 1; i >= 0; i--) {
      fwrite(data_ptr + (i * width_in_bytes), bytes_per_pixel, width, image_file);
      fwrite(padding, 1, padding_size, image_file);
   }

   fclose(image_file);
}